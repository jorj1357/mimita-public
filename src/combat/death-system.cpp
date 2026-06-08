#include "combat/death-system.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <glad/glad.h>
#include "config.h"
#include "debug/debug-visuals.h"
#include "debug/gl-debug.h"
#include "camera.h"
#include "audio/audio.h"
#include "input/input-state.h"
#include "npc/npc.h"
#include "physics/physics-mini.h"
#include "physics/movement/physics-collision.h"
#include "render/render-player.h"
#include "renderer/renderer.h"
#include "replay/replay.h"
#include "world/texture-store.h"
#include "world/world.h"

extern Renderer* gRenderer;

namespace {
constexpr float RESPAWN_SECONDS = 3.0f;
constexpr float CORPSE_STAGE1_SECONDS = 5.0f;
constexpr float CORPSE_STAGE2_SECONDS = 1.0f;
constexpr float CORPSE_TOTAL_SECONDS = 6.0f;

// Ragdoll physics tuning
constexpr float RAGDOLL_GRAVITY = 15.0f;
constexpr float RAGDOLL_DRAG = 0.8f;
constexpr float RAGDOLL_ANGULAR_DRAG = 4.0f;
constexpr float RAGDOLL_BOUNCE = 0.03f;
constexpr float RAGDOLL_FRICTION = 0.4f;
constexpr float RAGDOLL_MAX_LINEAR_VELOCITY = 20.0f;
constexpr float RAGDOLL_MAX_ANGULAR_VELOCITY = 15.0f;
constexpr float RAGDOLL_MAX_CONSTRAINT_IMPULSE = 30.0f;
constexpr float RAGDOLL_CONSTRAINT_STIFFNESS = 0.25f;
constexpr float RAGDOLL_CONSTRAINT_DAMPING = 0.9f;
constexpr float RAGDOLL_SLEEP_VELOCITY = 0.15f;
constexpr float RAGDOLL_SLEEP_ANGULAR = 0.08f;
constexpr float RAGDOLL_SLEEP_TIME = 0.5f;
constexpr float RAGDOLL_WORLD_FLOOR = -500.0f;

#define RAGDOLL_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[RAGDOLL] " fmt "\n", ##__VA_ARGS__); } while(0)

#define CONSTRAINT_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[CONSTRAINT] " fmt "\n", ##__VA_ARGS__); } while(0)

#define PENETRATION_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[PENETRATION] " fmt "\n", ##__VA_ARGS__); } while(0)

#define SELFCOLLISION_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[SELF COLLISION] " fmt "\n", ##__VA_ARGS__); } while(0)

void emitLifecycleEvent(const char* type,
                        const Player& actor,
                        const std::string& actorId,
                        const std::string& otherActorId)
{
    ReplayEffectEvent event;
    event.type = type;
    event.position = actor.pos;
    event.direction = actor.vel;
    event.sourceActorId = otherActorId;
    event.targetActorId = actorId;
    captureReplayEffect(event);
}
}

DeathSystem& DeathSystem::instance()
{
    static DeathSystem system;
    return system;
}

static int findPartIndex(const std::vector<RagdollPart>& parts, const std::string& name) {
    for (int i = 0; i < (int)parts.size(); ++i) {
        if (parts[i].name == name) return i;
    }
    return -1;
}

