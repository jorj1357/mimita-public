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

static constexpr float MAX_GODBALL_SPEED = 120.0f;

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
    phys.angularVelocity = glm::vec3(0.0f);
    phys.active = true;
    phys.radius = def.customParams.count("ballRadius") ? def.customParams.at("ballRadius") : 0.5f;
    phys.mass = def.customParams.count("ballMass") ? def.customParams.at("ballMass") : 0.1f;
    phys.ropeLength = def.customParams.count("ropeLength") ? def.customParams.at("ropeLength") : 2.5f;
    phys.ropeStiffness = def.customParams.count("ropeStiffness") ? def.customParams.at("ropeStiffness") : 50.0f;
    phys.ropeDamping = def.customParams.count("ropeDamping") ? def.customParams.at("ropeDamping") : 2.0f;
    phys.linearDamping = def.customParams.count("linearDamping") ? def.customParams.at("linearDamping") : 0.005f;
    phys.ropeTension = 0.0f;
    phys.constraintDist = 0.0f;
    phys.prevHandPos = handPos;
    phys.hasPrevHandPos = true;
    phys.handedEnergy = 0.0f;
    phys.radialVel = 0.0f;
    phys.tangentialSpeed = 0.0f;

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

    // === 2. GRAVITY ===
    constexpr float GRAVITY = 9.81f;
    phys.velocity.z -= GRAVITY * safeDt;

    // === 3. MOVEMENT ENERGY INJECTION — whip / sling / slingshot ===
    // Hand movement directly injects momentum into the ball.
    // Perpendicular (whip) component creates orbital swing.
    // Parallel pull component accelerates ball along rope.
    if (handSpeed > 0.3f) {
        glm::vec3 toBall = phys.position - handPos;
        float distToBall = glm::length(toBall);
        if (distToBall > 0.1f) {
            glm::vec3 ropeDir = toBall / distToBall;
            float parallel = glm::dot(handVel, ropeDir);
            glm::vec3 perp = handVel - ropeDir * parallel;
            float perpSpeed = glm::length(perp);

            // Whip injection — hand moving sideways whips the ball around
            if (perpSpeed > 0.3f) {
                float scale = std::min(perpSpeed * 0.04f, 1.0f);
                float factor = 0.6f * scale;
                phys.velocity += (perp / perpSpeed) * perpSpeed * factor;
            }

            // Pull injection — hand moving away from ball pulls it along
            float pull = -parallel;
            if (pull > 0.0f) {
                float scale = std::min(pull * 0.04f, 1.0f);
                float factor = 0.35f * scale;
                phys.velocity += ropeDir * pull * factor;
            }
        }
    }

    // === 4. DAMPING (essentially zero — momentum forever) ===
    phys.velocity *= std::pow(1.0f - phys.linearDamping, safeDt * 60.0f);

    // === 5. VELOCITY CONSTRAINT (soft — allows overshoot) ===
    // Remove most outward velocity but keep some for energetic overshoot.
    glm::vec3 toHand = handPos - phys.position;
    float dist = glm::length(toHand);

    if (dist >= phys.ropeLength * 0.95f && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float outwardVel = glm::dot(phys.velocity, dir);
        if (outwardVel > 0.0f) {
            phys.velocity -= dir * outwardVel * 0.7f;
        }
    }

    // === 6. INTEGRATE POSITION (semi-implicit Euler) ===
    phys.position += phys.velocity * safeDt;

    // === 7. POSITION CONSTRAINT (hard snap to rope sphere) ===
    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    phys.constraintDist = dist;

    if (dist > phys.ropeLength && dist > 0.001f) {
        glm::vec3 dir = toHand / dist;
        float error = dist - phys.ropeLength;
        phys.ropeTension = error * phys.ropeStiffness;
        phys.position = handPos - dir * phys.ropeLength;
    } else {
        phys.ropeTension = 0.0f;
    }

    // === 8. ANGULAR VELOCITY (from orbital motion) ===
    toHand = handPos - phys.position;
    dist = glm::length(toHand);
    if (dist > 0.1f) {
        glm::vec3 ropeDir = toHand / dist;
        phys.angularVelocity = glm::cross(ropeDir, phys.velocity) / dist;
        float angSpeed = glm::length(phys.angularVelocity);
        constexpr float MAX_ANG_SPEED = 80.0f;
        if (angSpeed > MAX_ANG_SPEED) {
            phys.angularVelocity = (phys.angularVelocity / angSpeed) * MAX_ANG_SPEED;
        }
    } else {
        phys.angularVelocity = glm::vec3(0.0f);
    }

    // === 9. SPEED CLAMP ===
    float speed = glm::length(phys.velocity);
    if (speed > MAX_GODBALL_SPEED) {
        phys.velocity = (phys.velocity / speed) * MAX_GODBALL_SPEED;
    }

    // === 10. PLAYER BODY AVOIDANCE ===
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

    // === 11. STORE STATE ===
    runtime.godball.ballPosition = phys.position;
    runtime.godball.ballVelocity = phys.velocity;

    // Debug fields
    phys.handedEnergy = handSpeed;
    phys.radialVel = 0.0f;
    phys.tangentialSpeed = 0.0f;
    if (dist > 0.1f) {
        glm::vec3 ropeDir = (handPos - phys.position) / dist;
        glm::vec3 radialComp = ropeDir * glm::dot(phys.velocity, ropeDir);
        phys.tangentialSpeed = glm::length(phys.velocity - radialComp);
        phys.radialVel = glm::dot(ropeDir, phys.velocity);
    } else {
        phys.tangentialSpeed = speed;
    }
    phys.constraintDist = dist;

    // === 12. DEBUG LOG ===
    printf("[GODBALL] speed=%.2f tang=%.2f radial=%.2f "
           "dist=%.2f/%.1f tension=%.1f handIn=%.1f\n",
           speed, phys.tangentialSpeed, phys.radialVel,
           phys.constraintDist, phys.ropeLength, phys.ropeTension,
           phys.handedEnergy);
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
            glm::vec3 hitPos = npc.body.pos + glm::vec3(0, 0, 0.8f);
            float ballSpeed = glm::length(phys.velocity);
            float intensity = std::min((float)rounded / 20.0f, 2.0f);

            // Damage popup at NPC position (not ball position)
            EffectPartSystem::instance().spawnDamage(
                hitPos,
                npc.body.username, rounded);

            // Blood sphere burst (scaled by damage and speed)
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

            printf("[GODBALL HIT] target=%s damage=%d vel=%.1f speedMult=%.2f\n",
                   npc.body.username.c_str(), rounded, ballSpeed,
                   1.0f + (ballSpeed / 10.0f) * 3.0f);

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

    // Rope line (yellow)
    DebugVis::drawLine(camera, handPos, phys.position, {1.0f, 1.0f, 0.0f, 0.8f});

    // Tether anchor at hand
    DebugVis::drawWireSphere(camera, handPos, 0.1f, {1.0f, 1.0f, 0.0f, 1.0f});

    // Velocity vector (magenta)
    if (speed > 0.1f) {
        glm::vec3 velEnd = phys.position + glm::normalize(phys.velocity) * std::min(speed * 0.3f, 5.0f);
        DebugVis::drawLine(camera, phys.position, velEnd, {1.0f, 0.0f, 1.0f, 1.0f});
    }

    // Angular velocity axis (cyan)
    float angSpeed = glm::length(phys.angularVelocity);
    if (angSpeed > 0.1f) {
        glm::vec3 angEnd = phys.position + glm::normalize(phys.angularVelocity) * std::min(angSpeed * 0.1f, 1.0f);
        DebugVis::drawLine(camera, phys.position, angEnd, {0.0f, 1.0f, 1.0f, 0.8f});
    }

    // Tangential / swing direction (green arrow for orbital visualization)
    if (speed > 0.5f && phys.tangentialSpeed > 0.5f) {
        glm::vec3 toHandDir = glm::normalize(handPos - phys.position);
        glm::vec3 swingDir = glm::normalize(phys.velocity - toHandDir * glm::dot(phys.velocity, toHandDir));
        float arrowLen = std::min(phys.tangentialSpeed * 0.3f, 3.0f);
        glm::vec3 swingEnd = phys.position + swingDir * arrowLen;
        DebugVis::drawLine(camera, phys.position, swingEnd, {0.0f, 1.0f, 0.0f, 0.9f});
    }

    // Overlap sphere (cyan, translucent)
    DebugVis::drawWireSphere(camera, phys.position, phys.radius + 0.5f, {0.0f, 1.0f, 1.0f, 0.4f});

    // Labels
    char label[192];
    float angSpd = glm::length(phys.angularVelocity);
    snprintf(label, sizeof(label),
             "GODBALL %.1f m/s  T=%.1f  D=%.2f/%.1f  W=%.1f  "
             "tan=%.1f  inj=%.1f",
             speed, phys.ropeTension, phys.constraintDist, phys.ropeLength, angSpd,
             phys.tangentialSpeed, phys.handedEnergy);
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
