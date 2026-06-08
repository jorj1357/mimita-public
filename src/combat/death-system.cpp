#include "combat/death-system.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>
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

        float vol = size.x * size.y * size.z;
        part.mass = std::max(vol * 100.0f, 0.5f);

        part.rotation = glm::quat_cast(src.worldTransform);

        // Velocity: spread across parts based on hit direction
        glm::vec3 offset = part.position - victimPos;
        float offsetDist = glm::length(offset);
        glm::vec3 offsetDir = offsetDist > 0.001f ? offset / offsetDist : glm::vec3(0.0f, 0.0f, 1.0f);
        float partFactor = 0.5f + 0.5f * std::max(0.0f, glm::dot(direction, offsetDir));
        part.velocity = baseVel + direction * lethalForce * 0.6f * partFactor;
        part.velocity.z += std::abs(lethalForce * 0.15f);

        // Angular velocity: stronger for visible tumbling
        part.angularVelocity = glm::cross(direction * lethalForce * 0.25f, offset);
        float angSpeed = glm::length(part.angularVelocity);
        if (angSpeed > 15.0f) {
            part.angularVelocity = (part.angularVelocity / angSpeed) * 15.0f;
        }

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
        root.mass = 10.0f;
        root.worldTransform = glm::translate(glm::mat4(1.0f), root.position);
        corpse.parts.push_back(std::move(root));
    }

    // Build constraints between connected body parts
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

    // Capture death state BEFORE disabling the alive body
    glm::vec3 deathPos = victim.pos;
    glm::vec3 deathVel = victim.vel;

    // Emit lifecycle events BEFORE moving body underground
    emitLifecycleEvent("death", victim, actorId, killer);

    // Disable alive body immediately: send it far below world so it
    // cannot overlap with the ragdoll or cause phantom collisions.
    victim.pos = glm::vec3(0.0f, 0.0f, -1000.0f);
    victim.vel = glm::vec3(0.0f);
    victim.externalImpulse = glm::vec3(0.0f);
    victim.currentHp = 0;
    victim.dead = true;
    victim.respawnTimer = RESPAWN_SECONDS;
    victim.killedBy = killer.empty() ? "unknown" : killer;

    if (actorType == "npc")
        AudioManager::instance().play(
            {"npc_death", AudioCategory::NPC, true, deathPos, 1.0f, 0.9f, 45.0f, 0});

    ReplayEffectEvent corpseEvent;
    corpseEvent.type = "corpse_spawn";
    corpseEvent.position = deathPos;
    corpseEvent.direction = direction;
    corpseEvent.velocity = baseVel;
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
    if (std::fabs(error) < 0.02f) return;

    glm::vec3 dir = delta / dist;
    float totalMass = a.mass + b.mass;
    if (totalMass < 0.001f) return;
    float aWeight = b.mass / totalMass;
    float bWeight = a.mass / totalMass;

    // Soft positional correction — avoid 100% snap which causes jitter
    float stiffness = 0.50f;
    float correction = error * stiffness;
    a.position += dir * correction * aWeight;
    b.position -= dir * correction * bWeight;

    // Velocity damping along constraint axis (stronger to kill oscillation)
    glm::vec3 relVel = b.velocity - a.velocity;
    float radialVel = glm::dot(relVel, dir);
    float damping = 0.8f;
    if (radialVel > 0.0f) {
        glm::vec3 impulse = dir * radialVel * damping;
        a.velocity += impulse * aWeight;
        b.velocity -= impulse * bWeight;
    }
}

void DeathSystem::updateRagdollPhysics(RagdollPart& part, const World& world, float dt)
{
    constexpr float GRAVITY = 9.81f;
    constexpr float DRAG = 0.3f;
    constexpr float ANGULAR_DRAG = 3.0f;
    constexpr float BOUNCE = 0.05f;
    constexpr float FRICTION = 0.2f;

    float safeDt = std::min(dt, 0.033f);

    const auto& triangles = world.collisionMesh.triangles;

    // --- STEP 0: Pre-penetration resolution ---
    // Push part out of world BEFORE integration to prevent spawn-in-floor jitter.
    // Use multiple passes for deep penetrations.
    for (int ppPass = 0; ppPass < 2; ++ppPass) {
        for (const auto& tri : triangles) {
            glm::vec3 closest = closestPointOnTriangle(part.position, tri.a, tri.b, tri.c);
            glm::vec3 diff = part.position - closest;
            float dist = glm::length(diff);
            if (dist < part.radius && dist > 0.0001f) {
                float penetration = (part.radius - dist) + 0.01f;
                glm::vec3 normal = diff / dist;
                part.position += normal * penetration;
                float vDotN = glm::dot(part.velocity, -normal);
                if (vDotN < 0.0f) {
                    part.velocity -= vDotN * (-normal) * 1.5f;
                }
            }
        }
    }

    // Gravity
    part.velocity.z -= GRAVITY * safeDt;

    // Linear drag
    part.velocity *= std::max(0.0f, 1.0f - DRAG * safeDt);

    // Angular velocity damping
    float angSpeed = glm::length(part.angularVelocity);
    if (angSpeed > 0.001f) {
        part.angularVelocity *= std::max(0.0f, 1.0f - ANGULAR_DRAG * safeDt);
        glm::quat deltaRot = glm::angleAxis(
            std::min(angSpeed * safeDt, 0.5f),
            part.angularVelocity / angSpeed);
        part.rotation = glm::normalize(deltaRot * part.rotation);
    }

    // Integrate position
    glm::vec3 move = part.velocity * safeDt;
    float moveLen = glm::length(move);

    // World collision during movement
    if (moveLen > 0.0001f) {
        glm::vec3 moveDir = move / moveLen;
        float remaining = moveLen;
        constexpr int MAX_STEPS = 3;

        for (int step = 0; step < MAX_STEPS && remaining > 0.0001f; ++step) {
            float stepDist = std::min(remaining, part.radius * 0.6f);
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

                remaining *= 0.3f;
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

            // Step 1: Update individual part physics
            for (RagdollPart& part : corpse.parts) {
                updateRagdollPhysics(part, world, dt);
            }

            // Step 2: Enforce constraints between parts (multiple iterations for stability)
            constexpr int CONSTRAINT_ITERS = 3;
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

        // Debug rendering for ragdoll constraints
        if (DebugVis::enabled()) {
            for (const RagdollConstraint& c : corpse.constraints) {
                if (c.partA >= 0 && c.partA < (int)corpse.parts.size() &&
                    c.partB >= 0 && c.partB < (int)corpse.parts.size()) {
                    const RagdollPart& a = corpse.parts[c.partA];
                    const RagdollPart& b = corpse.parts[c.partB];
                    // Constraint line
                    DebugVis::drawLine(camera, a.position, b.position, {0.0f, 1.0f, 0.0f, 0.7f});
                    // Part markers
                    DebugVis::drawWireSphere(camera, a.position, 0.08f, {0.0f, 1.0f, 0.0f, 1.0f});
                    DebugVis::drawWireSphere(camera, b.position, 0.08f, {1.0f, 0.5f, 0.0f, 1.0f});
                    // Velocity vectors
                    float aSpeed = glm::length(a.velocity);
                    if (aSpeed > 0.5f) {
                        glm::vec3 velEnd = a.position + glm::normalize(a.velocity) * std::min(aSpeed * 0.1f, 2.0f);
                        DebugVis::drawLine(camera, a.position, velEnd, {1.0f, 0.0f, 1.0f, 0.6f});
                    }
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