bool DeathSystem::kill(
    Player& victim,
    const std::string& actorId,
    const std::string& actorType,
    const std::string& killer,
    const glm::vec3& shotDirection,
    float lethalForce)
{
    if (victim.dead)
        return false;

    glm::vec3 direction = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    victim.updateModelWorldTransforms();

    glm::vec3 victimPos = victim.pos;
    glm::vec3 linearVel = victim.vel;
    glm::vec3 externalVel = victim.externalImpulse;

    printf("[RAGDOLL TRANSITION] alivePos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) externalImpulse=(%.2f %.2f %.2f) shotDir=(%.2f %.2f %.2f) force=%.1f\n",
           victimPos.x, victimPos.y, victimPos.z,
           linearVel.x, linearVel.y, linearVel.z,
           externalVel.x, externalVel.y, externalVel.z,
           direction.x, direction.y, direction.z,
           lethalForce);

    RagdollCorpse corpse;
    corpse.id = actorId + "_corpse_" + std::to_string(++mCorpseSerial);
    corpse.name = victim.username + " corpse";
    corpse.partMeshes = victim.physicalBody.partMeshes;
    corpse.spawnPosition = victimPos;
    corpse.transferredVelocity = linearVel;
    corpse.deathImpulse = direction * lethalForce;

    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        const auto& src = victim.physicalBody.parts[i];
        RagdollPart part;
        part.name = src.name;

        part.position = glm::vec3(src.worldTransform[3]);

        glm::vec3 size = src.collider.localMax - src.collider.localMin;
        part.radius = std::max({size.x, size.y, size.z}) * 0.5f;
        part.radius = std::max(part.radius, 0.1f);

        float vol = size.x * size.y * size.z;
        part.mass = std::max(vol * 100.0f, 0.5f);

        part.rotation = glm::quat_cast(src.worldTransform);

        glm::vec3 impulse = linearVel + externalVel + direction * lethalForce;

        glm::vec3 offset = part.position - victimPos;
        float offsetDist = glm::length(offset);
        if (offsetDist > 0.001f) {
            glm::vec3 offsetDir = offset / offsetDist;
            float alignment = glm::max(0.0f, glm::dot(direction, offsetDir));
            impulse += direction * lethalForce * alignment * 0.3f;
        }

        part.velocity = impulse;

        part.angularVelocity = glm::cross(direction * lethalForce * 0.3f, offset);
        float angSpeed = glm::length(part.angularVelocity);
        if (angSpeed > 20.0f) {
            part.angularVelocity = (part.angularVelocity / angSpeed) * 20.0f;
        }

        part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);

        corpse.parts.push_back(std::move(part));
    }

    if (corpse.parts.empty()) {
        RagdollPart root;
        root.name = "root";
        root.position = victimPos;
        root.velocity = linearVel + externalVel + direction * lethalForce;
        root.radius = 0.5f;
        root.mass = 10.0f;
        root.worldTransform = glm::translate(glm::mat4(1.0f), root.position);
        corpse.parts.push_back(std::move(root));
    }

    int torsoIdx = findPartIndex(corpse.parts, "torso");
    if (torsoIdx >= 0) {
        static const char* limbNames[] = {"head", "leftArm", "rightArm", "leftLeg", "rightLeg"};
        for (const char* limbName : limbNames) {
            int limbIdx = findPartIndex(corpse.parts, limbName);
            if (limbIdx >= 0) {
                RagdollConstraint c;
                c.partA = torsoIdx;
                c.partB = limbIdx;
                c.restDist = glm::length(corpse.parts[limbIdx].position - corpse.parts[torsoIdx].position);
                if (c.restDist > 0.01f) {
                    corpse.constraints.push_back(c);
                }
            }
        }
    }

    emitLifecycleEvent("death", victim, actorId, killer);

    victim.pos = glm::vec3(0.0f, 0.0f, -1000.0f);
    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.onGround = false;
    victim.currentHp = 0;
    victim.dead = true;
    victim.respawnTimer = RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;
    victim.syncLegacyStateToLayers();

    printf("[RAGDOLL SPAWN] ragdollPos=(%.2f %.2f %.2f) parts=%zu impulse=(%.2f %.2f %.2f)\n",
           corpse.spawnPosition.x, corpse.spawnPosition.y, corpse.spawnPosition.z,
           corpse.parts.size(),
           corpse.deathImpulse.x, corpse.deathImpulse.y, corpse.deathImpulse.z);

    if (!corpse.parts.empty()) {
        printf("[RAGDOLL IMPULSE] rootVel=(%.2f %.2f %.2f) rootAngVel=(%.2f %.2f %.2f)\n",
               corpse.parts[0].velocity.x, corpse.parts[0].velocity.y, corpse.parts[0].velocity.z,
               corpse.parts[0].angularVelocity.x, corpse.parts[0].angularVelocity.y, corpse.parts[0].angularVelocity.z);
    }

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victimPos, 1.0f, 0.9f, 45.0f, 0});

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = victimPos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = linearVel;
    corpseEvent.sourceActorId = actorId;
    corpseEvent.targetActorId = corpse.id;
    captureReplayEffect(corpseEvent);

    mCorpses.push_back(std::move(corpse));

    return true;
}

