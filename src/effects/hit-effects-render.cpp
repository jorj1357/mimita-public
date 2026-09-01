#include "effects/hit-effects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/glm.hpp>

#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "effects/effect-part.h"

static constexpr int MAX_BURSTS = 64;
extern HitBurstEffect gBursts[MAX_BURSTS];
extern int gBurstCount;
extern int gGlobalTick;
extern HitFxConfig gConfig;

static float evalCurve(const std::string& curve, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    if (curve == "ease_in") return t * t;
    if (curve == "ease_out") return 1.0f - (1.0f - t) * (1.0f - t);
    if (curve == "ease_in_out") return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    if (curve == "exponential") return t < 0.5f ? std::pow(2.0f, 10.0f * t - 10.0f) * 0.5f : (2.0f - std::pow(2.0f, -10.0f * t + 10.0f)) * 0.5f;
    if (curve == "linear") return t;
    return t;
}

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static glm::vec3 lerpVec3(const glm::vec3& a, const glm::vec3& b, float t)
{
    return glm::vec3(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t));
}

static void renderSphereTimeline(const HitBurstEffect& b, int age, const Camera& camera)
{
    for (const auto& kf : gConfig.sphereTimeline) {
        if (age < kf.startTick || age > kf.endTick) continue;
        int range = std::max(1, kf.endTick - kf.startTick);
        float t = (float)(age - kf.startTick) / (float)range;
        t = evalCurve(gConfig.curves.radiusCurve, t);
        float radius = lerp(kf.startRadius, kf.endRadius, t);
        float alpha = lerp(kf.alphaStart, kf.alphaEnd, evalCurve(gConfig.curves.alphaCurve, (float)(age - kf.startTick) / (float)range));
        float brightness = lerp(kf.brightnessStart, kf.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, (float)(age - kf.startTick) / (float)range));
        glm::vec3 color = lerpVec3(kf.colorStart, kf.colorEnd, (float)(age - kf.startTick) / (float)range);
        glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};
        DebugVis::drawFilledSphere(camera, b.position, radius, col);
    }
}

static void renderElongatedSphere(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = gConfig.elongatedSphere;
    if (!cfg.enabled || age < cfg.startTick || age > cfg.endTick) return;
    int range = std::max(1, cfg.endTick - cfg.startTick);
    float t = (float)(age - cfg.startTick) / (float)range;
    float len = lerp(cfg.lengthStart, cfg.lengthEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float rad = lerp(cfg.radiusStart, cfg.radiusEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, evalCurve(gConfig.curves.alphaCurve, t));
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, t));
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);

    if (cfg.shape == "shockwave") {
        const float shockRadius = std::max(0.0f, len);
        const float rotation = cfg.rotation.z + cfg.shockwaveRotationSpeedDegrees * (float)age / 60.0f;
        const glm::vec4 shockColor{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};
        DebugVis::drawFilledShockwave(camera, b.position, b.normal, shockRadius,
                                      cfg.shockwaveHeight, cfg.shockwavePoints,
                                      cfg.shockwavePointLength, rotation, shockColor);
        return;
    }
    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};

    glm::vec3 dir = glm::length(b.direction) > 0.001f ? glm::normalize(b.direction) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
    DebugVis::drawFilledSphere(camera, b.position, rad, col, scaleVec);
}

static void renderImpactDisc(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = gConfig.impactDisc;
    if (!cfg.enabled || age < cfg.startTick || age > cfg.endTick) return;
    int range = std::max(1, cfg.endTick - cfg.startTick);
    float t = (float)(age - cfg.startTick) / (float)range;
    float radius = lerp(cfg.radiusStart, cfg.radiusEnd, evalCurve(gConfig.curves.radiusCurve, t));
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, evalCurve(gConfig.curves.alphaCurve, t));
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, evalCurve(gConfig.curves.brightnessCurve, t));
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);
    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};
    glm::vec3 nrm = glm::length(b.normal) > 0.001f ? glm::normalize(b.normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    DebugVis::drawFilledDecal(camera, b.position, nrm, radius, col);
}

