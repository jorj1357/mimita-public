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

    // Spawn ball hanging below hand with a slight sideways nudge for initial swing
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

    // === 4. SUBTLE HAND DRAG (only when rope taut) ===
    // A tiny fraction of hand velocity transfers to the ball
    // to simulate drag from the rope anchor moving.
    // Most flail energy comes from anchor movement -> constraint correction.
    if (ropeTaut && handSpeed > 0.5f) {
        float dragFactor = phys.ropeDamping * 0.015f;
        phys.velocity += handVel * std::min(dragFactor, 0.05f);
    }

    // === 5. INTEGRATE POSITION (semi-implicit Euler) ===
    phys.position += phys.velocity * safeDt;

    // === 6. HARD ROPE CONSTRAINT WITH VELOCITY CORRECTION ===
    // The constraint pulls the ball back to the rope sphere.
    // Velocity correction converts constraint energy into ball momentum,
    // creating natural swing when the anchor (hand) moves.
    //
    // Key: inward velocity (toward hand) is preserved. Only outward
    // velocity (away from hand) is removed. This lets the ball build
    // orbital momentum and swing naturally past the hand.
    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    phys.constraintDist = dist;

    if (dist > phys.ropeLength && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float overshoot = dist - phys.ropeLength;
        phys.ropeTension = overshoot * phys.ropeStiffness;

        // Hard positional snap to rope sphere
        phys.position = handPos - dir * phys.ropeLength;

        // Velocity correction: constraint force feeds inward momentum
        glm::vec3 correction = dir * overshoot;
        phys.velocity += correction / safeDt;

        // Remove any residual outward velocity (ball moving away from hand)
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
                                 glm::vec3& hitPoint, glm::vec3& hitNormal) {
    // Segment from prev to current ball position
    glm::vec3 seg = currPos - prevPos;
    float segLen = glm::length(seg);

    if (segLen < 0.001f) {
        // Stationary: simple distance check
        glm::vec3 diff = targetPos - currPos;
        float dist = glm::length(diff);
        if (dist < radius + targetRadius) {
            if (dist > 0.001f) {
                hitNormal = diff / dist;
            } else {
                hitNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            hitPoint = currPos + hitNormal * radius;
            return true;
        }
        return false;
    }

    glm::vec3 segDir = seg / segLen;
    glm::vec3 toTarget = targetPos - prevPos;

    // Project target center onto segment
    float t = glm::dot(toTarget, segDir);
    t = std::clamp(t, 0.0f, segLen);

    glm::vec3 closestOnSeg = prevPos + segDir * t;
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

void checkOverlaps(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    NpcSystem& npcs, const Camera& camera, float dt) {
    if (!phys.active) return;

    phys.lastFrameHit = false;
    phys.lastHitNormal = glm::vec3(0.0f);

    float safeDt = std::min(dt, 0.05f);
    float tickInterval = def.customParams.count("damageTickInterval")
        ? def.customParams.at("damageTickInterval") : 0.1f;

    runtime.godball.overlapDamageTimer -= safeDt;
    if (runtime.godball.overlapDamageTimer > 0.0f) {
        for (auto& pair : runtime.godball.targetCooldowns) {
            if (pair.second > 0.0f) pair.second -= safeDt;
        }
        return;
    }
    runtime.godball.overlapDamageTimer = tickInterval;

    const float npcCollisionRadius = 0.5f;
    glm::vec3 currPos = phys.position;
    glm::vec3 prevPos = phys.prevPosition;
    float ballSpeed = glm::length(phys.velocity);

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        uint32_t npcId = npc.id;

        auto& cooldowns = runtime.godball.targetCooldowns;
        auto cooldownIt = cooldowns.find(npcId);
        if (cooldownIt != cooldowns.end() && cooldownIt->second > 0.0f) {
            continue;
        }

        // Swept sphere overlap check
        glm::vec3 hitPoint, hitNormal;
        bool hit = sweptSphereOverlap(
            prevPos, currPos, phys.radius,
            npc.body.pos, npcCollisionRadius,
            hitPoint, hitNormal);

        if (!hit) continue;

        // Hit detected!
        phys.lastFrameHit = true;
        phys.lastHitNormal = hitNormal;

        float damage = computeDamage(phys, def, owner, npc.body,
            hitPoint);
        int rounded = std::max(1, (int)std::round(damage));

        // === PHYSICS-DRIVEN KNOCKBACK ===
        // Knockback direction: blend of ball velocity and hit normal
        glm::vec3 kbDir;
        if (ballSpeed > 0.5f) {
            glm::vec3 velDir = glm::normalize(phys.velocity);
            // More velocity-driven the faster the ball moves
            float velInfluence = std::min(ballSpeed / 15.0f, 1.0f);
            kbDir = glm::normalize(glm::mix(hitNormal, velDir, velInfluence));
        } else {
            kbDir = hitNormal;
        }
        if (glm::length(kbDir) < 0.001f)
            kbDir = glm::vec3(0.0f, 1.0f, 0.0f);

        // Knockback scales with impact speed and damage
        float impactSpeed = std::max(ballSpeed, 1.0f);
        float knockbackBase = damage * 0.02f;
        float speedScale = impactSpeed / 10.0f;
        float knockbackForce = knockbackBase * (0.5f + 0.5f * speedScale);

        npc.body.currentHp = std::max(0, npc.body.currentHp - rounded);
        npc.body.vel += kbDir * knockbackForce + glm::vec3(0, 0, knockbackForce * 0.4f);
        npc.hitReactionTimer = 0.25f + std::min(ballSpeed * 0.005f, 0.15f);

        if (DebugConfig::DEBUG_GODBALL) {
            printf("[GODBALL OVERLAP] npc=%u damage=%d vel=%.1f kb=%.2f\n",
                   npcId, rounded, ballSpeed, knockbackForce);
        }

        // === BLOOD EFFECTS ===
        glm::vec3 hitPos = npc.body.pos + glm::vec3(0, 0, 0.8f);
        float intensity = std::min((float)rounded / 20.0f, 2.0f);

        EffectPartSystem::instance().spawnDamage(
            hitPos, npc.body.username, rounded);

        EffectPartSystem::instance().spawnBloodSphereBurst(
            hitPos, kbDir, intensity,
            owner.username, "npc_" + std::to_string(npcId));

        EffectPartSystem::instance().spawnBloodSpurt(
            hitPos, kbDir,
            owner.username, "npc_" + std::to_string(npcId));

        EffectPartSystem::instance().spawnEntityImpact(
            hitPos, kbDir,
            owner.username, "npc_" + std::to_string(npcId));

        {
            float maxPossibleDamage = def.customParams.count("maxDamageCap")
                ? def.customParams.at("maxDamageCap") : 200.0f;
            float damageFraction = std::clamp((float)rounded / maxPossibleDamage, 0.0f, 1.0f);
            WeaponAudio::playGodballImpact(hitPos, damageFraction);
        }

        if (DebugConfig::DEBUG_GODBALL) {
            printf("[GODBALL HIT] target=%s damage=%d vel=%.1f\n",
                   npc.body.username.c_str(), rounded, ballSpeed);
        }

        cooldowns[npcId] = tickInterval;
        hitmarker();

        if (npc.body.currentHp <= 0) {
            DeathSystem::instance().kill(
                npc.body,
                "npc_" + std::to_string(npcId),
                "npc",
                owner.username,
                kbDir,
                8.0f + ballSpeed * 0.15f);
            std::string line = owner.username + " killed " + npc.body.username + " with Godball";
            Terminal::instance().addLog(line);
        }
    }

    for (auto& pair : runtime.godball.targetCooldowns) {
        if (pair.second > 0.0f) pair.second -= tickInterval;
    }
}

float computeDamage(const GodballPhysics& phys, const WeaponDefinition& def,
                     const Player& owner, const Player& target,
                     const glm::vec3& overlapPoint) {
    float baseDamage = def.customParams.count("baseDamagePerTick")
        ? def.customParams.at("baseDamagePerTick") : 10.0f;
    float speedFactor = def.customParams.count("speedDamageFactor")
        ? def.customParams.at("speedDamageFactor") : 3.0f;
    float maxDamageCap = def.customParams.count("maxDamageCap")
        ? def.customParams.at("maxDamageCap") : 200.0f;

    float ballSpeed = glm::length(phys.velocity);
    float speedMultiplier = 1.0f + (ballSpeed / 10.0f) * speedFactor;

    // Impact angle: dot(ball_vel, target_to_ball)
    glm::vec3 toTarget = target.pos - phys.position;
    float dist = glm::length(toTarget);
    float angleFactor = 1.0f;
    if (dist > 0.001f && ballSpeed > 0.001f) {
        glm::vec3 dirToTarget = toTarget / dist;
        angleFactor = 0.5f + 0.5f * std::max(0.0f,
            glm::dot(glm::normalize(phys.velocity), dirToTarget));
    }

    // Relative velocity bonus
    float relativeFactor = def.customParams.count("relativeVelocityFactor")
        ? def.customParams.at("relativeVelocityFactor") : 2.0f;
    glm::vec3 relativeVel = phys.velocity - target.vel;
    float relativeSpeed = glm::length(relativeVel);
    float relativeMultiplier = 1.0f + (relativeSpeed / 15.0f) * relativeFactor;

    // Swing direction bonus
    float swingFactor = def.customParams.count("swingDirectionFactor")
        ? def.customParams.at("swingDirectionFactor") : 2.0f;
    glm::vec3 ownerToBall = phys.position - owner.pos;
    float swingBonus = 1.0f;
    if (glm::length(ownerToBall) > 0.001f && ballSpeed > 0.001f) {
        ownerToBall = glm::normalize(ownerToBall);
        swingBonus = 1.0f + std::max(0.0f,
            glm::dot(glm::normalize(phys.velocity), ownerToBall)) * swingFactor;
    }

    float totalDamage = baseDamage * speedMultiplier * angleFactor * relativeMultiplier * swingBonus;
    totalDamage = std::clamp(totalDamage, 1.0f, maxDamageCap);

    if (DebugConfig::DEBUG_GODBALL) {
        printf("[GODBALL DAMAGE] speed=%.1f speedMult=%.2f angle=%.2f relMult=%.2f swing=%.2f total=%.1f\n",
               ballSpeed, speedMultiplier, angleFactor, relativeMultiplier, swingBonus, totalDamage);
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
        // === GODBALL_DEBUG VISUALIZATION SPEC ===
        // Yellow line = rope
        // Green sphere = valid hit, Red sphere = no collision
        // Blue line = hit normal
        // White text = velocity magnitude

        // Rope line (yellow)
        DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 0.9f});

        // Ball collision sphere: green if last frame was a hit, red otherwise
        glm::vec4 sphereColor = phys.lastFrameHit
            ? glm::vec4(0.0f, 1.0f, 0.0f, 0.5f)
            : glm::vec4(1.0f, 0.0f, 0.0f, 0.35f);
        DebugVis::drawWireSphere(camera, phys.position, phys.radius, sphereColor);

        // Also draw the overlap radius in faint version of same color
        DebugVis::drawWireSphere(camera, phys.position, phys.radius + 0.5f,
            phys.lastFrameHit
                ? glm::vec4(0.0f, 1.0f, 0.0f, 0.25f)
                : glm::vec4(1.0f, 0.0f, 0.0f, 0.15f));

        // Hit normal (blue line from impact point)
        if (phys.lastFrameHit && glm::length(phys.lastHitNormal) > 0.001f) {
            glm::vec3 normalEnd = phys.position + phys.lastHitNormal * 1.5f;
            DebugVis::drawLine(camera, phys.position, normalEnd, {0.0f, 0.0f, 1.0f, 1.0f});
        }

        // White text label with velocity magnitude
        char label[64];
        snprintf(label, sizeof(label), "%.1f m/s", speed);
        DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.4f),
                                  label, {1.0f, 1.0f, 1.0f, 1.0f});

        // Velocity vector (magenta)
        if (speed > 0.1f) {
            glm::vec3 velEnd = phys.position + glm::normalize(phys.velocity) * std::min(speed * 0.3f, 5.0f);
            DebugVis::drawLine(camera, phys.position, velEnd, {1.0f, 0.0f, 1.0f, 1.0f});
        }
    }

    // Always show basic debug info if master debug visuals are on
    if (DebugVis::enabled() && !DebugConfig::DEBUG_GODBALL) {
        // Rope line (yellow)
        DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 0.8f});

        // Basic info label
        char label[128];
        snprintf(label, sizeof(label),
                 "GODBALL %.1f m/s  T=%.1f  D=%.2f/%.1f",
                 speed, phys.ropeTension, phys.constraintDist, phys.ropeLength);
        DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.5f),
                                  label, {1.0f, 1.0f, 1.0f, 1.0f});
    }
}

} // namespace WeaponGodball