void DeathSystem::respawn(Player& actor, const std::string& actorId, const World& world)
{
    SpawnPoint* sp = const_cast<World&>(world).pickSpawnPoint();
    if (sp) {
        actor.pos = sp->position;
        actor.respawnPosition = sp->position;
    } else {
        actor.pos = actor.respawnPosition;
    }

    actor.vel = glm::vec3(0.0f);
    actor.externalImpulse = glm::vec3(0.0f);
    actor.currentHp = actor.maxHp;
    actor.dead = false;
    actor.proceduralFrozen = false;
    actor.respawnTimer = 0.0f;
    actor.killedBy.clear();
    actor.onGround = false;
    actor.syncLegacyStateToLayers();
    actor.updateModelWorldTransforms();
    emitLifecycleEvent("respawn", actor, actorId, actorId);
}

glm::vec3 DeathSystem::closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d5 >= 0.0f && d6 >= 0.0f) return c;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + v * ab + w * ac;
}

static void enforceConstraint(RagdollPart& a, RagdollPart& b, float restDist, float dt) {
    glm::vec3 delta = b.position - a.position;
    float dist = glm::length(delta);
    if (dist < 0.001f) return;

    float error = dist - restDist;
    if (std::fabs(error) < 0.005f) return;

    glm::vec3 dir = delta / dist;
    float totalMass = a.mass + b.mass;
    if (totalMass < 0.001f) return;
    float aWeight = b.mass / totalMass;
    float bWeight = a.mass / totalMass;

    // Soft positional correction — reduced stiffness to prevent jitter
    float correction = error * RAGDOLL_CONSTRAINT_STIFFNESS;
    glm::vec3 posCorr = dir * correction;
    a.position += posCorr * aWeight;
    b.position -= posCorr * bWeight;

    // Velocity damping along constraint axis (both expansion and compression)
    glm::vec3 relVel = b.velocity - a.velocity;
    float radialVel = glm::dot(relVel, dir);
    glm::vec3 impulse = dir * radialVel * RAGDOLL_CONSTRAINT_DAMPING;

    // Clamp impulse to prevent explosive correction
    float impulseMag = glm::length(impulse);
    if (impulseMag > RAGDOLL_MAX_CONSTRAINT_IMPULSE) {
        impulse = (impulse / impulseMag) * RAGDOLL_MAX_CONSTRAINT_IMPULSE;
    }

    a.velocity += impulse * aWeight;
    b.velocity -= impulse * bWeight;

    CONSTRAINT_LOG("error=%.3f correction=%.3f impulse=%.1f dist=%.2f rest=%.2f",
                   error, glm::length(posCorr), glm::length(impulse), dist, restDist);
}

void DeathSystem::clampVelocities(RagdollPart& part)
{
    float speed = glm::length(part.velocity);
    if (speed > RAGDOLL_MAX_LINEAR_VELOCITY) {
        RAGDOLL_LOG("Clamping linear velocity %.1f -> %.1f for '%s'",
                    speed, RAGDOLL_MAX_LINEAR_VELOCITY, part.name.c_str());
        part.velocity = (part.velocity / speed) * RAGDOLL_MAX_LINEAR_VELOCITY;
    }

    float angSpeed = glm::length(part.angularVelocity);
    if (angSpeed > RAGDOLL_MAX_ANGULAR_VELOCITY) {
        RAGDOLL_LOG("Clamping angular velocity %.1f -> %.1f for '%s'",
                    angSpeed, RAGDOLL_MAX_ANGULAR_VELOCITY, part.name.c_str());
        part.angularVelocity = (part.angularVelocity / angSpeed) * RAGDOLL_MAX_ANGULAR_VELOCITY;
    }
}

