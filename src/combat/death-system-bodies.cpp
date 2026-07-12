#include "combat/death-system.h"
#include "ragdoll/ragdoll-config.h"

#include <algorithm>
#include <cmath>

#include <glad/glad.h>
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/transform-debug.h"
#include "debug/gl-debug.h"
#include "camera.h"
#include "physics/config.h"
#include "world/world.h"
#include "renderer/renderer.h"
#include "replay/replay.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;

namespace {

constexpr float DEAD_GRAVITY = 30.0f;
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
constexpr float CORPSE_STAGE1_SECONDS = 2.0f;
constexpr float CORPSE_STAGE2_SECONDS = 1.0f;
constexpr float CORPSE_TOTAL_SECONDS = 3.0f;

#define DEAD_LOG(fmt, ...) \
    do { if (DebugConfig::DEBUG_RAGDOLL) \
        printf("[DEAD] " fmt "\n", ##__VA_ARGS__); } while(0)

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

glm::vec3 closestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 ab = b - a;
    float len2 = glm::dot(ab, ab);
    if (len2 < 0.0001f) return a;
    float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    return a + ab * t;
}
}

void DeathSystem::updateDeadBodyPhysics(DeadBody& body, const World& world, float dt)
{
    glm::vec3 prePos = body.position;
    glm::vec3 preVel = body.velocity;

    float safeDt = std::min(dt, 0.033f);
    const auto& triangles = world.collisionMesh.triangles;
    float radius = body.capsuleRadius;

    body.velocity.z -= DEAD_GRAVITY * safeDt;

    body.velocity *= std::max(0.0f, 1.0f - DEAD_DRAG * safeDt);

    float angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > 0.001f) {
        body.angularVelocity *= std::max(0.0f, 1.0f - DEAD_ANGULAR_DRAG * safeDt);
        glm::quat deltaRot = glm::angleAxis(
            std::min(angSpeed * safeDt, 0.5f),
            body.angularVelocity / angSpeed);
        body.rotation = glm::normalize(deltaRot * body.rotation);
    }

    float speed = glm::length(body.velocity);
    if (speed > DEAD_MAX_LINEAR_VELOCITY) {
        body.velocity = (body.velocity / speed) * DEAD_MAX_LINEAR_VELOCITY;
    }
    angSpeed = glm::length(body.angularVelocity);
    if (angSpeed > DEAD_MAX_ANGULAR_VELOCITY) {
        body.angularVelocity = (body.angularVelocity / angSpeed) * DEAD_MAX_ANGULAR_VELOCITY;
    }

    glm::vec3 move = body.velocity * safeDt;
    float moveLen = glm::length(move);

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

    speed = glm::length(body.velocity);
    if (speed > DEAD_MAX_LINEAR_VELOCITY) {
        body.velocity = (body.velocity / speed) * DEAD_MAX_LINEAR_VELOCITY;
    }

    DEAD_LOG("pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f) speed=%.2f",
             body.position.x, body.position.y, body.position.z,
             body.velocity.x, body.velocity.y, body.velocity.z,
             speed);

    glm::vec3 delta = body.position - prePos;
    if (glm::length(delta) > 0.001f || glm::length(body.velocity - preVel) > 0.001f) {
        TransformDebug::instance().logWrite("Physics", body.id,
                                            prePos, body.position,
                                            preVel, body.velocity);
    }
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
        if (body.sleepTimer > DEAD_SLEEP_TIME * 0.5f) {
            body.velocity *= 0.9f;
            body.angularVelocity *= 0.9f;
        }
    } else {
        body.sleepTimer = 0.0f;
    }
    return false;
}

