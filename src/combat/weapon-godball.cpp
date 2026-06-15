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

glm::vec3 getHandPosition(const Player& player) {
    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name == "rightArm") {
            glm::vec3 center = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 boundsSize = part.collider.localMax - part.collider.localMin;
            int axis = boundsSize.y > boundsSize.x ? 1 : 0;
            if (boundsSize.z > boundsSize[axis]) axis = 2;
            float minDist = std::fabs(part.collider.localMin[axis]);
            float maxDist = std::fabs(part.collider.localMax[axis]);
            center[axis] = maxDist >= minDist ? part.collider.localMax[axis] : part.collider.localMin[axis];
            return glm::vec3(part.worldTransform * glm::vec4(center, 1.0f));
        }
    }
    return player.pos + glm::vec3(0.0f, 0.5f, 1.0f);
}

void spawnBall(GodballPhysics& phys, const WeaponDefinition& def, const Player& owner) {
    glm::vec3 handPos = getHandPosition(owner);
    phys.radius = def.customParams.count("ballRadius") ? def.customParams.at("ballRadius") : 0.5f;
    phys.mass = def.customParams.count("ballMass") ? def.customParams.at("ballMass") : 0.1f;
    phys.ropeLength = def.customParams.count("ropeLength") ? def.customParams.at("ropeLength") : 2.5f;
    phys.ropeStiffness = def.customParams.count("ropeStiffness") ? def.customParams.at("ropeStiffness") : 50.0f;
    phys.ropeDamping = def.customParams.count("ropeDamping") ? def.customParams.at("ropeDamping") : 2.0f;
    phys.linearDamping = def.customParams.count("linearDamping") ? def.customParams.at("linearDamping") : 0.005f;

    float yawRad = glm::radians(owner.yaw);
    glm::vec3 ownerForward = glm::vec3(std::cos(yawRad), std::sin(yawRad), 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(ownerForward, glm::vec3(0.0f, 0.0f, 1.0f)));
    if (glm::length(right) < 0.1f) right = glm::vec3(1.0f, 0.0f, 0.0f);

    phys.position = handPos - glm::vec3(0.0f, 0.0f, phys.ropeLength * 0.8f) + right * 0.5f;
    phys.prevPosition = phys.position;
    phys.velocity = right * 4.0f + owner.vel * 0.5f;
    phys.angularVelocity = glm::vec3(0.0f);
    phys.active = true;
    phys.ropeTension = 0.0f;
    phys.constraintDist = 0.0f;
    phys.prevHandPos = handPos;
    phys.hasPrevHandPos = true;
    phys.handedEnergy = 0.0f;
    phys.radialVel = 0.0f;
    phys.tangentialSpeed = 0.0f;
    phys.lastFrameHit = false;
    phys.lastHitNormal = glm::vec3(0.0f);
    phys.impactEvents.clear();
    phys.hitstopTimer = 0.0f;

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] spawned at (%.2f, %.2f, %.2f) vel (%.2f, %.2f, %.2f) rope=%.2f radius=%.2f\n",
               phys.position.x, phys.position.y, phys.position.z,
               phys.velocity.x, phys.velocity.y, phys.velocity.z,
               phys.ropeLength, phys.radius);
    }
}

void despawnBall(GodballPhysics& phys) {
    phys.active = false;
    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL] despawned\n");
    }
}