void DeathSystem::updateRagdollPhysics(RagdollPart& part, const World& world, float dt)
{
    float safeDt = std::min(dt, 0.033f);

    const auto& triangles = world.collisionMesh.triangles;

    // --- STEP 0: Pre-penetration resolution ---
    // Push part out of world BEFORE integration to prevent spawn-in-floor jitter.
    // Use multiple passes for deep penetrations.
    for (int ppPass = 0; ppPass < 3; ++ppPass) {
        for (const auto& tri : triangles) {
            glm::vec3 closest = closestPointOnTriangle(part.position, tri.a, tri.b, tri.c);
            glm::vec3 diff = part.position - closest;
            float dist = glm::length(diff);
            if (dist < part.radius && dist > 0.0001f) {
                float penetration = (part.radius - dist) + 0.02f;
                glm::vec3 normal = diff / dist;
                part.position += normal * penetration;
                float vDotN = glm::dot(part.velocity, -normal);
                if (vDotN < 0.0f) {
                    part.velocity -= vDotN * (-normal) * 1.2f;
                }
            }
        }
    }

    // Clamp velocities BEFORE integration
    clampVelocities(part);

    // Gravity
    part.velocity.z -= RAGDOLL_GRAVITY * safeDt;

    // Linear drag (higher to kill vibration)
    part.velocity *= std::max(0.0f, 1.0f - RAGDOLL_DRAG * safeDt);

    // Angular velocity damping
    float angSpeed = glm::length(part.angularVelocity);
    if (angSpeed > 0.001f) {
        part.angularVelocity *= std::max(0.0f, 1.0f - RAGDOLL_ANGULAR_DRAG * safeDt);
        glm::quat deltaRot = glm::angleAxis(
            std::min(angSpeed * safeDt, 0.5f),
            part.angularVelocity / angSpeed);
        part.rotation = glm::normalize(deltaRot * part.rotation);
    }

    // Clamp velocities AFTER integration too (safety net)
    clampVelocities(part);

    // Integrate position
    glm::vec3 move = part.velocity * safeDt;
    float moveLen = glm::length(move);

    // World collision during movement
    if (moveLen > 0.0001f) {
        glm::vec3 moveDir = move / moveLen;
        float remaining = moveLen;
        constexpr int MAX_STEPS = 4;

        for (int step = 0; step < MAX_STEPS && remaining > 0.0001f; ++step) {
            float stepDist = std::min(remaining, part.radius * 0.5f);
            glm::vec3 newPos = part.position + moveDir * stepDist;

            bool hit = false;
            float bestPenetration = 0.0f;
            glm::vec3 bestNormal(0.0f);

            for (const auto& tri : triangles) {
                glm::vec3 closest = closestPointOnTriangle(newPos, tri.a, tri.b, tri.c);
                glm::vec3 diff = newPos - closest;
                float dist = glm::length(diff);

                if (dist < part.radius) {
                    glm::vec3 normal;
                    if (dist > 0.0001f) {
                        normal = diff / dist;
                    } else {
                        normal = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
                    }

                    float penetration = part.radius - dist;
                    if (penetration > bestPenetration) {
                        bestPenetration = penetration;
                        bestNormal = normal;
                        hit = true;
                    }
                }
            }

            if (hit) {
                part.position += bestNormal * bestPenetration;

                float vDotN = glm::dot(part.velocity, bestNormal);
                if (vDotN < 0.0f) {
                    part.velocity -= (1.0f + RAGDOLL_BOUNCE) * vDotN * bestNormal;

                    glm::vec3 tangential = part.velocity - glm::dot(part.velocity, bestNormal) * bestNormal;
                    float tangLen = glm::length(tangential);
                    if (tangLen > 0.001f) {
                        tangential *= std::max(0.0f, 1.0f - RAGDOLL_FRICTION * safeDt);
                        part.velocity = glm::dot(part.velocity, bestNormal) * bestNormal + tangential;
                    }
                }

                remaining *= 0.3f;
            } else {
                part.position = newPos;
                remaining -= stepDist;
            }
        }
    }

    // Clamp velocities again after collision
    clampVelocities(part);

    part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);
}

