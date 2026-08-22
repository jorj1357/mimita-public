// 07 31 2026, 18 41
/* purpose
* Renders the EffectPartSystem effect pool (spheres, beams, boxes, decals, text).
* Owns per-effect render distance culling and distance fade for all effect types.
* Does NOT spawn effects, apply damage, or own server authority.
* Does NOT render viewmodels or run the fixed-step simulation tick.
*/
#include "effect-part.h"
#include "renderer/renderer.h"
#include "camera.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "config.h"
#include "world/texture-store.h"
#include <algorithm>
#include <cstdio>
#include <glm/glm.hpp>
#include <cstring>
#include <vector>

extern Renderer* gRenderer;

namespace {

struct TexturedParticleVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec4 color;
};

void drawTexturedHitParticles(const Camera& camera,
                              const std::vector<TexturedParticleVertex>& vertices,
                              const std::string& texturePath)
{
    if (vertices.empty() || texturePath.empty() || !gRenderer || !gRenderer->shaderProgram)
        return;

    static GLuint vao = 0;
    static GLuint vbo = 0;
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
    }

    const GLuint texture = gTextures.getPath(texturePath);
    if (!texture)
        return;

    const GLuint shader = gRenderer->shaderProgram;
    const glm::mat4 model(1.0f);
    const glm::mat4 view = camera.getView();
    const glm::mat4 projection = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 3);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(TexturedParticleVertex), vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedParticleVertex), (void*)offsetof(TexturedParticleVertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedParticleVertex), (void*)offsetof(TexturedParticleVertex, uv));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TexturedParticleVertex), (void*)offsetof(TexturedParticleVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(TexturedParticleVertex), (void*)offsetof(TexturedParticleVertex, color));
    glEnableVertexAttribArray(3);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());

    glDepthMask(GL_TRUE);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
}

void appendTexturedHitParticle(std::vector<TexturedParticleVertex>& vertices,
                               const Camera& camera, const EffectPart& effect,
                               float size, float alpha)
{
    const float rotation = effect.rotation.z;
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);
    const glm::vec3 right = (camera.right * c + camera.up * s) * size;
    const glm::vec3 up = (-camera.right * s + camera.up * c) * size;
    const glm::vec3 normal = -camera.front;
    const glm::vec4 color{effect.color.x, effect.color.y, effect.color.z, alpha};
    const glm::vec3 bl = effect.position - right - up;
    const glm::vec3 br = effect.position + right - up;
    const glm::vec3 tr = effect.position + right + up;
    const glm::vec3 tl = effect.position - right + up;

    vertices.push_back({bl, {0.0f, 0.0f}, normal, color});
    vertices.push_back({br, {1.0f, 0.0f}, normal, color});
    vertices.push_back({tr, {1.0f, 1.0f}, normal, color});
    vertices.push_back({bl, {0.0f, 0.0f}, normal, color});
    vertices.push_back({tr, {1.0f, 1.0f}, normal, color});
    vertices.push_back({tl, {0.0f, 1.0f}, normal, color});
}

} // namespace

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