void updatePhysics(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    const Camera& camera, float dt) {
    if (!phys.active) return;

    // Hitstop: freeze physics during hitstop (godball_hitstop_debug)
    if (phys.hitstopTimer > 0.0f) {
        phys.hitstopTimer -= dt;
        return;
    }

    float safeDt = std::min(dt, 0.033f);

    // Save previous position for swept sphere collision
    phys.prevPosition = phys.position;

    // === 1. HAND TRACKING ===
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

    // === 2. ROPE STATE BEFORE INTEGRATION ===
    glm::vec3 toHand = handPos - phys.position;
    float dist = glm::length(toHand);
    glm::vec3 ropeDir(0.0f, 0.0f, 1.0f);
    if (dist > 0.001f) ropeDir = toHand / dist;
    bool ropeTaut = dist >= phys.ropeLength * 0.95f;

    // === 3. GRAVITY ===
    phys.velocity.z -= GODBALL_GRAVITY * safeDt;

    // === 4. SUBTLE HAND DRAG ===
    if (ropeTaut && handSpeed > 0.5f) {
        float dragFactor = phys.ropeDamping * 0.015f;
        phys.velocity += handVel * std::min(dragFactor, 0.05f);
    }

    // === 5. INTEGRATE POSITION ===
    phys.position += phys.velocity * safeDt;

    // === 6. HARD ROPE CONSTRAINT ===
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

    // === 7. COMPUTE ORBITAL METRICS ===
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

    // === 8. SPEED CLAMP ===
    if (speed > MAX_GODBALL_SPEED) {
        phys.velocity = (phys.velocity / speed) * MAX_GODBALL_SPEED;
    }

    // === 9. PLAYER BODY AVOIDANCE ===
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

    // === 10. STORE STATE ===
    runtime.godball.ballPosition = phys.position;
    runtime.godball.ballVelocity = phys.velocity;

    // Debug fields
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

// Age out old impact events
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

        // Ensure body part world transforms are current
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

        // Check cooldown
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

        // Check overlap against ALL body parts
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

        // Hit detected! Apply damage.
        phys.lastFrameHit = true;
        phys.lastHitNormal = hitNormal;

        float damage = computeDamage(phys, def, owner, npc.body, hitPoint);
        int rounded = std::max(1, (int)std::round(damage));
        cd.computedDamage = damage;

        printf("[GODBALL HIT] speed=%.2f damage=%d part=%s target=%s\n",
               ballSpeed, rounded, hitPartName.c_str(), npc.body.username.c_str());

        // === PHYSICS-DRIVEN KNOCKBACK ===
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

        // Record impact event
        {
            GodballPhysics::ImpactEvent ev;
            ev.position = npc.body.pos + glm::vec3(0, 0, 0.8f);
            ev.normal = kbDir;
            ev.damage = (float)rounded;
            ev.velocity = ballSpeed;
            ev.age = 0.0f;
            phys.impactEvents.push_back(ev);
            // Cap events
            if (phys.impactEvents.size() > 16)
                phys.impactEvents.erase(phys.impactEvents.begin());
        }

        // Hitstop for debug
        if (DebugConfig::DEBUG_GODBALL_HITSTOP) {
            phys.hitstopTimer = 0.08f;
        }

        // === BLOOD EFFECTS ===
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
        hitmarker();

        if (npc.body.currentHp <= 0) {
            DeathSystem::instance().kill(
                npc.body, "npc_" + std::to_string(npcId), "npc",
                owner.username, kbDir, 8.0f + ballSpeed * 0.15f);
            std::string line = owner.username + " killed " + npc.body.username + " with Godball";
            Terminal::instance().addLog(line);
        }

        phys.npcCollisions.push_back(cd);
    }

    // Cooldown decrement
    if (tickReady) {
        for (auto& pair : runtime.godball.targetCooldowns) {
            if (pair.second > 0.0f) pair.second -= tickInterval;
        }
    }
}

float computeDamage(const GodballPhysics& phys, const WeaponDefinition& def,
                     const Player& owner, const Player& target,
                     const glm::vec3& overlapPoint) {
    (void)owner;
    (void)overlapPoint;
    float baseDamage = def.customParams.count("baseDamagePerTick")
        ? def.customParams.at("baseDamagePerTick") : 10.0f;
    float speedFactor = def.customParams.count("speedDamageFactor")
        ? def.customParams.at("speedDamageFactor") : 0.5f;
    float angleMultiplier = def.customParams.count("angleMultiplier")
        ? def.customParams.at("angleMultiplier") : 20.0f;
    float maxDamageCap = def.customParams.count("maxDamageCap")
        ? def.customParams.at("maxDamageCap") : 200.0f;

    float ballSpeed = glm::length(phys.velocity);

    // Always deal at least base damage
    float totalDamage = baseDamage;

    // Speed bonus
    totalDamage += ballSpeed * speedFactor;

    // Angle bonus: reward clean hits where ball moves toward target
    glm::vec3 toTarget = target.pos - phys.position;
    float dist = glm::length(toTarget);
    if (dist > 0.001f && ballSpeed > 0.001f) {
        glm::vec3 dirToTarget = toTarget / dist;
        float angleFactor = std::abs(glm::dot(glm::normalize(phys.velocity), dirToTarget));
        totalDamage += angleFactor * angleMultiplier;
    }

    totalDamage = std::clamp(totalDamage, baseDamage, maxDamageCap);

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL DAMAGE] speed=%.1f speedBonus=%.1f angleFactor=%.2f total=%.1f\n",
               ballSpeed, ballSpeed * speedFactor,
               dist > 0.001f && ballSpeed > 0.001f
                   ? (float)std::abs(glm::dot(glm::normalize(phys.velocity), glm::normalize(toTarget)))
                   : 0.0f,
               totalDamage);
    }

    return totalDamage;
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

    // Ball
    DebugVis::drawFilledSphere(camera, phys.position, phys.radius, {0.2f, 0.4f, 0.8f, 0.7f});
    DebugVis::drawWireSphere(camera, phys.position, phys.radius, {0.4f, 0.6f, 1.0f, 1.0f});

    // Rope
    renderRopeCylinders(camera, handPos, phys.position, phys.ropeLength);
}