static const MovementDashBurstConfig& burstConfigForType(BurstType type) {
    switch (type) {
        case BurstType::GroundJump: return gConfig.groundJumpBurst;
        case BurstType::AirJump: return gConfig.airJumpBurst;
        case BurstType::Walk: return gConfig.walkBurst;
        case BurstType::Landing: return gConfig.landingBurst;
        case BurstType::HealthGained: return gConfig.healthGained;
        default: return gConfig.movementDashBurst;
    }
}

static void renderDirectionalBurst(const HitBurstEffect& b, int age, const Camera& camera)
{
    const auto& cfg = burstConfigForType(b.burstType);
    if (!cfg.enabled) return;
    if (age < 0 || age > cfg.lifetimeTicks) return;

    float t = cfg.lifetimeTicks > 0 ? (float)age / (float)cfg.lifetimeTicks : 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    float len = lerp(cfg.lengthStart, cfg.lengthEnd, t);
    float rad = lerp(cfg.radiusStart, cfg.radiusEnd, t);
    float alpha = lerp(cfg.alphaStart, cfg.alphaEnd, t);
    float brightness = lerp(cfg.brightnessStart, cfg.brightnessEnd, t);
    glm::vec3 color = lerpVec3(cfg.colorStart, cfg.colorEnd, t);

    if (cfg.speedScaling && b.dashSpeed > cfg.speedThreshold) {
        float scale = std::clamp(b.dashSpeed / cfg.speedThreshold, cfg.speedScaleMin, cfg.speedScaleMax);
        len *= scale;
    }

    glm::vec3 fwd = glm::length(b.direction) > 0.001f ? glm::normalize(b.direction) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::vec3 right = glm::normalize(glm::cross(fwd, up));
    if (glm::length(right) < 0.001f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    up = glm::normalize(glm::cross(right, fwd));

    glm::vec3 stretchDir;
    if (cfg.stretchAxis == "right")
        stretchDir = right;
    else if (cfg.stretchAxis == "up")
        stretchDir = up;
    else if (cfg.stretchAxis == "world_x")
        stretchDir = glm::vec3(1.0f, 0.0f, 0.0f);
    else if (cfg.stretchAxis == "world_y")
        stretchDir = glm::vec3(0.0f, 1.0f, 0.0f);
    else if (cfg.stretchAxis == "world_z")
        stretchDir = glm::vec3(0.0f, 0.0f, 1.0f);
    else
        stretchDir = fwd;

    if (glm::length(cfg.rotation) > 0.001f)
    {
        glm::vec3 radAngles = glm::radians(cfg.rotation);
        glm::quat rotQuat = glm::quat(radAngles);
        stretchDir = rotQuat * stretchDir;
    }

    glm::vec4 col{color.x * brightness, color.y * brightness, color.z * brightness, std::clamp(alpha, 0.0f, 1.0f)};

    glm::vec3 scaleVec = stretchDir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - stretchDir;
    scaleVec *= cfg.scale;
    DebugVis::drawFilledSphere(camera, b.position, rad, col, scaleVec);
}

void HitEffects::renderHitBursts(const Camera& camera)
{
    for (int i = 0; i < gBurstCount; ++i) {
        const HitBurstEffect& b = gBursts[i];
        if (!b.alive) continue;
        int age = gGlobalTick - b.spawnTick;

        if (b.dashBurst || b.burstType == BurstType::GroundJump || b.burstType == BurstType::AirJump
            || b.burstType == BurstType::Walk || b.burstType == BurstType::Landing
            || b.burstType == BurstType::HealthGained) {
            renderDirectionalBurst(b, age, camera);
        } else {
            renderSphereTimeline(b, age, camera);
            renderElongatedSphere(b, age, camera);
            renderImpactDisc(b, age, camera);
        }
    }
}
