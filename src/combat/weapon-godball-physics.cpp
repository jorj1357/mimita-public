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

static constexpr float MAX_GODBALL_SPEED = 120.0f;
static constexpr float GODBALL_GRAVITY = 15.0f;

void updatePhysics(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    const Camera& camera, float dt) {
    if (!phys.active) return;

    if (phys.hitstopTimer > 0.0f) {
        phys.hitstopTimer -= dt;
        return;
    }

    float safeDt = std::min(dt, 0.033f);

    phys.prevPosition = phys.position;

    glm::vec3 handPos = getHandPosition(owner);
    glm::vec3 handVel(0.0f);
    if (phys.hasPrevHandPos && safeDt > 0.0001f) {
        handVel = (handPos - phys.prevHandPos) / safeDt;
        float hSpeed = glm::length(handVel);
        if (hSpeed > 120.0f) handVel *= 120.0f / hSpeed;
    }
    phys.prevHandPos = handPos;
    phys.hasPrevHandPos = true;
    float handSpeed = glm::length(handVel);

    glm::vec3 toHand = handPos - phys.position;
    float dist = glm::length(toHand);
    glm::vec3 ropeDir(0.0f, 0.0f, 1.0f);
    if (dist > 0.001f) ropeDir = toHand / dist;
    bool ropeTaut = dist >= phys.ropeLength * 0.95f;

    phys.velocity.z -= GODBALL_GRAVITY * safeDt;

    if (ropeTaut && handSpeed > 0.5f) {
        float dragFactor = phys.ropeDamping * 0.015f;
        phys.velocity += handVel * std::min(dragFactor, 0.05f);
    }

    phys.position += phys.velocity * safeDt;

    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    phys.constraintDist = dist;

    if (dist > phys.ropeLength && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float overshoot = dist - phys.ropeLength;
        phys.ropeTension = overshoot * phys.ropeStiffness;

        phys.position = handPos - dir * phys.ropeLength;

        glm::vec3 correction = dir * overshoot;
        phys.velocity += correction / safeDt;

        float radialVel = glm::dot(phys.velocity, dir);
        if (radialVel < 0.0f) {
            phys.velocity -= dir * radialVel;
        }
    } else {
        phys.ropeTension = 0.0f;
    }

    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    float speed = glm::length(phys.velocity);
    if (dist > 0.1f) {
        ropeDir = toHand / dist;
        phys.angularVelocity = glm::cross(ropeDir, phys.velocity) / dist;
        float angSpeed = glm::length(phys.angularVelocity);
        constexpr float MAX_ANG_SPEED = 80.0f;
        if (angSpeed > MAX_ANG_SPEED) {
            phys.angularVelocity = (phys.angularVelocity / angSpeed) * MAX_ANG_SPEED;
        }
        glm::vec3 radialComp = ropeDir * glm::dot(phys.velocity, ropeDir);
        phys.tangentialSpeed = glm::length(phys.velocity - radialComp);
        phys.radialVel = glm::dot(ropeDir, phys.velocity);
    } else {
        phys.angularVelocity = glm::vec3(0.0f);
        phys.tangentialSpeed = speed;
        phys.radialVel = 0.0f;
    }

    if (speed > MAX_GODBALL_SPEED) {
        phys.velocity = (phys.velocity / speed) * MAX_GODBALL_SPEED;
    }

    glm::vec3 ballToOwner = phys.position - owner.pos;
    float ownerDist = glm::length(ballToOwner);
    constexpr float MIN_OWNER_DIST = 0.7f;
    if (ownerDist < MIN_OWNER_DIST && ownerDist > 0.001f) {
        glm::vec3 pushDir = ballToOwner / ownerDist;
        phys.position = owner.pos + pushDir * MIN_OWNER_DIST;
        float velToward = glm::dot(phys.velocity, pushDir);
        if (velToward < 0.0f) {
            phys.velocity -= pushDir * velToward;
        }
    }

    runtime.godball.ballPosition = phys.position;
    runtime.godball.ballVelocity = phys.velocity;

    phys.handedEnergy = handSpeed;
    phys.constraintDist = dist;

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] speed=%.2f tang=%.2f radial=%.2f "
               "dist=%.2f/%.1f tension=%.1f hand=%.1f\n",
               speed, phys.tangentialSpeed, phys.radialVel,
               phys.constraintDist, phys.ropeLength, phys.ropeTension,
               phys.handedEnergy);
    }
}

