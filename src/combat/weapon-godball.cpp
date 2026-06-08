#include "weapon-godball.h"
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
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "effects/effect-part.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "ui/hitmarker.h"
#include "world/world.h"

namespace WeaponGodball {

static constexpr float MAX_GODBALL_SPEED = 80.0f;

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
    phys.position = handPos;
    phys.velocity = owner.vel * 1.2f;
    phys.prevHandPos = handPos;
    phys.prevHandValid = true;
    phys.active = true;
    phys.radius = def.customParams.count("ballRadius") ? def.customParams.at("ballRadius") : 0.5f;
    phys.mass = def.customParams.count("ballMass") ? def.customParams.at("ballMass") : 0.1f;
    phys.ropeLength = def.customParams.count("ropeLength") ? def.customParams.at("ropeLength") : 2.0f;
    phys.ropeStiffness = def.customParams.count("ropeStiffness") ? def.customParams.at("ropeStiffness") : 50.0f;
    phys.ropeDamping = def.customParams.count("ropeDamping") ? def.customParams.at("ropeDamping") : 2.0f;
    phys.linearDamping = def.customParams.count("linearDamping") ? def.customParams.at("linearDamping") : 0.02f;
    phys.ropeTension = 0.0f;
    phys.constraintDist = 0.0f;

    printf("[GODBALL] spawned at (%.2f, %.2f, %.2f) with vel (%.2f, %.2f, %.2f) rope=%.2f radius=%.2f\n",
           phys.position.x, phys.position.y, phys.position.z,
           phys.velocity.x, phys.velocity.y, phys.velocity.z,
           phys.ropeLength, phys.radius);
}

void despawnBall(GodballPhysics& phys) {
    phys.active = false;
    printf("[GODBALL] despawned\n");
}

void updatePhysics(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    const Camera& camera, float dt) {
    if (!phys.active) return;

    float safeDt = std::min(dt, 0.033f);

    glm::vec3 handPos = getHandPosition(owner);

    // === 1. MOMENTUM TRANSFER FROM HAND TRAJECTORY ===
    // Track hand velocity to transfer orbital/linear momentum to the ball.
    // This naturally handles both player translation AND rotation.
    if (phys.prevHandValid) {
        glm::vec3 handVel = (handPos - phys.prevHandPos) / std::max(safeDt, 0.0001f);
        float handSpeed = glm::length(handVel);
        if (handSpeed > 40.0f) handVel = (handVel / handSpeed) * 40.0f;
        phys.velocity += handVel * 0.35f;
    }
    phys.prevHandPos = handPos;
    phys.prevHandValid = true;

    // === 2. LIGHT GRAVITY (weighted feel, not floaty) ===
    phys.velocity.z -= 3.0f * safeDt;

    // === 3. DAMPING (very gentle — let the ball swing) ===
    phys.velocity *= std::max(0.0f, 1.0f - phys.linearDamping * safeDt * 60.0f);

    // === 4. SPEED CLAMP ===
    float speed = glm::length(phys.velocity);
    if (speed > MAX_GODBALL_SPEED) {
        phys.velocity = (phys.velocity / speed) * MAX_GODBALL_SPEED;
    }

    // === 5. INTEGRATE POSITION ===
    phys.position += phys.velocity * safeDt;

    // === 6. ROPE CONSTRAINT (POSITIONAL CORRECTION) ===
    glm::vec3 toHand = handPos - phys.position;
    float dist = glm::length(toHand);
    phys.constraintDist = dist;
    phys.ropeTension = 0.0f;

    if (dist > phys.ropeLength && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float error = dist - phys.ropeLength;
        phys.ropeTension = error * phys.ropeStiffness;

        // Positional correction: pull ball back onto constraint sphere
        phys.position += dir * error;

        // Velocity correction: remove outward radial component,
        // preserve tangential component for orbiting.
        float radialVel = glm::dot(phys.velocity, dir);
        if (radialVel > 0.0f) {
            phys.velocity -= dir * radialVel;
            phys.velocity -= dir * radialVel * 0.15f;
        }
    }

    // === 7. SOFT PUSH FROM PLAYER BODY ===
    // Prevent ball from clipping through the player without hard snapping.
    glm::vec3 ballToOwner = phys.position - owner.pos;
    float ownerDist = glm::length(ballToOwner);
    float minOwnerDist = 0.7f;
    if (ownerDist < minOwnerDist && ownerDist > 0.001f) {
        glm::vec3 pushDir = ballToOwner / ownerDist;
        phys.position = owner.pos + pushDir * minOwnerDist;
        float velToward = glm::dot(phys.velocity, pushDir);
        if (velToward < 0.0f) {
            phys.velocity -= pushDir * velToward;
        }
    }

    // === 8. STORE STATE ===
    runtime.godball.ballPosition = phys.position;
    runtime.godball.ballVelocity = phys.velocity;

    printf("[GODBALL] pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) speed=%.2f dist=%.2f tension=%.1f\n",
           phys.position.x, phys.position.y, phys.position.z,
           phys.velocity.x, phys.velocity.y, phys.velocity.z,
           speed, dist, phys.ropeTension);
}

