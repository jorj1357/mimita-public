#include "effect-part.h"
#include "renderer/renderer.h"
#include "camera.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "config.h"
#include <algorithm>
#include <cstdio>
#include <glm/glm.hpp>
#include <cstring>

static float damageNumberOpacity(const DamageNumberConfig& cfg, float t, float distFade)
{
    const float fadeStart = std::clamp(cfg.fadeStart, 0.0f, 1.0f);
    float fadeEnd = std::clamp(cfg.fadeEnd, 0.0f, 1.0f);
    if (fadeEnd <= fadeStart)
        fadeEnd = fadeStart + 0.001f;
    float opacity = cfg.startOpacity;
    if (t >= fadeEnd) {
        opacity = cfg.endOpacity;
    } else if (t > fadeStart) {
        const float fadeT = (t - fadeStart) / (fadeEnd - fadeStart);
        opacity = cfg.startOpacity + (cfg.endOpacity - cfg.startOpacity) * fadeT;
    }
    return std::clamp(opacity, 0.0f, 1.0f) * distFade;
}

static void drawDamageNumberText(const char* text, float x, float y, float scale,
                                 glm::vec4 color, const DamageNumberConfig& cfg)
{
    const float italic = cfg.italic ? 0.18f : 0.0f;
    if (cfg.shadowEnabled) {
        glm::vec4 shadow{cfg.outlineColor.x, cfg.outlineColor.y, cfg.outlineColor.z, color.a * 0.65f};
        uiDrawText(text, x + cfg.shadowOffset.x, y + cfg.shadowOffset.y, scale, shadow, italic);
    }

    const float outline = std::max(0.0f, cfg.outlineThickness);
    if (cfg.outlineEnabled && outline > 0.0f) {
        glm::vec4 outlineColor{cfg.outlineColor.x, cfg.outlineColor.y, cfg.outlineColor.z, color.a};
        uiDrawText(text, x - outline, y, scale, outlineColor, italic);
        uiDrawText(text, x + outline, y, scale, outlineColor, italic);
        uiDrawText(text, x, y - outline, scale, outlineColor, italic);
        uiDrawText(text, x, y + outline, scale, outlineColor, italic);
        uiDrawText(text, x - outline, y - outline, scale, outlineColor, italic);
        uiDrawText(text, x + outline, y - outline, scale, outlineColor, italic);
        uiDrawText(text, x - outline, y + outline, scale, outlineColor, italic);
        uiDrawText(text, x + outline, y + outline, scale, outlineColor, italic);
    }

    if (cfg.bold)
        uiDrawText(text, x + std::max(1.0f, scale), y, scale, color, italic);
    uiDrawText(text, x, y, scale, color, italic);
}

