#include "combat/death-system.h"

#include <algorithm>
#include <cmath>

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
#include "game/duel.h"

extern DuelManager gDuelManager;

extern Renderer* gRenderer;

namespace {
constexpr float RESPAWN_SECONDS = 3.0f;
constexpr float CORPSE_STAGE1_SECONDS = 5.0f;
constexpr float CORPSE_STAGE2_SECONDS = 1.0f;
constexpr float CORPSE_TOTAL_SECONDS = 6.0f;

// Dead body physics tuning
constexpr float DEAD_GRAVITY = 20.0f;
constexpr float DEAD_DRAG = 0.3f;
constexpr float DEAD_ANGULAR_DRAG = 0.5f;
constexpr float DEAD_BOUNCE = 0.15f;
constexpr float DEAD_FRICTION = 0.8f;
constexpr float DEAD_MAX_LINEAR_VELOCITY = 50.0f;
constexpr float DEAD_MAX_ANGULAR_VELOCITY = 25.0f;
constexpr float DEAD_SLEEP_VELOCITY = 0.05f;
constexpr float DEAD_SLEEP_ANGULAR = 0.03f;
constexpr float DEAD_SLEEP_TIME = 0.8f;
constexpr float DEAD_WORLD_FLOOR = -500.0f;
constexpr float DEAD_COLLISION_SKIN = 0.02f;

#define DEAD_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[DEAD] " fmt "\n", ##__VA_ARGS__); } while(0)

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

glm::vec3 closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
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

// Find closest point on a line segment to a point
glm::vec3 closestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 0.0001f) return a;
    float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    return a + ab * t;
}
}

DeathSystem& DeathSystem::instance()
{
    static DeathSystem system;
    return system;
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

    // Step 1: 0.1 second freeze — stop all animation/control
    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.inputWishMove = glm::vec2(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.proceduralFrozen = true;
    victim.syncLegacyStateToLayers();

    // Step 2: disable weapon/aim/procedural pose before capturing final state.
    // Clear arm poses to neutral so the corpse doesn't hold a weapon pose forever.
    for (PhysicalBodyPart& part : victim.physicalBody.parts) {
        if (part.name == "leftArm" || part.name == "rightArm") {
            part.pose = ProceduralPose{};
            part.perfectPose = ProceduralPose{};
            part.translationSpring = SpringState{};
            part.rotationSpring = SpringState{};
        }
    }
    victim.syncLegacyStateToLayers();

    // Capture final body state (neutral pose, no weapon offsets)
    victim.updateModelWorldTransforms();

    glm::vec3 victimPos = victim.pos;
    glm::vec3 linearVel = victim.vel;
    glm::vec3 externalVel = victim.externalImpulse;
    glm::quat victimRotation = glm::angleAxis(glm::radians(victim.yaw), glm::vec3(0.0f, 0.0f, 1.0f));

    printf("[DEATH] victim=%s hitDir=(%.2f %.2f %.2f) damage=%.0f\n",
           victim.username.c_str(), direction.x, direction.y, direction.z, lethalForce);

    DeadBody body;
    body.id = actorId + "_corpse_" + std::to_string(++mCorpseSerial);
    body.name = victim.username + " corpse";
    body.spawnPosition = victimPos;
    body.transferredVelocity = linearVel;
    body.deathImpulse = direction * lethalForce;

    // Single physics body at the player's death position
    body.position = victimPos;
    body.rotation = victimRotation;

    // Capsule dimensions matching the player
    body.capsuleRadius = PLAYER_RADIUS;
    body.capsuleHeight = PLAYER_HEIGHT;

    // Inherit velocity — clamped to prevent launch, preserve momentum feel
    float velMag = glm::length(linearVel + externalVel);
    float maxInherit = 8.0f;
    if (velMag > maxInherit) {
        body.velocity = (linearVel + externalVel) * (maxInherit / velMag);
    } else {
        body.velocity = linearVel + externalVel;
    }

    // Death impulse scales with hit force — flings body backward on strong hits
    body.velocity += direction * (lethalForce * 0.5f);

    // Angular velocity from shot direction — makes corpse spin/tumble naturally
    body.angularVelocity = glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f))
                         * (lethalForce * 0.15f + 0.5f);
    float angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > 15.0f) {
        body.angularVelocity = (body.angularVelocity / angSpeed) * 15.0f;
    }

    // Freeze skeleton pose from current physical body transforms
    body.frozenParts.reserve(victim.physicalBody.parts.size());
    body.partMeshes = victim.physicalBody.partMeshes;
    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        DeadBody::FrozenPart fp;
        fp.name = victim.physicalBody.parts[i].name;
        fp.nodeIndex = victim.physicalBody.parts[i].nodeIndex;
        fp.worldTransform = victim.physicalBody.parts[i].worldTransform;
        body.frozenParts.push_back(std::move(fp));
    }

    emitLifecycleEvent("death", victim, actorId, killer);

    // Step 3 (continued from above): duel tracking and respawn timer
    if (actorType == "player")
    {
        gDuelManager.onEntityDeath(DuelTeam::Player);
    }
    else if (actorType == "npc")
    {
        gDuelManager.onEntityDeath(DuelTeam::NPC);
    }
    victim.respawnTimer = RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;

    printf("[RAGDOLL] player=%s activated=true parts=%zu\n",
           victim.username.c_str(), body.frozenParts.size());
    printf("[RAGDOLL IMPULSE] force=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n",
           body.deathImpulse.x, body.deathImpulse.y, body.deathImpulse.z,
           body.velocity.x, body.velocity.y, body.velocity.z);

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victimPos, 1.0f, 0.9f, 45.0f, 0});

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = victimPos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = linearVel;
    corpseEvent.sourceActorId = actorId;
    corpseEvent.targetActorId = body.id;
    captureReplayEffect(corpseEvent);

    mCorpses.push_back(std::move(body));

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