void DeathSystem::render(const Camera& camera) const
{
    if (mCorpses.empty() || !gRenderer)
        return;

    // [RAGDOLL SUPPRESSION REMOVED] — always render DeadBody skeleton

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    GLuint shader = gRenderer->shaderProgram;

    for (const DeadBody& body : mCorpses) {
        if (body.fade >= 1.0f)
            continue;

        glm::mat4 rootTransform = glm::translate(glm::mat4(1.0f), body.position)
                                * glm::mat4_cast(body.rotation);
        glm::mat4 invSpawnRoot = glm::translate(glm::mat4(1.0f), -body.spawnPosition);

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

        if (DebugConfig::DEBUG_NPC_DEATH) {
            DebugVis::drawWireSphere(camera, body.spawnPosition, 0.15f, {1.0f, 1.0f, 1.0f, 1.0f});
            DebugVis::drawWireSphere(camera, body.position, 0.2f, {1.0f, 0.0f, 1.0f, 1.0f});
            DebugVis::drawLine(camera, body.spawnPosition, body.position, {0.5f, 0.0f, 0.5f, 0.6f});

            float speed = glm::length(body.velocity);
            if (speed > 0.1f) {
                glm::vec3 velEnd = body.position + glm::normalize(body.velocity) * std::min(speed * 0.2f, 4.0f);
                DebugVis::drawLine(camera, body.position, velEnd, {0.0f, 1.0f, 0.0f, 1.0f});
            }

            float impLen = glm::length(body.deathImpulse);
            if (impLen > 0.1f) {
                glm::vec3 impEnd = body.spawnPosition + glm::normalize(body.deathImpulse) * std::min(impLen * 0.1f, 3.0f);
                DebugVis::drawLine(camera, body.spawnPosition, impEnd, {1.0f, 0.0f, 0.0f, 1.0f});
            }

            glm::vec3 gravEnd = body.position + body.debugGravity * 0.05f;
            DebugVis::drawLine(camera, body.position, gravEnd, {0.0f, 0.0f, 1.0f, 0.7f});

            float angSpeed = glm::length(body.angularVelocity);
            if (angSpeed > 0.1f) {
                glm::vec3 angEnd = body.position + glm::normalize(body.angularVelocity) * std::min(angSpeed * 0.15f, 2.0f);
                DebugVis::drawLine(camera, body.position, angEnd, {1.0f, 1.0f, 0.0f, 0.8f});
            }

            {
                float distFromSpawn = glm::length(body.position - body.spawnPosition);
                char label[512];
                const char* auth = body.sleeping ? "SLEEPING" : "Ragdoll";
                snprintf(label, sizeof(label),
                         "RAGDOLL #%d\n"
                         "POS: %.1f %.1f %.1f\n"
                         "VEL: %.1f %.1f %.1f\n"
                         "SPD: %.1f\n"
                         "ANG: %.2f %.2f %.2f\n"
                         "DST: %.2f\n"
                         "AGE: %.2f\n"
                         "GRAVITY: %.0f %.0f %.0f\n"
                         "IMPULSE: %.1f %.1f %.1f\n"
                         "AUTHORITY: %s",
                         body.debugId,
                         body.position.x, body.position.y, body.position.z,
                         body.velocity.x, body.velocity.y, body.velocity.z,
                         speed,
                         body.angularVelocity.x, body.angularVelocity.y, body.angularVelocity.z,
                         distFromSpawn,
                         body.age,
                         body.debugGravity.x, body.debugGravity.y, body.debugGravity.z,
                         body.deathImpulse.x, body.deathImpulse.y, body.deathImpulse.z,
                         auth);

                glm::vec3 labelPos = body.position + glm::vec3(0.0f, 0.0f, body.capsuleHeight * 0.5f + 0.5f);
                DebugVis::drawWorldLabel(labelPos, label, {1.0f, 0.8f, 0.2f, 1.0f});
            }
        }

        if (DebugVis::enabled() || DebugConfig::DEBUG_RAGDOLL) {
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
            if (body.sleeping) {
                DebugVis::drawWireSphere(camera, body.position, 0.2f, {0.0f, 1.0f, 0.0f, 1.0f});
                DebugVis::drawPointCross(camera, body.position, 0.08f, {0.0f, 1.0f, 0.0f, 1.0f});
            } else {
                float sleepProgress = body.sleepTimer / DEAD_SLEEP_TIME;
                glm::vec4 sleepColor = {1.0f - sleepProgress, sleepProgress, 0.0f, 0.5f};
                DebugVis::drawWireSphere(camera, body.position, 0.2f, sleepColor);
            }

            if (glm::length(body.spawnPosition - body.position) > 0.05f) {
                DebugVis::drawLine(camera, body.spawnPosition, body.position,
                                  {0.5f, 0.5f, 0.5f, 0.6f});
            }

            float halfH = body.capsuleHeight * 0.5f - body.capsuleRadius;
            glm::vec3 capsuleBottom = body.position - glm::vec3(0.0f, 0.0f, halfH);
            glm::vec3 capsuleTop = body.position + glm::vec3(0.0f, 0.0f, halfH);
            DebugVis::drawWireSphere(camera, capsuleBottom, body.capsuleRadius, {0.0f, 0.5f, 1.0f, 0.3f});
            DebugVis::drawWireSphere(camera, capsuleTop, body.capsuleRadius, {0.0f, 0.5f, 1.0f, 0.3f});
            DebugVis::drawLine(camera, capsuleBottom, capsuleTop, {0.0f, 0.5f, 1.0f, 0.3f});

            float speed = glm::length(body.velocity);
            if (speed > 0.1f) {
                glm::vec3 velEnd = body.position + glm::normalize(body.velocity) * std::min(speed * 0.15f, 3.0f);
                DebugVis::drawLine(camera, body.position, velEnd, {1.0f, 0.3f, 0.3f, 0.7f});
            }

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