void EffectPartSystem::render(const Camera& camera) const {
    for (const auto& effect : mPool) {
        if (!effect.alive) continue;
        if (effect.lifetime < 0.0f) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;

        float dist = glm::length(effect.position - camera.pos);
        if (dist > 40.0f) continue;
        float distFade = (dist > 20.0f) ? (40.0f - dist) / 20.0f : 1.0f;

        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        const bool damageNumber = effect.replayType == "damage_number";
        float alpha = effect.alpha * distFade;
        if (!damageNumber && (effect.replayType != "death_ellipsoid" || HitEffects::config().deathEllipsoid.fade))
            alpha *= (1.0f - t);
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};
        
        if (effect.beam) {
            DebugVis::drawFilledBeam(camera, effect.position, effect.endPosition, drawScale, drawColor);
        }
        else if (effect.box) {
            DebugVis::drawFilledBox(camera, effect.position, effect.halfSize, drawColor, effect.rotation);
        }
        else if (effect.flatDecal) {
            DebugVis::drawFilledDecal(camera, effect.position, effect.normal, drawScale, drawColor);
        }
        else if (effect.replayType == "death_ellipsoid") {
            glm::vec3 dir = glm::length(effect.endPosition - effect.position) > 0.001f
                ? glm::normalize(effect.endPosition - effect.position)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            float len = glm::length(effect.endPosition - effect.position);
            float rad = drawScale;
            glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
            DebugVis::drawFilledSphere(camera, effect.position, rad, drawColor, scaleVec);
        }
        else if (!effect.billboardText) {
            DebugVis::drawFilledSphere(camera, effect.position, drawScale, drawColor);
        }
        
        if (effect.billboardText && !effect.label.empty()) {
            float x = 0.0f, y = 0.0f;
            const glm::vec3 projectPos = damageNumber
                ? effect.position
                : effect.position + glm::vec3(0, 0, effect.scale + 0.15f);
            bool projected = DebugVis::projectToScreen(camera, projectPos, x, y);
            if (!projected) {
                Debug::logThrottled(Debug::Category::General, "popup-project-fail", 2.0f,
                    "[DAMAGE POPUP] projectToScreen failed label=%s pos=(%.2f %.2f %.2f) camDist=%.1f\n",
                    effect.label.c_str(), effect.position.x, effect.position.y, effect.position.z,
                    glm::length(effect.position - camera.pos));
            } else if (damageNumber) {
                const auto& cfg = HitEffects::config().damageNumber;
                const float textScale = std::max(0.01f,
                    cfg.fontSize * std::max(0.0f, cfg.startScale + (cfg.endScale - cfg.startScale) * t));
                const float textAlpha = damageNumberOpacity(cfg, t, distFade);
                const glm::vec3 color = (!effect.label.empty() && effect.label[0] == '+')
                    ? cfg.healingColor
                    : effect.color;
                glm::vec4 textColor{color.x, color.y, color.z, textAlpha};
                drawDamageNumberText(effect.label.c_str(),
                    x + cfg.screenOffsetX,
                    y + cfg.screenOffsetY,
                    textScale,
                    textColor,
                    cfg);
                Debug::logThrottled(Debug::Category::General, "popup-render", 1.0f,
                    "[DAMAGE POPUP] RENDER label=%s screen=(%.0f %.0f) alpha=%.2f fontSize=%.2f "
                    "worldPos=(%.2f %.2f %.2f) camDist=%.1f lifetime=%.2f/%.2f color=(%.2f %.2f %.2f)\n",
                    effect.label.c_str(), x + cfg.screenOffsetX, y + cfg.screenOffsetY,
                    textAlpha, textScale,
                    effect.position.x, effect.position.y, effect.position.z,
                    glm::length(effect.position - camera.pos),
                    effect.lifetime, effect.maxLifetime,
                    color.x, color.y, color.z);
            } else {
                // Apply upward drift: move text up over its lifetime
                float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
                float driftPx = (1.0f - t) * 60.0f;
                glm::vec4 textColor = {effect.color.x, effect.color.y, effect.color.z, alpha};
                float fontSize = 48.0f;
                uiDrawText(effect.label.c_str(), x, y - driftPx, fontSize, textColor);
                Debug::logThrottled(Debug::Category::General, "popup-render", 1.0f,
                    "[DAMAGE POPUP] RENDER label=%s screen=(%.0f %.0f) alpha=%.2f fontSize=%.0f "
                    "worldPos=(%.2f %.2f %.2f) camDist=%.1f lifetime=%.2f/%.2f drift=%.1f color=(%.2f %.2f %.2f)\n",
                    effect.label.c_str(), x, y - driftPx, alpha, fontSize,
                    effect.position.x, effect.position.y, effect.position.z,
                    glm::length(effect.position - camera.pos),
                    effect.lifetime, effect.maxLifetime, driftPx,
                    effect.color.x, effect.color.y, effect.color.z);
            }
        }
    }

    for (const BloodParticle& particle : mBloodParticles) {
        const float dist = glm::length(particle.position - camera.pos);
        if (dist > 40.0f)
            continue;
        const float distFade = dist > 20.0f ? (40.0f - dist) / 20.0f : 1.0f;
        DebugVis::drawFilledBillboard(
            camera,
            particle.position,
            particle.size,
            particle.rotation,
            particle.stretch,
            {0.92f, 0.015f, 0.025f, particle.alpha * distFade});
    }

    for (const BloodDecal& decal : mBloodDecals) {
        const float dist = glm::length(decal.position - camera.pos);
        if (dist > 60.0f)
            continue;
        const float distFade = dist > 40.0f ? (60.0f - dist) / 20.0f : 1.0f;
        DebugVis::drawBloodDecal(
            camera,
            decal.position,
            decal.normal,
            decal.radius,
            decal.rotation,
            decal.stretch,
            {0.78f, 0.01f, 0.02f, decal.alpha * distFade});
    }

    // Particle debug logging
    if (DebugConfig::DEBUG_BLOOD_HITS || DebugConfig::DEBUG_BLOOD_RAYS) {
        static float partTimer = 0.0f;
        partTimer -= 0.016f;
        if (partTimer <= 0.0f) {
            partTimer = 2.0f;
            int debrisCount = 0;
            for (const auto& e : mPool) {
                if (!e.alive) continue;
                if (e.replayType == "debris_block") ++debrisCount;
            }
            printf("[PARTICLE] debris=%d blood=%d decals=%zu transparentPass=1 depthWrite=0\n",
                   debrisCount, (int)mBloodParticles.size(), mBloodDecals.size());
        }
    }

    // Blood performance metrics
    if (DebugConfig::DEBUG_BLOOD_HITS) {
        static float bloodPerfTimer = 0.0f;
        bloodPerfTimer -= 0.016f;
        if (bloodPerfTimer <= 0.0f) {
            bloodPerfTimer = 2.0f;
            printf("[BLOOD PERF] particles=%zu decals=%zu collisionUpdates=0\n",
                   mBloodParticles.size(), mBloodDecals.size());
        }
    }

    if (DebugConfig::DEBUG_BLOOD_RAYS) {
        for (unsigned int i = 0; i < mBloodDebugSegmentCount; ++i) {
            const BloodDebugSegment& segment = mBloodDebugSegments[i];
            DebugVis::drawLine(
                camera,
                segment.from,
                segment.to,
                segment.hit
                    ? glm::vec4(1.0f, 0.25f, 0.05f, 1.0f)
                    : glm::vec4(0.8f, 0.02f, 0.04f, 0.85f));
        }
    }
    if (DebugConfig::DEBUG_BLOOD_HITS) {
        for (unsigned int i = 0; i < mBloodDebugSegmentCount; ++i) {
            const BloodDebugSegment& segment = mBloodDebugSegments[i];
            if (!segment.hit)
                continue;
            DebugVis::drawPointCross(camera, segment.to, 0.12f, {1.0f, 1.0f, 0.0f, 1.0f});
            DebugVis::drawLine(
                camera,
                segment.to,
                segment.to + segment.normal * 0.5f,
                {0.2f, 1.0f, 0.2f, 1.0f});
        }
    }

    if (DebugConfig::DEBUG_DEBRIS) {
        int debrisCount = 0;
        glm::vec3 centroid{0.0f};
        for (const auto& effect : mPool) {
            if (!effect.alive || !effect.box)
                continue;
            centroid += effect.position;
            ++debrisCount;
            float velLen = glm::length(effect.velocity);
            if (velLen > 0.01f) {
                DebugVis::drawLine(
                    camera,
                    effect.position,
                    effect.position + glm::normalize(effect.velocity) * std::min(velLen * 0.15f, 2.0f),
                    {1.0f, 0.9f, 0.0f, 0.85f});
            }
            DebugVis::drawPointCross(camera, effect.position, 0.04f, {1.0f, 0.6f, 0.0f, 0.8f});
        }
        if (debrisCount > 0) {
            centroid /= (float)debrisCount;
            char dbgLabel[64];
            snprintf(dbgLabel, sizeof(dbgLabel), "debris:%d", debrisCount);
            DebugVis::drawWorldLabel(centroid + glm::vec3(0, 0, 0.3f), dbgLabel, {1.0f, 0.9f, 0.0f, 1.0f});
        }
    }
}