void DeathSystem::updateDeadBodyPhysics(DeadBody& body, const World& world, float dt)
{
    // Death freeze: skip physics during the initial freeze period
    if (body.deathFreezeTimer > 0.0f) {
        body.deathFreezeTimer -= dt;
        return;
    }

    float safeDt = std::min(dt, 0.033f);
    const auto& triangles = world.collisionMesh.triangles;
    float radius = body.capsuleRadius;

    // Gravity
    body.velocity.z -= DEAD_GRAVITY * safeDt;

    // Linear drag
    body.velocity *= std::max(0.0f, 1.0f - DEAD_DRAG * safeDt);

    // Angular velocity damping
    float angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > 0.001f) {
        body.angularVelocity *= std::max(0.0f, 1.0f - DEAD_ANGULAR_DRAG * safeDt);
        glm::quat deltaRot = glm::angleAxis(
            std::min(angSpeed * safeDt, 0.5f),
            body.angularVelocity / angSpeed);
        body.rotation = glm::normalize(deltaRot * body.rotation);
    }

    // Clamp velocities
    float speed = glm::length(body.velocity);
    if (speed > DEAD_MAX_LINEAR_VELOCITY) {
        body.velocity = (body.velocity / speed) * DEAD_MAX_LINEAR_VELOCITY;
    }
    angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > DEAD_MAX_ANGULAR_VELOCITY) {
        body.angularVelocity = (body.angularVelocity / angSpeed) * DEAD_MAX_ANGULAR_VELOCITY;
    }

    // Integrate position
    glm::vec3 move = body.velocity * safeDt;
    float moveLen = glm::length(move);

    // Collision: step-based sphere-vs-triangle
    if (moveLen > 0.0001f) {
        glm::vec3 moveDir = move / moveLen;
        float remaining = moveLen;
        constexpr int MAX_STEPS = 4;

        for (int step = 0; step < MAX_STEPS && remaining > 0.0001f; ++step) {
            float stepDist = std::min(remaining, radius * 0.5f);
            glm::vec3 newPos = body.position + moveDir * stepDist;

            bool hit = false;
            float bestPenetration = 0.0f;
            glm::vec3 bestNormal(0.0f);

            for (const auto& tri : triangles) {
                glm::vec3 closest = closestPointOnTriangle(newPos, tri.a, tri.b, tri.c);
                glm::vec3 diff = newPos - closest;
                float dist = glm::length(diff);

                if (dist < radius) {
                    glm::vec3 triNormal = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
                    glm::vec3 normal;
                    if (dist > 0.0001f) {
                        float belowPlane = glm::dot(diff, triNormal);
                        normal = (belowPlane < 0.0f) ? triNormal : diff / dist;
                    } else {
                        normal = triNormal;
                    }

                    float penetration = radius - dist;
                    if (penetration > bestPenetration) {
                        bestPenetration = penetration;
                        bestNormal = normal;
                        hit = true;
                    }
                }
            }

            if (hit) {
                body.position += bestNormal * (bestPenetration + DEAD_COLLISION_SKIN);

                float vDotN = glm::dot(body.velocity, bestNormal);
                if (vDotN < 0.0f) {
                    body.velocity -= (1.0f + DEAD_BOUNCE) * vDotN * bestNormal;

                    // Friction on tangential component
                    glm::vec3 tangential = body.velocity - glm::dot(body.velocity, bestNormal) * bestNormal;
                    float tangLen = glm::length(tangential);
                    if (tangLen > 0.001f) {
                        tangential *= std::max(0.0f, 1.0f - DEAD_FRICTION * safeDt);
                        body.velocity = glm::dot(body.velocity, bestNormal) * bestNormal + tangential;
                    }
                }

                remaining *= 0.3f;
            } else {
                body.position = newPos;
                remaining -= stepDist;
            }
        }
    }

    // Post-movement ground proximity check — zero slow downward velocity near surfaces
    for (const auto& tri : triangles) {
        glm::vec3 closest = closestPointOnTriangle(body.position, tri.a, tri.b, tri.c);
        glm::vec3 diff = body.position - closest;
        float dist = glm::length(diff);

        if (dist < radius * 2.0f && dist > 0.0001f) {
            glm::vec3 triNormal = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
            float vDotN = glm::dot(body.velocity, triNormal);
            if (vDotN < 0.0f && vDotN > -1.5f) {
                body.velocity -= vDotN * triNormal;
            }
        }
    }

    // Clamp again after all corrections
    speed = glm::length(body.velocity);
    if (speed > DEAD_MAX_LINEAR_VELOCITY) {
        body.velocity = (body.velocity / speed) * DEAD_MAX_LINEAR_VELOCITY;
    }

    DEAD_LOG("pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) speed=%.2f",
             body.position.x, body.position.y, body.position.z,
             body.velocity.x, body.velocity.y, body.velocity.z,
             speed);
}