void DeathSystem::resolveSelfCollisions(RagdollCorpse& corpse)
{
    auto& parts = corpse.parts;
    const auto& constraints = corpse.constraints;

    // Build constrained pair lookup
    std::vector<std::pair<int,int>> constrainedPairs;
    for (const auto& c : constraints) {
        if (c.partA >= 0 && c.partB >= 0) {
            constrainedPairs.emplace_back(c.partA, c.partB);
            constrainedPairs.emplace_back(c.partB, c.partA);
        }
    }

    auto isConstrained = [&](int i, int j) -> bool {
        for (const auto& p : constrainedPairs) {
            if (p.first == i && p.second == j) return true;
        }
        return false;
    };

    for (size_t i = 0; i < parts.size(); ++i) {
        for (size_t j = i + 1; j < parts.size(); ++j) {
            if (isConstrained((int)i, (int)j)) continue;

            const auto& a = parts[i];
            const auto& b = parts[j];

            glm::vec3 delta = b.position - a.position;
            float dist = glm::length(delta);
            float minDist = a.radius + b.radius;

            if (dist < minDist && dist > 0.0001f) {
                float penetration = minDist - dist;
                glm::vec3 dir = delta / dist;
                float totalMass = a.mass + b.mass;
                if (totalMass < 0.001f) continue;
                float aWeight = b.mass / totalMass;
                float bWeight = a.mass / totalMass;

                // Position correction: push apart
                parts[i].position -= dir * penetration * aWeight;
                parts[j].position += dir * penetration * bWeight;

                // Velocity correction: damp relative motion toward each other
                glm::vec3 relVel = b.velocity - a.velocity;
                float radialVel = glm::dot(relVel, dir);
                if (radialVel < 0.0f) {
                    glm::vec3 impulse = dir * radialVel * 0.4f; // 40% restitution
                    parts[i].velocity += impulse * aWeight;
                    parts[j].velocity -= impulse * bWeight;
                }

                SELFCOLLISION_LOG("'%s' <-> '%s' penetration=%.3f dist=%.2f minDist=%.2f",
                                  a.name.c_str(), b.name.c_str(), penetration, dist, minDist);
            }
        }
    }
}

bool DeathSystem::trySleepCorpse(RagdollCorpse& corpse, float dt)
{
    if (corpse.sleeping) return true;

    float maxSpeed = 0.0f;
    float maxAngSpeed = 0.0f;
    for (const auto& part : corpse.parts) {
        maxSpeed = std::max(maxSpeed, glm::length(part.velocity));
        maxAngSpeed = std::max(maxAngSpeed, glm::length(part.angularVelocity));
    }

    if (maxSpeed < RAGDOLL_SLEEP_VELOCITY && maxAngSpeed < RAGDOLL_SLEEP_ANGULAR) {
        corpse.sleepTimer += dt;
        if (corpse.sleepTimer >= RAGDOLL_SLEEP_TIME) {
            corpse.sleeping = true;
            // Zero out all velocities to prevent micro-oscillation
            for (auto& part : corpse.parts) {
                part.velocity = glm::vec3(0.0f);
                part.angularVelocity = glm::vec3(0.0f);
            }
            RAGDOLL_LOG("Corpse '%s' is now sleeping", corpse.id.c_str());
            return true;
        }
    } else {
        corpse.sleepTimer = 0.0f;
    }
    return false;
}