static bool sweptSphereOverlap(const glm::vec3& prevPos, const glm::vec3& currPos,
                                 float radius, const glm::vec3& targetPos, float targetRadius,
                                 glm::vec3& hitPoint, glm::vec3& hitNormal,
                                 glm::vec3* outClosest = nullptr) {
    glm::vec3 seg = currPos - prevPos;
    float segLen = glm::length(seg);

    if (segLen < 0.001f) {
        glm::vec3 diff = targetPos - currPos;
        float dist = glm::length(diff);
        if (dist < radius + targetRadius) {
            if (dist > 0.001f) {
                hitNormal = diff / dist;
            } else {
                hitNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            hitPoint = currPos + hitNormal * radius;
            if (outClosest) *outClosest = currPos;
            return true;
        }
        if (outClosest) *outClosest = currPos;
        return false;
    }

    glm::vec3 segDir = seg / segLen;
    glm::vec3 toTarget = targetPos - prevPos;

    float t = glm::dot(toTarget, segDir);
    t = std::clamp(t, 0.0f, segLen);

    glm::vec3 closestOnSeg = prevPos + segDir * t;
    if (outClosest) *outClosest = closestOnSeg;

    glm::vec3 diff = targetPos - closestOnSeg;
    float dist = glm::length(diff);
    float overlap = (radius + targetRadius) - dist;

    if (overlap > 0.0f) {
        if (dist > 0.001f) {
            hitNormal = diff / dist;
        } else {
            hitNormal = -segDir;
        }
        hitPoint = closestOnSeg + hitNormal * radius;
        return true;
    }
    return false;
}

static void tickImpactEvents(GodballPhysics& phys, float dt) {
    for (auto it = phys.impactEvents.begin(); it != phys.impactEvents.end(); ) {
        it->age += dt;
        if (it->age > 1.0f) {
            it = phys.impactEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void checkOverlaps(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    NpcSystem& npcs, const Camera& camera, float dt) {
    if (!phys.active) return;

    phys.lastFrameHit = false;
    phys.lastHitNormal = glm::vec3(0.0f);
    phys.npcCollisions.clear();
    tickImpactEvents(phys, dt);

    float safeDt = std::min(dt, 0.05f);
    float tickInterval = def.customParams.count("damageTickInterval")
        ? def.customParams.at("damageTickInterval") : 0.1f;

    runtime.godball.overlapDamageTimer -= safeDt;
    bool tickReady = runtime.godball.overlapDamageTimer <= 0.0f;
    if (tickReady) {
        runtime.godball.overlapDamageTimer = tickInterval;
    }

    const float damageRadius = phys.radius * 1.25f;
    glm::vec3 currPos = phys.position;
    glm::vec3 prevPos = phys.prevPosition;
    float ballSpeed = glm::length(phys.velocity);

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;

        npc.body.updateModelWorldTransforms();

        uint32_t npcId = npc.id;

        GodballPhysics::NpcCollisionDebug cd;
        cd.npcId = npcId;
        cd.ballSpeed = ballSpeed;
        cd.npcPos = npc.body.pos;

        glm::vec3 toTarget = npc.body.pos - currPos;
        cd.distanceToTarget = glm::length(toTarget);
        if (cd.distanceToTarget > 0.001f) {
            cd.angleDot = glm::dot(toTarget / cd.distanceToTarget,
                ballSpeed > 0.001f ? glm::normalize(phys.velocity) : glm::vec3(0.0f, 1.0f, 0.0f));
        }

        auto& cooldowns = runtime.godball.targetCooldowns;
        auto cooldownIt = cooldowns.find(npcId);
        if (cooldownIt != cooldowns.end() && cooldownIt->second > 0.0f) {
            cd.cooldownActive = true;
            cd.rejected = true;
            cd.rejectReason = "cooldown";
            if (DebugConfig::DEBUG_GODBALL) {
                printf("[GODBALL] npc=%u cooldown=%.3f -> skip\n",
                       npcId, cooldownIt->second);
            }
            phys.npcCollisions.push_back(cd);
            continue;
        }
        cd.cooldownActive = false;

        if (!tickReady) {
            cd.rejected = true;
            cd.rejectReason = "notTickReady";
            phys.npcCollisions.push_back(cd);
            continue;
        }

        bool hit = false;
        glm::vec3 hitPoint, hitNormal, closestOnSeg;
        std::string hitPartName;

        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 worldCenter = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 halfSize = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float partRadius = glm::length(halfSize) * 1.25f;

            if (sweptSphereOverlap(prevPos, currPos, damageRadius,
                                   worldCenter, partRadius,
                                   hitPoint, hitNormal, &closestOnSeg)) {
                hit = true;
                hitPartName = part.name;
                break;
            }
        }

        cd.overlapCheck = true;
        cd.sweptHit = hit;
        cd.hitPoint = hitPoint;
        cd.hitNormal = hitNormal;
        cd.sweepClosest = closestOnSeg;
        cd.overlapAmount = damageRadius + 0.5f - cd.distanceToTarget;

        if (!hit) {
            cd.rejected = true;
            cd.rejectReason = "noIntersection";
            if (DebugConfig::DEBUG_GODBALL) {
                printf("[GODBALL] npc=%u noIntersection dist=%.2f\n",
                       npcId, cd.distanceToTarget);
            }
            phys.npcCollisions.push_back(cd);
            continue;
        }

        phys.lastFrameHit = true;
        phys.lastHitNormal = hitNormal;

        float damage = computeDamage(phys, def, owner, npc.body, hitPoint);
        int rounded = std::max(1, (int)std::round(damage));
        cd.computedDamage = damage;

        printf("[GODBALL HIT] speed=%.2f damage=%d part=%s target=%s\n",
               ballSpeed, rounded, hitPartName.c_str(), npc.body.username.c_str());

        glm::vec3 kbDir;
        if (ballSpeed > 0.5f) {
            glm::vec3 velDir = glm::normalize(phys.velocity);
            float velInfluence = std::min(ballSpeed / 15.0f, 1.0f);
            kbDir = glm::normalize(glm::mix(hitNormal, velDir, velInfluence));
        } else {
            kbDir = hitNormal;
        }
        if (glm::length(kbDir) < 0.001f)
            kbDir = glm::vec3(0.0f, 1.0f, 0.0f);

        float impactSpeed = std::max(ballSpeed, 1.0f);
        float knockbackBase = damage * 0.02f;
        float speedScale = impactSpeed / 10.0f;
        float knockbackForce = knockbackBase * (0.5f + 0.5f * speedScale);

        npc.body.currentHp = std::max(0, npc.body.currentHp - rounded);
        npc.body.vel += kbDir * knockbackForce + glm::vec3(0, 0, knockbackForce * 0.4f);
        npc.hitReactionTimer = 0.25f + std::min(ballSpeed * 0.005f, 0.15f);

        {
            GodballPhysics::ImpactEvent ev;
            ev.position = npc.body.pos + glm::vec3(0, 0, 0.8f);
            ev.normal = kbDir;
            ev.damage = (float)rounded;
            ev.velocity = ballSpeed;
            ev.age = 0.0f;
            phys.impactEvents.push_back(ev);
            if (phys.impactEvents.size() > 16)
                phys.impactEvents.erase(phys.impactEvents.begin());
        }

        if (DebugConfig::DEBUG_GODBALL_HITSTOP) {
            phys.hitstopTimer = 0.08f;
        }

        glm::vec3 hitPos = npc.body.pos + glm::vec3(0, 0, 0.8f);
        {
            HitEvent ev;
            ev.position = hitPos;
            ev.normal = kbDir;
            ev.direction = kbDir;
            ev.hitEntity = true;
            ev.damage = rounded;
            ev.attacker = owner.username;
            ev.victim = "npc_" + std::to_string(npcId);
            ev.weaponSource = "godball";
            HitEffects::onHit(ev);
        }

        {
            float maxPossibleDamage = def.customParams.count("maxDamageCap")
                ? def.customParams.at("maxDamageCap") : 200.0f;
            float damageFraction = std::clamp((float)rounded / maxPossibleDamage, 0.0f, 1.0f);
            WeaponAudio::playGodballImpact(hitPos, damageFraction);
        }

        cooldowns[npcId] = tickInterval;
        hitmarker(rounded);

        if (npc.body.currentHp <= 0) {
            DeathSystem::instance().kill(
                npc.body, "npc_" + std::to_string(npcId), "npc",
                owner.username, kbDir, 8.0f + ballSpeed * 0.15f);
            std::string line = owner.username + " killed " + npc.body.username + " with Godball";
            Terminal::instance().addLog(line);
        }

        phys.npcCollisions.push_back(cd);
    }

    if (tickReady) {
        for (auto& pair : runtime.godball.targetCooldowns) {
            if (pair.second > 0.0f) pair.second -= tickInterval;
        }
    }
}

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
