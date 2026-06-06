#include "effect-part.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "camera.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include "audio/audio.h"

EffectPartSystem& EffectPartSystem::instance() {
    static EffectPartSystem sInstance;
    return sInstance;
}

void EffectPartSystem::init() {
    printf("[EFFECT PART] Initialized\n");
}

void EffectPartSystem::update(float dt) {
    for (auto& effect : mEffects) {
        effect.lifetime += dt;
        if (!effect.sticky)
            effect.position += effect.velocity * dt;
        if (effect.affectedByGravity)
            effect.velocity.z -= 4.0f * dt;
        if (effect.lifetime >= effect.maxLifetime) {
            effect.alive = false;
        }
    }
    
    mEffects.erase(
        std::remove_if(mEffects.begin(), mEffects.end(),
            [](const EffectPart& e) { return !e.alive; }),
        mEffects.end()
    );
}

EffectPart* EffectPartSystem::spawnDamage(glm::vec3 position, const std::string& victim, int damage) {
    EffectPart e;
    e.position = position;
    e.color = {1.0f, 0.0f, 0.0f};
    e.maxLifetime = 1.0f;
    e.label = victim + " took " + std::to_string(damage) + " damage!!";
    e.scale = 0.24f;
    return spawn(e);
}

void EffectPartSystem::spawnBlood(glm::vec3 position, glm::vec3 direction, float amount) {
    spawnStickyBlood(position, -direction, amount);
}

void EffectPartSystem::spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId) {
    bool highForce = force >= 0.55f;
    int bigCount = highForce ? 5 : 1;
    int smallCount = highForce ? 25 : 5;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                             : glm::cross(n, glm::vec3(0,1,0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    for (int i = 0; i < bigCount + smallCount; ++i) {
        bool big = i < bigCount;
        float angle = i * 2.399963f;
        float radius = big ? 0.08f * i : 0.12f + 0.025f * (i - bigCount);
        EffectPart e;
        e.position = position + tangent * std::cos(angle) * radius + bitangent * std::sin(angle) * radius + n * 0.003f;
        e.normal = n;
        e.color = {0.75f, 0.0f, 0.02f};
        e.maxLifetime = 30.0f;
        e.scale = big ? (0.18f + force * 0.14f) : (0.035f + force * 0.045f);
        e.endScale = e.scale;
        e.billboardText = false;
        e.sticky = true;
        e.flatDecal = true;
        e.ownerId = ownerId;
        e.debugVisual = true;
        spawn(e);
    }
}

EffectPart* EffectPartSystem::spawnWorldImpact(glm::vec3 position, glm::vec3 normal) {
    EffectPart e;
    e.position = position;
    e.normal = normal;
    e.color = {0.55f, 0.55f, 0.55f};
    e.maxLifetime = 0.5f;
    e.scale = 0.1f;
    e.endScale = 0.5f;
    e.alpha = 0.5f;
    e.billboardText = false;
    e.sticky = true;
    EffectPart* spawned = spawn(e);
    AudioManager::instance().play({"world_impact", AudioCategory::Impacts, true, position, 0.7f, 1.0f, 30.0f});
    return spawned;
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    mEffects.erase(std::remove_if(mEffects.begin(), mEffects.end(),
        [ownerId](const EffectPart& e) { return e.ownerId == ownerId; }), mEffects.end());
}

EffectPart* EffectPartSystem::spawn(const EffectPart& effect) {
    mEffects.push_back(effect);
    return &mEffects.back();
}

EffectPart* EffectPartSystem::spawnFootstep(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.color = {1.0f, 1.0f, 1.0f};
    e.maxLifetime = 0.5f;
    e.label = "walk()";
    e.scale = 0.15f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDash(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.color = {0.2f, 0.6f, 1.0f};
    e.maxLifetime = 0.8f;
    e.label = "dash()";
    e.scale = 0.25f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnFreeze(glm::vec3 position, float freezeDuration) {
    EffectPart e;
    e.position = position;
    e.color = {0.2f, 1.0f, 0.3f};
    e.maxLifetime = 0.1f;
    char buf[64];
    snprintf(buf, sizeof(buf), "freeze(%.2f)", freezeDuration);
    e.label = buf;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnImpact(glm::vec3 position, glm::vec3 color, const char* label) {
    EffectPart e;
    e.position = position;
    e.color = color;
    e.maxLifetime = 1.0f;
    e.label = label;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnCustom(glm::vec3 position, glm::vec3 color, float lifetime, const char* label) {
    EffectPart e;
    e.position = position;
    e.color = color;
    e.maxLifetime = lifetime;
    e.label = label;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

void EffectPartSystem::clear() {
    mEffects.clear();
}

void EffectPartSystem::render(const Camera& camera) const {
    if (mEffects.empty()) return;
    
    // Render spheres for each effect
    for (const auto& effect : mEffects) {
        if (!effect.alive) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;
        
        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        float alpha = effect.alpha * (1.0f - t);
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        // Draw sphere using debug visuals
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};
        if (effect.flatDecal) {
            glm::vec3 n = glm::length(effect.normal) > 0.001f ? glm::normalize(effect.normal) : glm::vec3(0,0,1);
            glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                                     : glm::cross(n, glm::vec3(0,1,0)));
            glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
            constexpr int SEGMENTS = 12;
            for (int i = 0; i < SEGMENTS; ++i) {
                float a0 = 6.2831853f * i / SEGMENTS;
                float a1 = 6.2831853f * (i + 1) / SEGMENTS;
                glm::vec3 p0 = effect.position + (tangent * std::cos(a0) + bitangent * std::sin(a0)) * drawScale;
                glm::vec3 p1 = effect.position + (tangent * std::cos(a1) + bitangent * std::sin(a1)) * drawScale;
                DebugVis::drawLine(camera, p0, p1, drawColor);
            }
        } else {
            DebugVis::drawWireSphere(camera, effect.position, drawScale, drawColor);
        }
        
        // Draw billboard text label
        if (effect.billboardText && !effect.label.empty()) {
            float x, y;
            if (DebugVis::projectToScreen(camera, effect.position + glm::vec3(0, 0, effect.scale + 0.15f), x, y)) {
                glm::vec4 textColor = {effect.color.x, effect.color.y, effect.color.z, alpha};
                uiDrawText(effect.label.c_str(), x, y, 0.3f * effect.scale, textColor);
            }
        }
    }
}