bool DeathSystem::underworldCheck(RagdollCorpse& corpse, float worldFloor)
{
    for (auto& part : corpse.parts) {
        if (part.position.z < worldFloor) {
            RAGDOLL_LOG("[RAGDOLL UNDERWORLD] Corpse '%s' part '%s' at z=%.1f below floor %.1f",
                        corpse.id.c_str(), part.name.c_str(), part.position.z, worldFloor);
            return true;
        }
    }
    return false;
}

void DeathSystem::resolveGroundPenetration(RagdollCorpse& corpse, const World& world)
{
    if (corpse.groundResolved) return;

    const auto& triangles = world.collisionMesh.triangles;
    if (triangles.empty()) {
        corpse.groundResolved = true;
        return;
    }

    RAGDOLL_LOG("Resolving ground penetration for corpse '%s'", corpse.id.c_str());

    for (auto& part : corpse.parts) {
        for (int pass = 0; pass < 5; ++pass) {
            float maxPenetration = 0.0f;
            glm::vec3 resolveDir(0.0f);

            for (const auto& tri : triangles) {
                glm::vec3 closest = closestPointOnTriangle(part.position, tri.a, tri.b, tri.c);
                glm::vec3 diff = part.position - closest;
                float dist = glm::length(diff);

                if (dist < part.radius && dist > 0.0001f) {
                    float penetration = part.radius - dist;
                    if (penetration > maxPenetration) {
                        maxPenetration = penetration;
                        resolveDir = diff / dist;
                    }
                }
            }

            if (maxPenetration > 0.001f) {
                part.position += resolveDir * (maxPenetration + 0.02f);
                PENETRATION_LOG("Part '%s' pass %d: pushed %.3f along (%.2f %.2f %.2f)",
                                part.name.c_str(), pass, maxPenetration + 0.02f,
                                resolveDir.x, resolveDir.y, resolveDir.z);
            } else {
                break;
            }
        }

        // Zero velocity pointing into any surface
        for (const auto& tri : triangles) {
            glm::vec3 closest = closestPointOnTriangle(part.position, tri.a, tri.b, tri.c);
            glm::vec3 diff = part.position - closest;
            float dist = glm::length(diff);

            if (dist < part.radius + 0.05f && dist > 0.0001f) {
                glm::vec3 normal = diff / dist;
                float vDotN = glm::dot(part.velocity, normal);
                if (vDotN < 0.0f) {
                    part.velocity -= vDotN * normal;
                    PENETRATION_LOG("Part '%s' velocity zeroed along normal (%.2f %.2f %.2f)",
                                    part.name.c_str(), normal.x, normal.y, normal.z);
                }
            }
        }

        part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);
    }

    corpse.groundResolved = true;
}

