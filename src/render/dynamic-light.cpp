// aug 18 2026, 14 30
/* purpose
* Implements the DynamicLightManager singleton.
* Owns a fixed-size pool of dynamic point lights with lifetime management.
* Provides spawn, update (lifetime/fade), and submitToShader (distance-culled, sorted).
* Does NOT own shader compilation or uniform upload.
* Does NOT own config loading or hot-reload.
*/
#include "dynamic-light.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "debug/debug-log.h"

DynamicLightManager& DynamicLightManager::instance()
{
    static DynamicLightManager mgr;
    return mgr;
}

DynamicLight* DynamicLightManager::spawn(const glm::vec3& position,
    const glm::vec3& color, float intensity, float radius, float lifetime,
    float fadeIn, float fadeOut)
{
    for (auto& light : mLights) {
        if (light.active) continue;
        light.id = mNextId++;
        light.position = position;
        light.color = color;
        light.intensity = intensity;
        light.radius = radius;
        light.lifetime = lifetime;
        light.age = 0.0f;
        light.fadeIn = fadeIn;
        light.fadeOut = fadeOut;
        light.active = true;
        Debug::log(Debug::Category::Render,
            "[DYNAMIC LIGHT] spawned id=%u pos=(%.2f,%.2f,%.2f) radius=%.2f intensity=%.2f lifetime=%.3f\n",
            light.id, position.x, position.y, position.z, radius, intensity, lifetime);
        return &light;
    }
    Debug::logThrottled(Debug::Category::Render, "dlight-pool-full", 2.0f,
        "[DYNAMIC LIGHT] pool full (max=%d)\n", (int)MAX_LIGHTS);
    return nullptr;
}

void DynamicLightManager::update(float dt)
{
    for (auto& light : mLights) {
        if (!light.active) continue;
        light.age += dt;
        if (light.age >= light.lifetime) {
            light.active = false;
        }
    }
}

DynamicLightManager::SubmitResult DynamicLightManager::submitToShader(
    const glm::vec3& cameraPos, int maxCount) const
{
    SubmitResult result;
    result.count = 0;

    struct Candidate {
        int index;
        float dist;
    };
    Candidate candidates[MAX_LIGHTS];
    int candidateCount = 0;

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (!mLights[i].active) continue;
        const DynamicLight& light = mLights[i];

        float dist = glm::length(light.position - cameraPos);
        if (dist > light.radius + 20.0f) continue;

        float t = (light.lifetime > 0.0f) ? (light.age / light.lifetime) : 1.0f;
        float fadeMultiplier = 1.0f;
        if (light.fadeIn > 0.0f && light.age < light.fadeIn) {
            fadeMultiplier = light.age / light.fadeIn;
        }
        if (light.fadeOut > 0.0f && light.age > light.lifetime - light.fadeOut) {
            fadeMultiplier = (light.lifetime - light.age) / light.fadeOut;
        }
        fadeMultiplier = std::max(0.0f, std::min(1.0f, fadeMultiplier));

        float effectiveIntensity = light.intensity * fadeMultiplier;
        if (effectiveIntensity < 0.01f) continue;

        candidates[candidateCount++] = {i, dist};
    }

    std::sort(candidates, candidates + candidateCount,
        [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });

    int submitCount = std::min(candidateCount, maxCount);
    mSubmittedCount = submitCount;

    for (int i = 0; i < submitCount; ++i) {
        const DynamicLight& light = mLights[candidates[i].index];
        float fadeMultiplier = 1.0f;
        if (light.fadeIn > 0.0f && light.age < light.fadeIn) {
            fadeMultiplier = light.age / light.fadeIn;
        }
        if (light.fadeOut > 0.0f && light.age > light.lifetime - light.fadeOut) {
            fadeMultiplier = (light.lifetime - light.age) / light.fadeOut;
        }
        fadeMultiplier = std::max(0.0f, std::min(1.0f, fadeMultiplier));

        result.lights[i].position = light.position;
        result.lights[i].color = light.color;
        result.lights[i].radius = light.radius;
        result.lights[i].intensity = light.intensity * fadeMultiplier;
    }
    result.count = submitCount;

    return result;
}

void DynamicLightManager::clear()
{
    for (auto& light : mLights)
        light.active = false;
    mSubmittedCount = 0;
}

void DynamicLightManager::resetStats()
{
    mSubmittedCount = 0;
}

int DynamicLightManager::activeCount() const
{
    int count = 0;
    for (const auto& light : mLights)
        if (light.active) ++count;
    return count;
}