bool DeathSystem::trySleepBody(DeadBody& body, float dt)
{
    if (body.sleeping) return true;

    float speed = glm::length(body.velocity);
    float angSpeed = glm::length(body.angularVelocity);

    if (speed < DEAD_SLEEP_VELOCITY && angSpeed < DEAD_SLEEP_ANGULAR) {
        body.sleepTimer += dt;
        if (body.sleepTimer >= DEAD_SLEEP_TIME) {
            body.sleeping = true;
            body.velocity = glm::vec3(0.0f);
            body.angularVelocity = glm::vec3(0.0f);
            DEAD_LOG("Body '%s' sleeping at pos=(%.2f %.2f %.2f)",
                     body.id.c_str(), body.position.x, body.position.y, body.position.z);
            return true;
        }
        // Pre-sleep damping
        if (body.sleepTimer > DEAD_SLEEP_TIME * 0.5f) {
            body.velocity *= 0.9f;
            body.angularVelocity *= 0.9f;
        }
    } else {
        body.sleepTimer = 0.0f;
    }
    return false;
}

void DeathSystem::update(
    World& world,
    Player& player,
    NpcSystem& npcs,
    bool instantRespawnPressed,
    float dt)
{
    // Kill any entity that just reached 0 HP
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

    // Update all dead bodies
    for (DeadBody& body : mCorpses) {
        body.age += dt;

        // Underworld safety
        if (body.position.z < DEAD_WORLD_FLOOR) {
            DEAD_LOG("[UNDERWORLD] Body '%s' at z=%.1f, cleaning up",
                     body.id.c_str(), body.position.z);
            body.age = CORPSE_TOTAL_SECONDS;
            continue;
        }

        if (body.age < CORPSE_STAGE1_SECONDS) {
            body.blackness = std::clamp(body.age / CORPSE_STAGE1_SECONDS, 0.0f, 1.0f);
            body.fade = 0.0f;

            // Sleep check
            if (trySleepBody(body, dt))
                continue;

            // Physics update — single body, no constraints
            updateDeadBodyPhysics(body, world, dt);
        } else {
            body.blackness = 1.0f;
            body.fade = std::clamp(
                (body.age - CORPSE_STAGE1_SECONDS) / CORPSE_STAGE2_SECONDS,
                0.0f, 1.0f);
        }
    }

    // Remove expired corpses
    mCorpses.erase(
        std::remove_if(mCorpses.begin(), mCorpses.end(), [](const DeadBody& body) {
            return body.age >= CORPSE_TOTAL_SECONDS;
        }),
        mCorpses.end());

    // Respawn logic
    if (player.dead)
    {
        player.respawnTimer =
            std::max(0.0f, player.respawnTimer - dt);

        bool duelModeActive =
            gDuelManager.phase() != DuelPhase::Off;

        bool shouldRespawn =
            !duelModeActive;

        if (shouldRespawn)
        {
            if (instantRespawnPressed ||
                player.respawnTimer <= 0.0f)
            {
                respawn(
                    player,
                    player.username,
                    world);
            }
        }
    }

    for (Npc& npc : npcs.all())
    {
        if (!npc.body.dead)
            continue;

        npc.body.respawnTimer =
            std::max(0.0f,
            npc.body.respawnTimer - dt);

        bool duelModeActive =
            gDuelManager.phase() != DuelPhase::Off;

        bool shouldRespawn =
            !duelModeActive;

        if (shouldRespawn)
        {
            if (npc.body.respawnTimer <= 0.0f)
            {
                respawn(
                    npc.body,
                    "npc_" + std::to_string(npc.id),
                    world);
            }
        }
    }
}