void DeathSystem::update(
    World& world,
    Player& player,
    NpcSystem& npcs,
    bool instantRespawnPressed,
    float dt)
{
    if (player.currentHp <= 0 && !player.dead) {
        kill(player, player.username, "player", "unknown", player.vel, 12.0f);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0 && !npc.body.dead) {
            kill(
                npc.body,
                "npc_" + std::to_string(npc.id),
                "npc",
                "unknown",
                npc.body.vel,
                18.0f);
        }
    }

    for (RagdollCorpse& corpse : mCorpses) {
        corpse.age += dt;

        // --- Underworld safety check ---
        if (underworldCheck(corpse, RAGDOLL_WORLD_FLOOR)) {
            RAGDOLL_LOG("[RAGDOLL UNDERWORLD] Cleaning up corpse '%s'", corpse.id.c_str());
            corpse.age = CORPSE_TOTAL_SECONDS;
            continue;
        }

        if (corpse.age < CORPSE_STAGE1_SECONDS) {
            corpse.blackness = std::clamp(corpse.age / CORPSE_STAGE1_SECONDS, 0.0f, 1.0f);
            corpse.fade = 0.0f;

            // --- Ground penetration fix (first frame only) ---
            resolveGroundPenetration(corpse, world);

            // --- Sleep check ---
            if (trySleepCorpse(corpse, dt)) {
                // Corpse is sleeping; just update transforms
                for (RagdollPart& part : corpse.parts) {
                    part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);
                }
                continue;
            }

            // Step 1: Update individual part physics
            for (RagdollPart& part : corpse.parts) {
                updateRagdollPhysics(part, world, dt);
            }

            // Step 2: Self-collision between non-connected parts
            resolveSelfCollisions(corpse);

            // Step 3: Enforce constraints between parts (multiple iterations for stability)
            constexpr int CONSTRAINT_ITERS = 5;
            float subDt = dt / (float)CONSTRAINT_ITERS;
            for (int iter = 0; iter < CONSTRAINT_ITERS; ++iter) {
                for (const RagdollConstraint& c : corpse.constraints) {
                    if (c.partA >= 0 && c.partA < (int)corpse.parts.size() &&
                        c.partB >= 0 && c.partB < (int)corpse.parts.size()) {
                        enforceConstraint(corpse.parts[c.partA], corpse.parts[c.partB],
                                          c.restDist, subDt);
                    }
                }
            }

            // Step 4: Clamp velocities once more after all corrections
            for (RagdollPart& part : corpse.parts) {
                clampVelocities(part);
            }
        } else {
            corpse.blackness = 1.0f;
            corpse.fade = std::clamp(
                (corpse.age - CORPSE_STAGE1_SECONDS) / CORPSE_STAGE2_SECONDS,
                0.0f, 1.0f);
        }
    }

    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(), [](const RagdollCorpse& corpse) {
            return corpse.age >= CORPSE_TOTAL_SECONDS;
        }),
        mCorpses.end());

    if (player.dead) {
        player.respawnTimer = std::max(0.0f, player.respawnTimer - dt);
        if (instantRespawnPressed || player.respawnTimer <= 0.0f)
            respawn(player, player.username, world);
    }

    for (Npc& npc : npcs.all()) {
        if (!npc.body.dead) continue;
        npc.body.respawnTimer = std::max(0.0f, npc.body.respawnTimer - dt);
        if (npc.body.respawnTimer <= 0.0f)
            respawn(npc.body, "npc_" + std::to_string(npc.id), world);
    }
}

