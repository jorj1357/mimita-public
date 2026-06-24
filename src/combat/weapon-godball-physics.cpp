#include "weapon-godball.h"
#include "weapon-audio.h"
#include "weapon-types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "combat/death-system.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"

namespace WeaponGodball {
static void renderRopeCylinders(const Camera& camera, const glm::vec3& handPos,
                                 const glm::vec3& ballPos, float ropeLength) {
    glm::vec3 dir = ballPos - handPos;
    float dist = glm::length(dir);
    if (dist < 0.01f) return;

    constexpr int SEGMENTS = 6;
    constexpr float ROPE_RADIUS = 0.025f;
    glm::vec3 segDir = dir / (float)SEGMENTS;
    glm::vec3 current = handPos;

    for (int i = 0; i < SEGMENTS; i++) {
        glm::vec3 next = current + segDir;
        glm::vec3 mid = (current + next) * 0.5f;
        glm::vec3 axis = next - current;
        float segLen = glm::length(axis);
        if (segLen > 0.001f) {
            DebugVis::drawFilledCylinder(camera, mid, glm::normalize(axis),
                                          ROPE_RADIUS, segLen, {0.6f, 0.5f, 0.3f, 0.9f});
        }
        current = next;
    }
}

void render(const Camera& camera, const GodballPhysics& phys, const glm::vec3& handPos) {
    if (!phys.active) return;

    DebugVis::drawFilledSphere(camera, phys.position, phys.radius, {0.2f, 0.4f, 0.8f, 0.7f});
    DebugVis::drawWireSphere(camera, phys.position, phys.radius, {0.4f, 0.6f, 1.0f, 1.0f});

    renderRopeCylinders(camera, handPos, phys.position, phys.ropeLength);
}

void renderDebug(const Camera& camera, const GodballPhysics& phys,
                  const WeaponRuntime& runtime, const glm::vec3& handPos) {
    if (!phys.active) return;

    float speed = glm::length(phys.velocity);

    if (DebugConfig::DEBUG_GODBALL) {
        DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 1.0f});
        DebugVis::drawFilledBeam(camera, handPos, phys.position, 0.03f,
                                 {1.0f, 1.0f, 0.0f, 0.4f});

        DebugVis::drawFilledSphere(camera, handPos, 0.1f, {1.0f, 1.0f, 0.0f, 0.8f});

        if (glm::length(phys.position - phys.prevPosition) > 0.001f) {
            DebugVis::drawFilledBeam(camera, phys.prevPosition, phys.position,
                                     0.06f, {0.0f, 0.0f, 1.0f, 0.5f});
            DebugVis::drawLine(camera, phys.prevPosition, phys.position,
                               {0.0f, 0.5f, 1.0f, 0.9f});
            DebugVis::drawWireSphere(camera, phys.prevPosition, 0.08f,
                                     {0.0f, 0.5f, 1.0f, 0.6f});
        }

