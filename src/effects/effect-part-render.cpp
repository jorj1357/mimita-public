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

void EffectPartSystem::render(const Camera& camera) const {
    for (const auto& effect : mPool) {
        if (!effect.alive) continue;
        if (effect.lifetime < 0.0f) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;

        float dist = glm::length(effect.position - camera.pos);
        if (dist > 40.0f) continue;
        float distFade = (dist > 20.0f) ? (40.0f - dist) / 20.0f : 1.0f;

        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        float alpha = effect.alpha * distFade;
        if (effect.replayType != "death_ellipsoid" || HitEffects::config().deathEllipsoid.fade)
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
        else {
            DebugVis::drawFilledSphere(camera, effect.position, drawScale, drawColor);
        }
        
        if (effect.billboardText && !effect.label.empty()) {
            float x, y;
            if (DebugVis::projectToScreen(camera, effect.position + glm::vec3(0, 0, effect.scale + 0.15f), x, y)) {
                glm::vec4 textColor = {effect.color.x, effect.color.y, effect.color.z, alpha};
                uiDrawText(effect.label.c_str(), x, y, 0.3f * effect.scale, textColor);
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