void DeathSystem::render(const Camera& camera) const
{
    if (mCorpses.empty() || !gRenderer)
        return;

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    GLuint shader = gRenderer->shaderProgram;

    for (const DeadBody& body : mCorpses) {
        if (body.fade >= 1.0f)
            continue;

        // Build root transform from dead body physics state
        glm::mat4 rootTransform = glm::translate(glm::mat4(1.0f), body.position)
                                * glm::mat4_cast(body.rotation);
        glm::mat4 invSpawnRoot = glm::translate(glm::mat4(1.0f), -body.spawnPosition);

        // Render each frozen body part using cached mesh + frozen world transform
        // The frozen transforms are in world space from death moment.
        // We compute: currentWorld = currentRoot * inv(spawnRoot) * frozenWorld
        // This correctly applies body rotation to both position offset and part orientation,
        // making limbs tumble naturally with the corpse instead of staying fixed in world space.
        for (size_t i = 0; i < body.frozenParts.size() && i < body.partMeshes.size(); ++i) {
            const DeadBody::FrozenPart& fp = body.frozenParts[i];
            const Mesh& mesh = body.partMeshes[i];
            if (mesh.verts.empty())
                continue;

            glm::mat4 partTransform = rootTransform * invSpawnRoot * fp.worldTransform;

            uploadBodyPartMesh(mesh);

            MIMITA_GL_CALL(glUseProgram(shader));
            glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, &view[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, &proj[0][0]);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, &partTransform[0][0]);
            glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
            glUniform1i(glGetUniformLocation(shader, "uTex"), 0);

            glActiveTexture(GL_TEXTURE0);
            for (const Mesh::Batch& batch : mesh.batches) {
                glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
            }
        }

        // Debug visualizations
        if (DebugVis::enabled() || DebugConfig::DEBUG_RAGDOLL) {
            // Spawn position marker
            if (body.age < 0.5f) {
                DebugVis::drawWireSphere(camera, body.spawnPosition, 0.15f, {1.0f, 1.0f, 0.0f, 1.0f});

                if (glm::length(body.deathImpulse) > 0.01f) {
                    float impLen = glm::length(body.deathImpulse);
                    glm::vec3 impEnd = body.spawnPosition + glm::normalize(body.deathImpulse) * std::min(impLen * 0.1f, 3.0f);
                    DebugVis::drawLine(camera, body.spawnPosition, impEnd, {1.0f, 0.0f, 0.0f, 1.0f});
                }

                if (glm::length(body.transferredVelocity) > 0.1f) {
                    float velLen = glm::length(body.transferredVelocity);
                    glm::vec3 velEnd = body.spawnPosition + glm::normalize(body.transferredVelocity) * std::min(velLen * 0.1f, 3.0f);
                    DebugVis::drawLine(camera, body.spawnPosition, velEnd, {0.0f, 0.5f, 1.0f, 1.0f});
                }
            }
        }

        if (DebugConfig::DEBUG_RAGDOLL) {
            // Sleep state
            if (body.sleeping) {
                DebugVis::drawWireSphere(camera, body.position, 0.2f, {0.0f, 1.0f, 0.0f, 1.0f});
                DebugVis::drawPointCross(camera, body.position, 0.08f, {0.0f, 1.0f, 0.0f, 1.0f});
            } else {
                float sleepProgress = body.sleepTimer / DEAD_SLEEP_TIME;
                glm::vec4 sleepColor = {1.0f - sleepProgress, sleepProgress, 0.0f, 0.5f};
                DebugVis::drawWireSphere(camera, body.position, 0.2f, sleepColor);
            }

            // Root anchor: spawn to current
            if (glm::length(body.spawnPosition - body.position) > 0.05f) {
                DebugVis::drawLine(camera, body.spawnPosition, body.position,
                                  {0.5f, 0.5f, 0.5f, 0.6f});
            }

            // Capsule collider wireframe
            float halfH = body.capsuleHeight * 0.5f - body.capsuleRadius;
            glm::vec3 capsuleBottom = body.position - glm::vec3(0.0f, 0.0f, halfH);
            glm::vec3 capsuleTop = body.position + glm::vec3(0.0f, 0.0f, halfH);
            DebugVis::drawWireSphere(camera, capsuleBottom, body.capsuleRadius, {0.0f, 0.5f, 1.0f, 0.3f});
            DebugVis::drawWireSphere(camera, capsuleTop, body.capsuleRadius, {0.0f, 0.5f, 1.0f, 0.3f});
            DebugVis::drawLine(camera, capsuleBottom, capsuleTop, {0.0f, 0.5f, 1.0f, 0.3f});

            // Velocity vector
            float speed = glm::length(body.velocity);
            if (speed > 0.1f) {
                glm::vec3 velEnd = body.position + glm::normalize(body.velocity) * std::min(speed * 0.15f, 3.0f);
                DebugVis::drawLine(camera, body.position, velEnd, {1.0f, 0.3f, 0.3f, 0.7f});
            }

            // Angular velocity indicator
            float angSpeed = glm::length(body.angularVelocity);
            if (angSpeed > 0.1f) {
                glm::vec3 angEnd = body.position + glm::normalize(body.angularVelocity) * std::min(angSpeed * 0.1f, 1.5f);
                DebugVis::drawLine(camera, body.position, angEnd, {0.0f, 0.8f, 0.8f, 0.5f});
            }
        }
    }
}