static void drawDebrisBatch(const Camera& camera, const EffectPart& effect, float t, float alpha)
{
    unsigned int seed = (unsigned int)(effect.position.x * 73856093)
        ^ (unsigned int)(effect.position.y * 19349663)
        ^ (unsigned int)(effect.position.z * 83492791);
    seed ^= (unsigned int)(effect.normal.x * 52711);
    float spread = effect.scale;
    float baseSpeed = effect.endScale;
    float force = glm::length(effect.velocity);
    glm::vec3 n = effect.normal;
    glm::vec3 tangent = glm::normalize(
        std::abs(n.z) < 0.9f
            ? glm::cross(n, glm::vec3(0, 0, 1))
            : glm::cross(n, glm::vec3(0, 1, 0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

    int count = effect.debrisCount > 0
        ? effect.debrisCount
        : (8 + (seed & 7));
    glm::vec4 color{effect.color.x, effect.color.y, effect.color.z, alpha};

    for (int i = 0; i < count; ++i) {
        unsigned int si = seed + (unsigned int)i * 769u;
        float angle = (float)(si % 6283) / 1000.0f;
        float radius = ((float)((si * 311u) % 1000) / 1000.0f) * spread;
        // Bias direction outward from surface so debris doesn't hide behind geometry
        glm::vec3 dir = n * 2.0f
            + tangent * std::cos(angle) * radius
            + bitangent * std::sin(angle) * radius;
        dir = glm::normalize(dir);
        float speed = baseSpeed + ((si * 503u) % 5001) / 1000.0f;
        glm::vec3 pos = effect.position + n * 0.03f + dir * (((si * 211u) % 51) / 1000.0f);
        pos += dir * speed * t;
        float sx = 0.04f + force * 0.04f + ((si * 313u) % 501) / 3000.0f;
        float sy = 0.04f + force * 0.04f + ((si * 317u) % 501) / 3000.0f;
        float sz = 0.04f + force * 0.04f + ((si * 331u) % 501) / 3000.0f;
        glm::vec3 rot = {
            (float)((si * 419u) % 721 - 360),
            (float)((si * 421u) % 721 - 360),
            (float)((si * 431u) % 721 - 360)
        };
        rot += effect.angularVelocity * t;
        DebugVis::drawFilledBox(camera, pos, {sx, sy, sz}, color, rot);
    }
}

void EffectPartSystem::render(const Camera& camera) const {
    std::vector<TexturedParticleVertex> texturedHitParticles;
    texturedHitParticles.reserve(600);
    std::string texturedHitParticlePath;
    for (const auto& effect : mPool) {
        if (!effect.alive) continue;
        if (effect.lifetime < 0.0f) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;

        if (effect.replayType == "debris_batch") {
            float dist = glm::length(effect.position - camera.pos);
            if (dist > 40.0f) continue;
            float distFade = (dist > 20.0f) ? (40.0f - dist) / 20.0f : 1.0f;
            float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
            float alpha = effect.alpha * distFade * (1.0f - t);
            alpha = std::max(0.0f, alpha);
            drawDebrisBatch(camera, effect, t, alpha);
            continue;
        }

        float dist = glm::length(effect.position - camera.pos);

        // Disagreement effects use a much larger render distance so they're visible to all clients
        bool isDisagreementEffect =
            effect.replayType == "server_disagreement_pulse" ||
            effect.replayType == "server_disagreement_beam" ||
            effect.replayType == "server_disagreement_tracer" ||
            effect.replayType == "server_disagreement_text" ||
            effect.replayType == "server_disagreement_particle";

        // Explosion fireball/smoke also render at long range so the fireball is
        // visible wherever its shadow is (the shadow pass has no distance cull).
        bool isExplosionEffect =
            effect.replayType.find("_explosion_sphere") != std::string::npos ||
            effect.replayType.find("_explosion_smoke") != std::string::npos;

        float distFade;
        if (isDisagreementEffect)
        {
            if (dist > 200.0f) continue;
            distFade = (dist > 150.0f) ? (200.0f - dist) / 50.0f : 1.0f;
        }
        else if (isExplosionEffect)
        {
            const float cullDist = 120.0f;
            if (dist > cullDist) continue;
            distFade = (dist > cullDist * 0.5f) ? (cullDist - dist) / (cullDist * 0.5f) : 1.0f;
        }
        else
        {
            if (dist > 40.0f) continue;
            distFade = (dist > 20.0f) ? (40.0f - dist) / 20.0f : 1.0f;
        }

        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        const bool damageNumber = effect.replayType == "damage_number" ||
                                  effect.replayType == "server_disagreement_text";
        float alpha = effect.alpha * distFade;
        if (!damageNumber && (effect.replayType != "death_ellipsoid" || HitEffects::config().deathEllipsoid.fade))
            alpha *= (1.0f - t);
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};

        if (effect.replayType == "hitfx_particle" && !effect.texturePath.empty()) {
            if (!texturedHitParticlePath.empty() && texturedHitParticlePath != effect.texturePath) {
                drawTexturedHitParticles(camera, texturedHitParticles, texturedHitParticlePath);
                texturedHitParticles.clear();
            }
            texturedHitParticlePath = effect.texturePath;
            appendTexturedHitParticle(texturedHitParticles, camera, effect, drawScale, alpha);
            continue;
        }
        
        if (effect.beam) {
            // Beam width from thickness (interpolated), falling back to scale so
            // legacy spawners that only set scale still render.
            float beamWidth = effect.thickness +
                (effect.endThickness - effect.thickness) * t;
            if (beamWidth <= 0.0f)
                beamWidth = drawScale;
            DebugVis::drawFilledBeam(camera, effect.position, effect.endPosition, beamWidth, drawColor);
        }
        else if (effect.box) {
            DebugVis::drawFilledBox(camera, effect.position, effect.halfSize, drawColor, effect.rotation);
        }
        else if (effect.flatDecal) {
            DebugVis::drawFilledDecal(camera, effect.position, effect.normal, drawScale, drawColor);
        }
        else if (effect.replayType == "death_ellipsoid"
                 || effect.replayType == "freeze_trail" || effect.replayType == "down_dash") {
            glm::vec3 dir = glm::length(effect.endPosition - effect.position) > 0.001f
                ? glm::normalize(effect.endPosition - effect.position)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            float len = glm::length(effect.endPosition - effect.position);
            float rad = drawScale;
            glm::vec3 scaleVec = dir * (len / std::max(rad, 0.001f)) + glm::vec3(1.0f) - dir;
            DebugVis::drawFilledSphere(camera, effect.position, rad, drawColor, scaleVec);
        }
        else if (effect.replayType == "damage_impact_sphere") {
            const auto& impact = HitEffects::config().damageImpactSphere;
            const glm::vec3 axis = glm::length(effect.endPosition - effect.position) > 0.001f
                ? glm::normalize(effect.endPosition - effect.position)
                : glm::vec3(0.0f, 0.0f, 1.0f);
            const glm::vec3 center = (effect.position + effect.endPosition) * 0.5f;
            const glm::vec3 dimensions = glm::max(impact.localDimensions, glm::vec3(0.001f));
            DebugVis::drawFilledSphereOriented(camera, center, axis, 1.0f, drawColor,
                                               dimensions * 0.5f, impact.localAxis.c_str());
        }
        else if (!effect.billboardText) {
            DebugVis::drawFilledSphere(camera, effect.position, drawScale, drawColor);
        }
        
        if (effect.billboardText && !effect.label.empty()) {
            // Damage numbers must never write depth (that creates invisible
            // occluding rectangles over the world) and optionally skip depth
            // testing entirely so they float on top of geometry.
            GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            GLboolean depthMaskWasOn;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasOn);
            const auto& dnCfg = HitEffects::config().damageNumber;
            const std::string depthMode = damageNumber
                ? (dnCfg.occluded && dnCfg.depthMode == "always_on_top"
                       ? "occluded" : dnCfg.depthMode)
                : "always_on_top";
            const bool depthTestOn = depthMode != "always_on_top";
            const bool depthWriteOn = depthMode == "normal";
            if (depthTestOn)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            if (depthWriteOn)
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);

            float x = 0.0f, y = 0.0f;
            const glm::vec3 projectPos = damageNumber
                ? effect.position
                : effect.position + glm::vec3(0, 0, 0.5f);
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
                float fontSize = std::abs(effect.scale);
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

            if (depthMaskWasOn)
                glDepthMask(GL_TRUE);
            else
                glDepthMask(GL_FALSE);
            if (depthWasEnabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
        }
    }

    drawTexturedHitParticles(camera, texturedHitParticles, texturedHitParticlePath);

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
            {particle.color.x, particle.color.y, particle.color.z, particle.alpha * distFade});
    }

    for (const SurfaceDecal& decal : mSurfaceDecals) {
        const float dist = glm::length(decal.position - camera.pos);
        if (dist > 60.0f)
            continue;
        const float distFade = dist > 40.0f ? (60.0f - dist) / 20.0f : 1.0f;
        const float alpha = std::max(0.0f, decal.alpha * distFade);
        if (alpha <= 0.001f)
            continue;
        const glm::vec4 color{decal.color.x, decal.color.y, decal.color.z, alpha};
        if (decal.kind == SurfaceDecalKind::Crack) {
            DebugVis::drawFilledCylinder(camera, decal.position, decal.axis,
                std::max(0.001f, decal.radius), std::max(0.001f, decal.height), color);
        } else {
            DebugVis::drawFilledCylinder(camera, decal.position, decal.normal,
                std::max(0.001f, decal.radius), std::max(0.001f, decal.height), color);
        }
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
                   debrisCount, (int)mBloodParticles.size(), mSurfaceDecals.size());
        }
    }

    // Blood performance metrics
    if (DebugConfig::DEBUG_BLOOD_HITS) {
        static float bloodPerfTimer = 0.0f;
        bloodPerfTimer -= 0.016f;
        if (bloodPerfTimer <= 0.0f) {
            bloodPerfTimer = 2.0f;
            printf("[BLOOD PERF] particles=%zu decals=%zu collisionUpdates=0\n",
                   mBloodParticles.size(), mSurfaceDecals.size());
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
