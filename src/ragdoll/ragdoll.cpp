#include "ragdoll/ragdoll.h"
#include "ragdoll/ragdoll-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>
#include <glm/gtx/quaternion.hpp>

#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/gl-debug.h"
#include "camera.h"
#include "npc/npc.h"
#include "physics/movement/physics-collision.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"

extern Renderer* gRenderer;
extern TextureStore gTextures;

static constexpr float GRAVITY = 30.0f;
static constexpr float SLEEP_VELOCITY = 0.05f;
static constexpr float SLEEP_ANGULAR = 0.03f;
static constexpr float SLEEP_TIME = 1.0f;
static constexpr float FADE_DURATION = 3.0f;

static glm::vec3 closestPointOnTriangleFn(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c)
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
    float denom = va + vb + vc;
    if (denom == 0.0f) return a;
    float u = va / denom;
    float v = vb / denom;
    float w = vc / denom;
    return u * a + v * b + w * c;
}

RagdollDeathSystem& RagdollDeathSystem::instance()
{
    static RagdollDeathSystem sys;
    return sys;
}

void RagdollDeathSystem::spawnFromPlayer(
    Player& victim,
    const glm::vec3& deathImpulse,
    const std::string& actorId)
{
    const auto& cfg = RagdollConfig::instance().data();
    if (!cfg.enabled || victim.physicalBody.parts.empty())
        return;

    RagdollInstance ragdoll;
    ragdoll.id = actorId + "_ragdoll_" + std::to_string(mNextSerial++);
    ragdoll.lifetime = cfg.lifetimeSeconds;

    glm::vec3 playerVel = victim.vel + victim.externalImpulse;
    glm::mat4 invSpawnRoot = glm::translate(glm::mat4(1.0f), -victim.pos);

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL] death begin player=%s\n", victim.username.c_str());
    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL] velocity=(%.2f %.2f %.2f)\n",
        playerVel.x, playerVel.y, playerVel.z);

    {
        std::string partList;
        for (const auto& bp : victim.physicalBody.parts) {
            if (!partList.empty()) partList += " ";
            partList += bp.name;
        }
        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL] live parts found: %s\n", partList.c_str());
    }

    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        const PhysicalBodyPart& bp = victim.physicalBody.parts[i];
        const glm::mat4& frozenWT = bp.worldTransform;

        glm::vec3 partPos(frozenWT[3]);
        glm::quat partRot = glm::quat_cast(frozenWT);

        RagdollPart part;
        part.name = bp.name;
        part.position = partPos;
        part.rotation = partRot;

        auto cfgIt = cfg.parts.find(bp.name);
        if (cfgIt != cfg.parts.end()) {
            part.mass = cfgIt->second.mass;
            part.colliderRadius = cfgIt->second.radius;
        }

        if (cfg.inheritPlayerVelocity)
            part.velocity = playerVel * cfg.spawnVelocityMultiplier;

        part.velocity += deathImpulse * cfg.deathImpulseMultiplier / part.mass;

        part.localOffset = invSpawnRoot * frozenWT;
        part.meshIndex = (int)i;

        if (i < victim.outfitPartColors.size())
            part.tintColor = victim.outfitPartColors[i];

        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL] converted existing body part=%s pos=(%.2f %.2f %.2f) vel=(%.2f %.2f %.2f)\n",
            bp.name.c_str(), partPos.x, partPos.y, partPos.z,
            part.velocity.x, part.velocity.y, part.velocity.z);

        ragdoll.parts.push_back(std::move(part));
    }

    auto partOfName = [&](const std::string& name) -> int {
        for (size_t i = 0; i < ragdoll.parts.size(); ++i)
            if (ragdoll.parts[i].name == name) return (int)i;
        return -1;
    };

    const char* jointDefs[][2] = {
        {"head", "torso"},
        {"torso", "left_arm"},
        {"torso", "right_arm"},
        {"torso", "left_leg"},
        {"torso", "right_leg"},
    };

    for (const auto& jd : jointDefs) {
        int ia = partOfName(jd[0]);
        int ib = partOfName(jd[1]);
        if (ia < 0 || ib < 0) continue;

        RagdollJoint joint;
        joint.partA = ia;
        joint.partB = ib;
        joint.restDistance = glm::length(
            ragdoll.parts[ia].position - ragdoll.parts[ib].position);

        Debug::warn(Debug::Category::Ragdoll,
            "[RAGDOLL] joint created %s->%s rest=%.3f\n",
            jd[0], jd[1], joint.restDistance);

        ragdoll.joints.push_back(joint);
    }

    ragdoll.partMeshes = victim.physicalBody.partMeshes;

    Debug::warn(Debug::Category::Ragdoll,
        "[RAGDOLL] death complete parts=%zu joints=%zu\n",
        ragdoll.parts.size(), ragdoll.joints.size());

    mRagdolls.push_back(std::move(ragdoll));
}