void checkOverlaps(GodballPhysics& phys, const WeaponDefinition& def,
                    WeaponRuntime& runtime, Player& owner,
                    NpcSystem& npcs, const Camera& camera, float dt) {
    if (!phys.active) return;

    float safeDt = std::min(dt, 0.05f);
    float tickInterval = def.customParams.count("damageTickInterval")
        ? def.customParams.at("damageTickInterval") : 0.1f;

    runtime.godball.overlapDamageTimer -= safeDt;
    if (runtime.godball.overlapDamageTimer > 0.0f) {
        // Still decay cooldowns even when timer hasn't ticked
        for (auto& pair : runtime.godball.targetCooldowns) {
            if (pair.second > 0.0f) pair.second -= safeDt;
        }
        return;
    }
    runtime.godball.overlapDamageTimer = tickInterval;

    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        uint32_t npcId = npc.id;

        auto& cooldowns = runtime.godball.targetCooldowns;
        auto cooldownIt = cooldowns.find(npcId);
        if (cooldownIt != cooldowns.end() && cooldownIt->second > 0.0f) {
            continue;
        }

        glm::vec3 npcCenter = npc.body.pos;
        glm::vec3 toNPC = npcCenter - phys.position;
        float npcDist = glm::length(toNPC);
        float overlapDist = phys.radius + 0.5f;

        if (npcDist < overlapDist) {
            float damage = computeDamage(phys, def, owner, npc.body,
                phys.position + (toNPC / std::max(npcDist, 0.001f)) * phys.radius);

            int rounded = std::max(1, (int)std::round(damage));

            // Apply damage to NPC body via takeDamage for consistency
            glm::vec3 knockbackDir = glm::normalize(phys.velocity);
            if (glm::length(knockbackDir) < 0.001f)
                knockbackDir = glm::vec3(0.0f, 1.0f, 0.0f);
            float knockbackForce = damage * 0.02f;

            npc.body.currentHp = std::max(0, npc.body.currentHp - rounded);
            npc.body.vel += knockbackDir * knockbackForce + glm::vec3(0, 0, knockbackForce * 0.3f);
            npc.hitReactionTimer = 0.2f;

            printf("[GODBALL OVERLAP] npc=%u damage=%d vel=%.1f\n",
                   npcId, rounded, glm::length(phys.velocity));

            // === BLOOD EFFECTS ===
            glm::vec3 hitPos = phys.position;
            float intensity = std::min((float)rounded / 30.0f, 1.5f);

            // Damage popup
            EffectPartSystem::instance().spawnDamage(
                hitPos + glm::vec3(0, 0, 0.5f),
                npc.body.username, rounded);

            // Blood sphere burst (scaled by damage)
            EffectPartSystem::instance().spawnBloodSphereBurst(
                hitPos, knockbackDir, intensity,
                owner.username, "npc_" + std::to_string(npcId));

            // Blood spurt along velocity direction
            EffectPartSystem::instance().spawnBloodSpurt(
                hitPos, knockbackDir,
                owner.username, "npc_" + std::to_string(npcId));

            // Entity impact effect
            EffectPartSystem::instance().spawnEntityImpact(
                hitPos, knockbackDir,
                owner.username, "npc_" + std::to_string(npcId));

            // Set cooldown
            cooldowns[npcId] = tickInterval;
            hitmarker();

            // Death
            if (npc.body.currentHp <= 0) {
                DeathSystem::instance().kill(
                    npc.body,
                    "npc_" + std::to_string(npcId),
                    "npc",
                    owner.username,
                    knockbackDir,
                    8.0f + glm::length(phys.velocity) * 0.15f);
                std::string line = owner.username + " killed " + npc.body.username + " with Godball";
                Terminal::instance().addLog(line);
            }
        }
    }

    // Decay cooldowns
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

    printf("[GODBALL DAMAGE] speed=%.1f speedMult=%.2f angle=%.2f relMult=%.2f swing=%.2f total=%.1f\n",
           ballSpeed, speedMultiplier, angleFactor, relativeMultiplier, swingBonus, totalDamage);

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

    // Rope line (yellow, visible even without debug mode)
    DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 0.8f});

    // Tether anchor at hand
    DebugVis::drawWireSphere(camera, handPos, 0.1f, {1.0f, 1.0f, 0.0f, 1.0f});

    // Velocity vector (magenta)
    if (speed > 0.1f) {
        glm::vec3 velEnd = phys.position + glm::normalize(phys.velocity) * std::min(speed * 0.3f, 5.0f);
        DebugVis::drawLine(camera, phys.position, velEnd, {1.0f, 0.0f, 1.0f, 1.0f});
    }

    // Overlap sphere (cyan, translucent)
    DebugVis::drawWireSphere(camera, phys.position, phys.radius + 0.5f, {0.0f, 1.0f, 1.0f, 0.4f});

    // Labels
    char label[128];
    snprintf(label, sizeof(label), "GODBALL %.1f m/s  T=%.1f  D=%.2f/%.1f",
             speed, phys.ropeTension, phys.constraintDist, phys.ropeLength);
    DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 0.5f),
                              label, {1.0f, 1.0f, 1.0f, 1.0f});

    // Overlap target cooldown labels
    for (const auto& pair : runtime.godball.targetCooldowns) {
        if (pair.second > 0.0f) {
            char cdLabel[32];
            snprintf(cdLabel, sizeof(cdLabel), "CD:%.2f", pair.second);
            DebugVis::drawWorldLabel(phys.position + glm::vec3(0, 0, phys.radius + 1.0f),
                                      cdLabel, {0.0f, 1.0f, 0.0f, 1.0f});
        }
    }
}

} // namespace WeaponGodball