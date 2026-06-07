#include "combat/death-system.h"

#include <algorithm>

#include <glad/glad.h>
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

    glm::vec3 baseVel = victim.vel;

    glm::vec3 direction = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    RagdollCorpse corpse;
    corpse.id = actorId + "_corpse_" + std::to_string(++mCorpseSerial);
    corpse.name = victim.username + " corpse";
    corpse.partMeshes = victim.physicalBody.partMeshes;

    victim.updateModelWorldTransforms();

    glm::vec3 victimPos = victim.pos;

    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        const auto& src = victim.physicalBody.parts[i];
        RagdollPart part;
        part.name = src.name;

        part.position = glm::vec3(src.worldTransform[3]);

        glm::vec3 size = src.collider.localMax - src.collider.localMin;
        part.radius = std::max({size.x, size.y, size.z}) * 0.5f;
        part.radius = std::max(part.radius, 0.1f);

        part.rotation = glm::quat_cast(src.worldTransform);

        part.velocity = baseVel + direction * lethalForce * 0.5f;
        part.velocity.z += std::abs(lethalForce * 0.15f);

        glm::vec3 offset = part.position - victimPos;
        part.angularVelocity = glm::cross(direction * lethalForce * 0.3f, offset);

        part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);

        corpse.parts.push_back(std::move(part));
    }

    if (corpse.parts.empty()) {
        RagdollPart root;
        root.name = "root";
        root.position = victimPos;
        root.velocity = baseVel + direction * lethalForce;
        root.velocity.z += std::abs(lethalForce * 0.15f);
        root.radius = 0.5f;
        root.worldTransform = glm::translate(glm::mat4(1.0f), root.position);
        corpse.parts.push_back(std::move(root));
    }

    victim.externalImpulse = glm::vec3(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.respawnTimer = RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, victim.pos, 1.0f, 0.9f, 45.0f, 0});

    emitLifecycleEvent("death", victim, actorId, killer);

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = victim.pos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = baseVel;
    corpseEvent.sourceActorId = actorId;
    corpseEvent.targetActorId = corpse.id;
    captureReplayEffect(corpseEvent);

    mCorpses.push_back(std::move(corpse));

    return true;
}

void DeathSystem::respawn(Player& actor, const std::string& actorId)
{
    actor.pos = actor.respawnPosition;
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

void DeathSystem::updateRagdollPhysics(RagdollPart& part, const World& world, float dt)
{
    constexpr float GRAVITY = 9.81f;
    constexpr float DRAG = 2.0f;
    constexpr float ANGULAR_DRAG = 4.0f;
    constexpr float BOUNCE = 0.2f;
    constexpr float FRICTION = 0.5f;

    float safeDt = std::min(dt, 0.05f);

    part.velocity.z -= GRAVITY * safeDt;

    part.angularVelocity *= std::max(0.0f, 1.0f - ANGULAR_DRAG * safeDt);

    if (glm::length(part.angularVelocity) > 0.0001f) {
        float angSpeed = glm::length(part.angularVelocity);
        glm::quat deltaRot = glm::angleAxis(angSpeed * safeDt, part.angularVelocity / angSpeed);
        part.rotation = glm::normalize(deltaRot * part.rotation);
    }

    part.velocity *= std::max(0.0f, 1.0f - DRAG * safeDt);

    const auto& triangles = world.collisionMesh.triangles;

    glm::vec3 posBefore = part.position;
    glm::vec3 move = part.velocity * safeDt;
    float moveLen = glm::length(move);

    if (moveLen > 0.0001f) {
        glm::vec3 moveDir = move / moveLen;
        float remaining = moveLen;
        constexpr int MAX_STEPS = 4;

        for (int step = 0; step < MAX_STEPS && remaining > 0.0001f; ++step) {
            float stepDist = std::min(remaining, part.radius * 0.8f);
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
                    part.velocity -= (1.0f + BOUNCE) * vDotN * bestNormal;

                    glm::vec3 tangential = part.velocity - glm::dot(part.velocity, bestNormal) * bestNormal;
                    float tangLen = glm::length(tangential);
                    if (tangLen > 0.001f) {
                        tangential *= std::max(0.0f, 1.0f - FRICTION * safeDt);
                        part.velocity = glm::dot(part.velocity, bestNormal) * bestNormal + tangential;
                    }
                }

                float newRemaining = remaining * (1.0f - stepDist / moveLen);
                remaining = std::min(remaining * 0.5f, newRemaining);
            } else {
                part.position = newPos;
                remaining -= stepDist;
            }
        }
    }

    part.worldTransform = glm::translate(glm::mat4(1.0f), part.position) * glm::mat4_cast(part.rotation);
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

        if (corpse.age < CORPSE_STAGE1_SECONDS) {
            corpse.blackness = std::clamp(corpse.age / CORPSE_STAGE1_SECONDS, 0.0f, 1.0f);
            corpse.fade = 0.0f;

            for (RagdollPart& part : corpse.parts) {
                updateRagdollPhysics(part, world, dt);
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
            respawn(player, player.username);
    }

    for (Npc& npc : npcs.all()) {
        if (!npc.body.dead) continue;
        npc.body.respawnTimer = std::max(0.0f, npc.body.respawnTimer - dt);
        if (npc.body.respawnTimer <= 0.0f)
            respawn(npc.body, "npc_" + std::to_string(npc.id));
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