void RagdollDeathSystem::update(
    float dt, const World& world, Player& player, NpcSystem& npcs)
{
    (void)player;
    (void)npcs;
    const auto& cfg = RagdollConfig::instance().data();
    if (!cfg.enabled) return;

    for (auto it = mRagdolls.begin(); it != mRagdolls.end();) {
        RagdollInstance& ragdoll = *it;
        ragdoll.age += dt;

        if (ragdoll.age >= ragdoll.lifetime) {
            it = mRagdolls.erase(it);
            continue;
        }

        float fadeEnd = ragdoll.lifetime;
        float fadeStart = fadeEnd - FADE_DURATION;
        if (ragdoll.age > fadeStart) {
            float fadeT = (ragdoll.age - fadeStart) / FADE_DURATION;
            ragdoll.fade = std::clamp(fadeT, 0.0f, 1.0f);
        }

        for (auto& part : ragdoll.parts) {
            if (part.sleeping) continue;

            part.velocity.z -= GRAVITY * cfg.gravityScale * dt;

            part.velocity *= (1.0f - cfg.linearDamping * dt);
            part.angularVelocity *= (1.0f - cfg.angularDamping * dt);

            float speed = glm::length(part.velocity);
            if (speed > 50.0f)
                part.velocity *= 50.0f / speed;

            float angSpeed = glm::length(part.angularVelocity);
            if (angSpeed > 25.0f)
                part.angularVelocity *= 25.0f / angSpeed;

            if (angSpeed > 0.0001f) {
                glm::quat delta = glm::angleAxis(
                    angSpeed * dt, glm::normalize(part.angularVelocity));
                part.rotation = glm::normalize(delta * part.rotation);
            }

            part.position += part.velocity * dt;
        }

        for (int iter = 0; iter < 3; ++iter) {
            for (auto& joint : ragdoll.joints) {
                if (joint.broken) continue;

                RagdollPart& a = ragdoll.parts[joint.partA];
                RagdollPart& b = ragdoll.parts[joint.partB];

                glm::vec3 delta = b.position - a.position;
                float dist = glm::length(delta);
                if (dist < 0.0001f) continue;
                glm::vec3 dir = delta / dist;

                float displacement = dist - joint.restDistance;
                float springForce = displacement * cfg.jointStiffness;

                glm::vec3 relVel = b.velocity - a.velocity;
                float dampingForce = glm::dot(relVel, dir) * cfg.jointDamping;

                float totalForce = springForce + dampingForce;

                if (std::abs(totalForce) > cfg.jointBreakForce) {
                    joint.broken = true;
                    Debug::log(Debug::Category::Ragdoll,
                        "[RAGDOLL] Joint broke %s<->%s force=%.1f\n",
                        ragdoll.parts[joint.partA].name.c_str(),
                        ragdoll.parts[joint.partB].name.c_str(),
                        totalForce);
                    continue;
                }

                float invMassA = 1.0f / a.mass;
                float invMassB = 1.0f / b.mass;
                float totalInvMass = invMassA + invMassB;

                glm::vec3 impulse = dir * totalForce * dt / totalInvMass;
                a.velocity += impulse * invMassA;
                b.velocity -= impulse * invMassB;
            }
        }

        for (auto& part : ragdoll.parts) {
            if (part.sleeping) continue;
            float speed = glm::length(part.velocity);
            float angSpeed = glm::length(part.angularVelocity);
            if (speed < SLEEP_VELOCITY && angSpeed < SLEEP_ANGULAR) {
                part.sleepTimer += dt;
                if (part.sleepTimer >= SLEEP_TIME)
                    part.sleeping = true;
            } else {
                part.sleepTimer = 0.0f;
            }
        }

        for (auto& part : ragdoll.parts) {
            if (part.sleeping || !cfg.worldCollision) continue;

            float r = part.colliderRadius;
            float maxStep = std::max(r * 0.5f, 0.1f);
            int steps = std::min(
                (int)std::ceil(glm::length(part.velocity * dt) / maxStep) + 1, 8);
            glm::vec3 stepVel = part.velocity * dt / (float)steps;

            for (int s = 0; s < steps; ++s) {
                part.position += stepVel;

                AABB queryBounds;
                queryBounds.min = part.position - glm::vec3(r + 2.0f);
                queryBounds.max = part.position + glm::vec3(r + 2.0f);
                std::vector<int> candidates;
                appendChunkTrianglesForAABB(
                    const_cast<World&>(world), queryBounds, 0.1f, candidates);

                for (int ti : candidates) {
                    if (ti < 0 ||
                        ti >= (int)world.collisionMesh.triangles.size())
                        continue;

                    const CollisionTriangle& tri =
                        world.collisionMesh.triangles[ti];
                    glm::vec3 closest = closestPointOnTriangleFn(
                        part.position, tri.a, tri.b, tri.c);
                    glm::vec3 diff = part.position - closest;
                    float dist = glm::length(diff);

                    if (dist < r && dist > 0.0001f) {
                        glm::vec3 normal = diff / dist;
                        part.position += normal * (r - dist);

                        float velDot = glm::dot(part.velocity, normal);
                        if (velDot < 0.0f) {
                            glm::vec3 tangent =
                                part.velocity - normal * velDot;
                            part.velocity -=
                                normal * velDot * (1.0f + 0.15f);
                            part.velocity += tangent * 0.8f;
                            part.angularVelocity = glm::vec3(0.0f);
                        }
                        break;
                    }
                }
            }
        }

        ++it;
    }
}