void renderDebug(const Camera& camera, const GodballPhysics& phys,
                  const WeaponRuntime& runtime, const glm::vec3& handPos) {
    if (!phys.active) return;

    float speed = glm::length(phys.velocity);

    if (DebugConfig::DEBUG_GODBALL) {
        // ============================================================
        // GODBALL_DEBUG: COMPREHENSIVE COLLISION VISUALIZATION
        // ============================================================

        // --- 1. ROPE (yellow thick line) ---
        DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 1.0f});
        DebugVis::drawFilledBeam(camera, handPos, phys.position, 0.03f,
                                 {1.0f, 1.0f, 0.0f, 0.4f});

        // --- 2. TETHER ANCHOR AT HAND (yellow sphere) ---
        DebugVis::drawFilledSphere(camera, handPos, 0.1f, {1.0f, 1.0f, 0.0f, 0.8f});

        // --- 3. SWEEP PATH (blue beam from prevPos to currPos) ---
        if (glm::length(phys.position - phys.prevPosition) > 0.001f) {
            DebugVis::drawFilledBeam(camera, phys.prevPosition, phys.position,
                                     0.06f, {0.0f, 0.0f, 1.0f, 0.5f});
            DebugVis::drawLine(camera, phys.prevPosition, phys.position,
                               {0.0f, 0.5f, 1.0f, 0.9f});
            // Sweep path endpoints
            DebugVis::drawWireSphere(camera, phys.prevPosition, 0.08f,
                                     {0.0f, 0.5f, 1.0f, 0.6f});
        }

        // --- 4. COLLISION SPHERE - ACTUAL RADIUS ---
        // Cyan = physics position (collision), White = render position
        glm::vec3 sphereColor = phys.lastFrameHit
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.3f, 0.0f);

        // Physics collision sphere (thick wireframe)
        DebugVis::drawWireSphere(camera, phys.position, phys.radius,
                                 {sphereColor.x, sphereColor.y, sphereColor.z, 0.9f});

        // Filled collision sphere (translucent)
        DebugVis::drawFilledSphere(camera, phys.position, phys.radius,
                                   {sphereColor.x, sphereColor.y, sphereColor.z, 0.15f});

        // --- 5. DAMAGE / OVERLAP RADIUS (larger faint sphere) ---
        float overlapRadius = phys.radius + 0.5f;
        glm::vec4 overlapColor = phys.lastFrameHit
            ? glm::vec4(0.0f, 1.0f, 0.0f, 0.2f)
            : glm::vec4(1.0f, 0.0f, 0.0f, 0.12f);
        DebugVis::drawWireSphere(camera, phys.position, overlapRadius, overlapColor);

        // --- 6. VISUAL vs PHYSICS POSITION CHECK ---
        // White sphere = where it renders
        // Cyan sphere = physics collision position
        // If they differ, we see the mismatch
        DebugVis::drawFilledSphere(camera, phys.position, 0.06f,
                                   {1.0f, 1.0f, 1.0f, 0.9f});

        // --- 7. VELOCITY VECTOR (magenta arrow) ---
        if (speed > 0.1f) {
            glm::vec3 velEnd = phys.position + glm::normalize(phys.velocity) * std::min(speed * 0.3f, 5.0f);
            DebugVis::drawFilledBeam(camera, phys.position, velEnd, 0.03f,
                                     {1.0f, 0.0f, 1.0f, 0.7f});
            DebugVis::drawLine(camera, phys.position, velEnd,
                               {1.0f, 0.0f, 1.0f, 1.0f});
            // Arrow tip sphere
            DebugVis::drawFilledSphere(camera, velEnd, 0.08f,
                                       {1.0f, 0.0f, 1.0f, 0.9f});
        }

        // --- 8. HIT NORMAL (blue line from ball surface) ---
        if (phys.lastFrameHit && glm::length(phys.lastHitNormal) > 0.001f) {
            glm::vec3 normalStart = phys.position + phys.lastHitNormal * phys.radius;
            glm::vec3 normalEnd = normalStart + phys.lastHitNormal * 1.5f;
            DebugVis::drawFilledBeam(camera, normalStart, normalEnd, 0.04f,
                                     {0.0f, 0.0f, 1.0f, 0.8f});
            DebugVis::drawLine(camera, normalStart, normalEnd,
                               {0.0f, 0.0f, 1.0f, 1.0f});
        }

        // --- 9. ENEMY HURTBOXES ---
        // For each NPC, draw their collision capsule/sphere, colored by overlap status
        for (const auto& cd : phys.npcCollisions) {
            glm::vec3 npcPos = cd.npcPos;
            float npcRadius = 0.5f;

            glm::vec4 hurtColor;
            if (cd.rejected) {
                // Red = no valid collision
                hurtColor = glm::vec4(1.0f, 0.0f, 0.0f, 0.4f);
            } else if (cd.sweptHit) {
                // Green = hit detected
                hurtColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.8f);
            } else {
                // Yellow = overlap but invalid (shouldn't happen here)
                hurtColor = glm::vec4(1.0f, 1.0f, 0.0f, 0.4f);
            }

            // Enemy hurtbox sphere
            DebugVis::drawWireSphere(camera, npcPos, npcRadius, hurtColor);
            DebugVis::drawFilledSphere(camera, npcPos, npcRadius,
                                       {hurtColor.x, hurtColor.y, hurtColor.z, hurtColor.w * 0.3f});

            // Line from ball to NPC
            DebugVis::drawLine(camera, phys.position, npcPos,
                               {hurtColor.x, hurtColor.y, hurtColor.z, 0.3f});

            // Sweep closest point on segment
            if (glm::length(cd.sweepClosest) > 0.001f) {
                glm::vec4 scColor = cd.sweptHit
                    ? glm::vec4(0.0f, 1.0f, 0.0f, 0.8f)
                    : glm::vec4(1.0f, 0.5f, 0.0f, 0.6f);
                DebugVis::drawFilledSphere(camera, cd.sweepClosest, 0.1f, scColor);
            }

            // --- 10. PER-NPC LABEL ---
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

        // --- 11. IMPACT EVENTS (flash, damage number, velocity) ---
        for (const auto& ev : phys.impactEvents) {
            float alpha = std::max(0.0f, 1.0f - ev.age);
            float scale = 1.0f + ev.age * 2.0f;

            // Flash sphere
            DebugVis::drawFilledSphere(camera, ev.position, 0.3f * scale,
                                       {1.0f, 0.8f, 0.0f, alpha * 0.5f});
            DebugVis::drawWireSphere(camera, ev.position, 0.4f * scale,
                                     {1.0f, 0.8f, 0.0f, alpha * 0.8f});

            // Impact normal
            DebugVis::drawFilledBeam(camera, ev.position,
                                     ev.position + ev.normal * 1.0f * scale,
                                     0.05f, {1.0f, 0.5f, 0.0f, alpha});

            // Damage number
            char dmgLabel[64];
            snprintf(dmgLabel, sizeof(dmgLabel), "DMG: %.0f  VEL: %.1f",
                     ev.damage, ev.velocity);
            DebugVis::drawWorldLabel(ev.position + glm::vec3(0.0f, 0.0f, 0.6f * scale),
                                      dmgLabel, {1.0f, 1.0f, 0.0f, alpha});

            // Temporary overlap shape (growing wire sphere)
            float overlapVisual = phys.radius + 0.5f;
            DebugVis::drawWireSphere(camera, ev.position, overlapVisual * scale * 0.5f,
                                     {0.0f, 1.0f, 0.0f, alpha * 0.4f});
        }

        // --- 12. MAIN BALL LABEL ---
        char label[256];
        snprintf(label, sizeof(label),
                 "GODBALL: %.1f m/s  T=%.1f  D=%.2f/%.1f  "
                 "rad=%.2f  tan=%.1f  hits=%zu",
                 speed, phys.ropeTension, phys.constraintDist, phys.ropeLength,
                 phys.radialVel, phys.tangentialSpeed, phys.impactEvents.size());
        DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.8f),
                                  label, {1.0f, 1.0f, 1.0f, 1.0f});

        // Velocity label near velocity arrow tip
        if (speed > 0.1f) {
            char velLabel[32];
            snprintf(velLabel, sizeof(velLabel), "%.1f m/s", speed);
            glm::vec3 velTip = phys.position + glm::normalize(phys.velocity)
                * std::min(speed * 0.3f, 5.0f);
            DebugVis::drawWorldLabel(velTip + glm::vec3(0.0f, 0.0f, 0.2f),
                                      velLabel, {1.0f, 0.0f, 1.0f, 0.9f});
        }

        // --- 13. HITSTOP INDICATOR ---
        if (phys.hitstopTimer > 0.0f) {
            char hsLabel[64];
            snprintf(hsLabel, sizeof(hsLabel), "HITSTOP: %.3f", phys.hitstopTimer);
            DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 1.8f),
                                      hsLabel, {0.0f, 1.0f, 1.0f, 1.0f});
        }

    } else if (DebugVis::enabled()) {
        // Minimal debug when godball_debug is OFF but master debug is ON
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