        glm::vec3 sphereColor = phys.lastFrameHit
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.3f, 0.0f);

        DebugVis::drawWireSphere(camera, phys.position, phys.radius,
                                 {sphereColor.x, sphereColor.y, sphereColor.z, 0.9f});

        DebugVis::drawFilledSphere(camera, phys.position, phys.radius,
                                   {sphereColor.x, sphereColor.y, sphereColor.z, 0.15f});

        float overlapRadius = phys.radius + 0.5f;
        glm::vec4 overlapColor = phys.lastFrameHit
            ? glm::vec4(0.0f, 1.0f, 0.0f, 0.2f)
            : glm::vec4(1.0f, 0.0f, 0.0f, 0.12f);
        DebugVis::drawWireSphere(camera, phys.position, overlapRadius, overlapColor);

        DebugVis::drawFilledSphere(camera, phys.position, 0.06f,
                                   {1.0f, 1.0f, 1.0f, 0.9f});

        if (speed > 0.1f) {
            glm::vec3 velEnd = phys.position + glm::normalize(phys.velocity) * std::min(speed * 0.3f, 5.0f);
            DebugVis::drawFilledBeam(camera, phys.position, velEnd, 0.03f,
                                     {1.0f, 0.0f, 1.0f, 0.7f});
            DebugVis::drawLine(camera, phys.position, velEnd,
                               {1.0f, 0.0f, 1.0f, 1.0f});
            DebugVis::drawFilledSphere(camera, velEnd, 0.08f,
                                       {1.0f, 0.0f, 1.0f, 0.9f});
        }

        if (phys.lastFrameHit && glm::length(phys.lastHitNormal) > 0.001f) {
            glm::vec3 normalStart = phys.position + phys.lastHitNormal * phys.radius;
            glm::vec3 normalEnd = normalStart + phys.lastHitNormal * 1.5f;
            DebugVis::drawFilledBeam(camera, normalStart, normalEnd, 0.04f,
                                     {0.0f, 0.0f, 1.0f, 0.8f});
            DebugVis::drawLine(camera, normalStart, normalEnd,
                               {0.0f, 0.0f, 1.0f, 1.0f});
        }

        for (const auto& cd : phys.npcCollisions) {
            glm::vec3 npcPos = cd.npcPos;
            float npcRadius = 0.5f;

            glm::vec4 hurtColor;
            if (cd.rejected) {
                hurtColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.4f);
            } else if (cd.sweptHit) {
                hurtColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.8f);
            } else {
                hurtColor = glm::vec4(1.0f, 1.0f, 0.0f, 0.4f);
            }

            DebugVis::drawWireSphere(camera, npcPos, npcRadius, hurtColor);
            DebugVis::drawFilledSphere(camera, npcPos, npcRadius,
                                       {hurtColor.x, hurtColor.y, hurtColor.z, hurtColor.w * 0.3f});

            DebugVis::drawLine(camera, phys.position, npcPos,
                               {hurtColor.x, hurtColor.y, hurtColor.z, 0.3f});

            if (glm::length(cd.sweepClosest) > 0.001f) {
                glm::vec4 scColor = cd.sweptHit
                    ? glm::vec4(0.0f, 1.0f, 0.0f, 0.8f)
                    : glm::vec4(1.0f, 0.5f, 0.0f, 0.6f);
                DebugVis::drawFilledSphere(camera, cd.sweepClosest, 0.1f, scColor);
            }

            char npcLabel[256];
            if (cd.rejected) {
                snprintf(npcLabel, sizeof(npcLabel),
                    "[GODBALL] npc=%u reject=%s dist=%.2f",
                    cd.npcId, cd.rejectReason.c_str(), cd.distanceToTarget);
            } else {
                snprintf(npcLabel, sizeof(npcLabel),
                    "[GODBALL] npc=%u speed=%.1f damage=%.0f "
                    "overlap=%.2f angleDot=%.2f",
                    cd.npcId, cd.ballSpeed, cd.computedDamage,
                    cd.overlapAmount, cd.angleDot);
            }
            DebugVis::drawWorldLabel(npcPos + glm::vec3(0.0f, 0.0f, npcRadius + 0.5f),
                                      npcLabel, {1.0f, 1.0f, 1.0f, 0.9f});
        }

        for (const auto& ev : phys.impactEvents) {
            float alpha = std::max(0.0f, 1.0f - ev.age);
            float scale = 1.0f + ev.age * 2.0f;

            DebugVis::drawFilledSphere(camera, ev.position, 0.3f * scale,
                                       {1.0f, 0.8f, 0.0f, alpha * 0.5f});
            DebugVis::drawWireSphere(camera, ev.position, 0.4f * scale,
                                     {1.0f, 0.8f, 0.0f, alpha * 0.8f});

            DebugVis::drawFilledBeam(camera, ev.position,
                                     ev.position + ev.normal * 1.0f * scale,
                                     0.05f, {1.0f, 0.5f, 0.0f, alpha});

            char dmgLabel[64];
            snprintf(dmgLabel, sizeof(dmgLabel), "DMG: %.0f  VEL: %.1f",
                     ev.damage, ev.velocity);
            DebugVis::drawWorldLabel(ev.position + glm::vec3(0.0f, 0.0f, 0.6f * scale),
                                      dmgLabel, {1.0f, 1.0f, 0.0f, alpha});

            float overlapVisual = phys.radius + 0.5f;
            DebugVis::drawWireSphere(camera, ev.position, overlapVisual * scale * 0.5f,
                                     {0.0f, 1.0f, 0.0f, alpha * 0.4f});
        }

        char label[256];
        snprintf(label, sizeof(label),
                 "GODBALL: %.1f m/s  T=%.1f  D=%.2f/%.1f  "
                 "rad=%.2f  tan=%.1f  hits=%zu",
                 speed, phys.ropeTension, phys.constraintDist, phys.ropeLength,
                 phys.radialVel, phys.tangentialSpeed, phys.impactEvents.size());
        DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.8f),
                                  label, {1.0f, 1.0f, 1.0f, 1.0f});

        if (speed > 0.1f) {
            char velLabel[32];
            snprintf(velLabel, sizeof(velLabel), "%.1f m/s", speed);
            glm::vec3 velTip = phys.position + glm::normalize(phys.velocity)
                * std::min(speed * 0.3f, 5.0f);
            DebugVis::drawWorldLabel(velTip + glm::vec3(0.0f, 0.0f, 0.2f),
                                      velLabel, {1.0f, 0.0f, 1.0f, 0.9f});
        }

        if (phys.hitstopTimer > 0.0f) {
            char hsLabel[64];
            snprintf(hsLabel, sizeof(hsLabel), "HITSTOP: %.3f", phys.hitstopTimer);
            DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 1.8f),
                                      hsLabel, {0.0f, 1.0f, 1.0f, 1.0f});
        }

    } else if (DebugVis::enabled()) {
        DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 0.8f});

        char label[128];
        snprintf(label, sizeof(label),
                 "GODBALL %.1f m/s  T=%.1f  D=%.2f/%.1f",
                 speed, phys.ropeTension, phys.constraintDist, phys.ropeLength);
        DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.5f),
                                  label, {1.0f, 1.0f, 1.0f, 1.0f});
    }
}

} // namespace WeaponGodball