void DeathSystem::appendReplayActors(std::vector<ReplayActorState>& actors) const
{
    for (const DeadBody& body : mCorpses) {
        ReplayActorState actor;
        actor.id = body.id;
        actor.name = body.name;
        actor.type = "corpse";
        actor.modelPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
        actor.position = body.position;
        glm::vec3 euler = glm::eulerAngles(body.rotation);
        actor.rotation = glm::degrees(euler);
        actor.velocity = body.velocity;
        actor.health = 0;
        actor.maxHealth = 100;
        actor.grounded = false;
        actor.collidable = true;
        actor.fade = body.fade;
        actor.blackness = body.blackness;
        actor.animationState = "dead";

        // Report frozen body parts relative to current position
        for (const auto& fp : body.frozenParts) {
            ReplayBodyPartState bp;
            bp.name = fp.name;
            glm::vec3 frozenPos = glm::vec3(fp.worldTransform[3]);
            bp.position = body.position + (frozenPos - body.spawnPosition);
            glm::vec3 euler2 = glm::eulerAngles(glm::quat_cast(fp.worldTransform));
            bp.rotation = glm::degrees(euler2);
            bp.scale = glm::vec3(1.0f);
            actor.bodyParts.push_back(bp);
        }

        actors.push_back(actor);
    }
}