void DeathSystem::render(const Camera& camera) const
{
    if (mCorpses.empty() || !gRenderer)
        return;

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    GLuint shader = gRenderer->shaderProgram;

    for (const RagdollCorpse& corpse : mCorpses) {
        if (corpse.fade >= 1.0f)
            continue;

        for (size_t i = 0; i < corpse.parts.size() && i < corpse.partMeshes.size(); ++i) {
            const RagdollPart& part = corpse.parts[i];
            const Mesh& mesh = corpse.partMeshes[i];
            if (mesh.verts.empty())
                continue;

            uploadBodyPartMesh(mesh);

            MIMITA_GL_CALL(glUseProgram(shader));
            glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &proj[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &part.worldTransform[0][0]);
            glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
            glUniform1i(glGetUniformLocation(shader, "uTex"), 0);

            glActiveTexture(GL_TEXTURE0);
            for (const Mesh::Batch& batch : mesh.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
            }
        }

        // Debug rendering for ragdoll constraints and death data
        if (DebugVis::enabled() || DebugConfig::DEBUG_RAGDOLL) {
            if (corpse.age < 0.5f) {
                glm::vec3 spawnEnd = corpse.spawnPosition + glm::vec3(0.0f, 0.0f, 0.5f);
                DebugVis::drawWireSphere(camera, corpse.spawnPosition, 0.15f, {1.0f, 1.0f, 0.0f, 1.0f});

                if (glm::length(corpse.deathImpulse) > 0.01f) {
                    float impLen = glm::length(corpse.deathImpulse);
                    glm::vec3 impEnd = corpse.spawnPosition + glm::normalize(corpse.deathImpulse) * std::min(impLen * 0.05f, 3.0f);
                    DebugVis::drawLine(camera, corpse.spawnPosition, impEnd, {1.0f, 0.0f, 0.0f, 1.0f});
                }

                if (glm::length(corpse.transferredVelocity) > 0.1f) {
                    float velLen = glm::length(corpse.transferredVelocity);
                    glm::vec3 velEnd = corpse.spawnPosition + glm::normalize(corpse.transferredVelocity) * std::min(velLen * 0.05f, 3.0f);
                    DebugVis::drawLine(camera, corpse.spawnPosition, velEnd, {0.0f, 0.5f, 1.0f, 1.0f});
                }
            }

            for (const RagdollConstraint& c : corpse.constraints) {
                if (c.partA >= 0 && c.partA < (int)corpse.parts.size() &&
                    c.partB >= 0 && c.partB < (int)corpse.parts.size()) {
                    const RagdollPart& a = corpse.parts[c.partA];
                    const RagdollPart& b = corpse.parts[c.partB];
                    DebugVis::drawLine(camera, a.position, b.position, {0.0f, 1.0f, 0.0f, 0.7f});
                    DebugVis::drawWireSphere(camera, a.position, 0.08f, {0.0f, 1.0f, 0.0f, 1.0f});
                    DebugVis::drawWireSphere(camera, b.position, 0.08f, {1.0f, 0.5f, 0.0f, 1.0f});
                    float aSpeed = glm::length(a.velocity);
                    if (aSpeed > 0.5f) {
                        glm::vec3 velEnd = a.position + glm::normalize(a.velocity) * std::min(aSpeed * 0.1f, 2.0f);
                        DebugVis::drawLine(camera, a.position, velEnd, {1.0f, 0.0f, 1.0f, 0.6f});
                    }
                }
            }
        }

            // Additional debug visuals when DEBUG_RAGDOLL is on
            if (DebugConfig::DEBUG_RAGDOLL) {
                for (const RagdollPart& part : corpse.parts) {
                    DebugVis::drawWireSphere(camera, part.position, part.radius, {0.0f, 0.5f, 1.0f, 0.3f});
                    DebugVis::drawPointCross(camera, part.position, 0.04f, {1.0f, 1.0f, 0.0f, 0.8f});
                    float speed = glm::length(part.velocity);
                    if (speed > 0.1f) {
                        glm::vec3 velEnd = part.position + glm::normalize(part.velocity) * std::min(speed * 0.15f, 3.0f);
                        DebugVis::drawLine(camera, part.position, velEnd, {1.0f, 0.3f, 0.3f, 0.7f});
                    }
                }
            }
    }
}

void DeathSystem::appendReplayActors(std::vector<ReplayActorState>& actors) const
{
    for (const RagdollCorpse& corpse : mCorpses) {
        ReplayActorState actor;
        actor.id = corpse.id;
        actor.name = corpse.name;
        actor.type = "corpse";
        actor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
        if (!corpse.parts.empty()) {
            actor.position = corpse.parts[0].position;
            glm::vec3 euler = glm::eulerAngles(corpse.parts[0].rotation);
            actor.rotation = glm::degrees(euler);
            actor.velocity = corpse.parts[0].velocity;
        }
        actor.health = 0;
        actor.maxHealth = 100;
        actor.grounded = false;
        actor.collidable = true;
        actor.fade = corpse.fade;
        actor.blackness = corpse.blackness;
        actor.animationState = "dead";
        for (const auto& part : corpse.parts) {
            ReplayBodyPartState bp;
            bp.name = part.name;
            bp.position = part.position;
            glm::vec3 euler = glm::eulerAngles(part.rotation);
            bp.rotation = glm::degrees(euler);
            bp.scale = glm::vec3(1.0f);
            actor.bodyParts.push_back(bp);
        }
        actors.push_back(actor);
    }
}