void RagdollDeathSystem::render(const Camera& camera) const
{
    const auto& cfg = RagdollConfig::instance().data();
    if (!cfg.enabled || !gRenderer)
        return;

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj(
        (float)gRenderer->width, (float)gRenderer->height);
    GLuint shader = gRenderer->shaderProgram;

    static GLint uViewLoc = -1, uProjLoc = -1, uModelLoc = -1;
    static GLint uUseColorLoc = -1, uColorLoc = -1, uTexLoc = -1;
    if (uViewLoc < 0) uViewLoc = glGetUniformLocation(shader, "view");
    if (uProjLoc < 0) uProjLoc = glGetUniformLocation(shader, "projection");
    if (uModelLoc < 0) uModelLoc = glGetUniformLocation(shader, "model");
    if (uUseColorLoc < 0) uUseColorLoc = glGetUniformLocation(shader, "uUseColor");
    if (uColorLoc < 0) uColorLoc = glGetUniformLocation(shader, "uColor");
    if (uTexLoc < 0) uTexLoc = glGetUniformLocation(shader, "uTex");

    for (const auto& ragdoll : mRagdolls) {
        if (!ragdoll.alive) continue;

        float alpha = 1.0f - ragdoll.fade;
        if (alpha <= 0.0f) continue;

        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(uViewLoc, 1, 0, &view[0][0]);
        glUniformMatrix4fv(uProjLoc, 1, 0, &proj[0][0]);
        glUniform1i(uUseColorLoc, 0);
        glUniform1i(uTexLoc, 0);
        glActiveTexture(GL_TEXTURE0);

        for (size_t i = 0; i < ragdoll.parts.size(); ++i) {
            const RagdollPart& part = ragdoll.parts[i];
            int mi = part.meshIndex;
            if (mi < 0 || mi >= (int)ragdoll.partMeshes.size()) continue;
            const Mesh& mesh = ragdoll.partMeshes[mi];
            if (mesh.verts.empty()) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), part.position)
                            * glm::mat4_cast(part.rotation)
                            * part.localOffset;

            uploadBodyPartMesh(mesh);

            glUniformMatrix4fv(uModelLoc, 1, 0, &model[0][0]);
            glUniform4f(uColorLoc, part.tintColor.r, part.tintColor.g,
                        part.tintColor.b, alpha);

            for (const Mesh::Batch& batch : mesh.batches) {
                GLuint tex = batch.texture ? batch.texture
                            : gTextures.get("default");
                MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));
                MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES,
                    (GLint)batch.first, (GLsizei)batch.count));
            }
        }

        if (DebugConfig::DEBUG_RAGDOLL) {
            for (const auto& part : ragdoll.parts) {
                DebugVis::drawWireSphere(camera, part.position,
                    part.colliderRadius,
                    glm::vec4(1.0f, 0.5f, 0.5f, alpha));
                if (glm::length(part.velocity) > 0.01f) {
                    glm::vec3 velEnd = part.position +
                        glm::normalize(part.velocity) * 0.5f;
                    DebugVis::drawLine(camera, part.position, velEnd,
                        glm::vec4(0.0f, 1.0f, 0.0f, alpha));
                }
            }

            for (const auto& joint : ragdoll.joints) {
                if (joint.broken) continue;
                const auto& a = ragdoll.parts[joint.partA];
                const auto& b = ragdoll.parts[joint.partB];
                DebugVis::drawLine(camera, a.position, b.position,
                    glm::vec4(1.0f, 1.0f, 0.0f, alpha * 0.8f));
            }

            char label[256];
            snprintf(label, sizeof(label), "RAGDOLL %s age=%.1f parts=%zu",
                ragdoll.id.c_str(), ragdoll.age, ragdoll.parts.size());
            if (!ragdoll.parts.empty())
                DebugVis::drawWorldLabel(
                    ragdoll.parts[0].position + glm::vec3(0.0f, 0.0f, 1.0f),
                    label, glm::vec4(1.0f, 0.8f, 0.2f, alpha));
        }
    }
}

void RagdollDeathSystem::clear()
{
    Debug::log(Debug::Category::Ragdoll,
        "[RAGDOLL] system clear — %zu ragdolls removed\n", mRagdolls.size());
    mRagdolls.clear();
}

void RagdollDeathSystem::destroyRagdoll(size_t index)
{
    if (index < mRagdolls.size())
        mRagdolls.erase(mRagdolls.begin() + index);
}
