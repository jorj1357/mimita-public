#include "ragdoll/ragdoll.h"
#include "ragdoll/ragdoll-config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>
#include <glm/gtx/quaternion.hpp>

#include "config.h"
#include "debug/debug-log.h"
#include "perf/perf-spike.h"
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

static FILE* gRagdollLog = nullptr;
static int gLogFrame = 0;

static void ensureLogOpen()
{
    if (!gRagdollLog) {
        gRagdollLog = fopen("logs/ragdoll_diagnostic.txt", "w");
        if (gRagdollLog) {
            fprintf(gRagdollLog, "=== RAGDOLL DIAGNOSTIC LOG ===\n");
            fprintf(gRagdollLog, "Started: %s\n", __TIMESTAMP__);
            fprintf(gRagdollLog, "Format: [TAG] frame=... data\n\n");
            fflush(gRagdollLog);
        }
    }
}

#define RLOG(fmt, ...) \
    do { \
        ensureLogOpen(); \
        if (gRagdollLog) { \
            fprintf(gRagdollLog, fmt, ##__VA_ARGS__); \
            fflush(gRagdollLog); \
        } \
    } while(0)

#define RLOGF(fmt, ...) \
    do { \
        ensureLogOpen(); \
        if (gRagdollLog) { \
            fprintf(gRagdollLog, "[RDIAG] frame=%d " fmt "\n", \
                    gLogFrame, ##__VA_ARGS__); \
            fflush(gRagdollLog); \
        } \
    } while(0)

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
    MIMITA_PERF_SCOPE("Ragdoll::SpawnFromPlayer");
    const auto& cfg = RagdollConfig::instance().data();
    if (!cfg.enabled || victim.physicalBody.parts.empty())
        return;

    ensureLogOpen();

    RagdollInstance ragdoll;
    ragdoll.id = actorId + "_ragdoll_" + std::to_string(mNextSerial++);
    ragdoll.lifetime = cfg.lifetimeSeconds;

    glm::vec3 playerVel = victim.vel + victim.externalImpulse;
    glm::mat4 invSpawnRoot = glm::translate(glm::mat4(1.0f), -victim.pos);

    RLOGF("[RAGDOLL] death begin player=%s", victim.username.c_str());
    RLOGF("[RAGDOLL] velocity=(%.4f %.4f %.4f)",
          playerVel.x, playerVel.y, playerVel.z);
    RLOGF("[RAGDOLL] death_impulse=(%.4f %.4f %.4f)",
          deathImpulse.x, deathImpulse.y, deathImpulse.z);

    {
        std::string partList;
        for (const auto& bp : victim.physicalBody.parts) {
            if (!partList.empty()) partList += " ";
            partList += bp.name;
        }
        RLOGF("[RAGDOLL] live parts found: %s", partList.c_str());
    }

    for (size_t i = 0; i < victim.physicalBody.parts.size(); ++i) {
        const PhysicalBodyPart& bp = victim.physicalBody.parts[i];
        const glm::mat4& frozenWT = bp.worldTransform;

        glm::vec3 partPos(frozenWT[3]);
        glm::quat partRot = glm::quat_cast(frozenWT);

        // NaN check
        if (std::isnan(partPos.x) || std::isnan(partPos.y) || std::isnan(partPos.z)) {
            RLOGF("[RAGDOLL][ERROR] part=%s position is NaN!", bp.name.c_str());
            partPos = victim.pos;
        }
        if (std::isnan(partRot.x) || std::isnan(partRot.y) ||
            std::isnan(partRot.z) || std::isnan(partRot.w)) {
            RLOGF("[RAGDOLL][ERROR] part=%s rotation is NaN!", bp.name.c_str());
            partRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

        // Transform ownership check
        RLOGF("[RAGDOLL_OWNER] part=%s: physics=true animation=false "
              "proceduralFrozen=%d weapon_system=CHECK network=false controller=false",
              bp.name.c_str(), (int)victim.proceduralFrozen);

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

        RLOGF("[RAGDOLL] converted existing body part=%s "
              "pos=(%.4f %.4f %.4f) vel=(%.4f %.4f %.4f) "
              "mass=%.1f colliderRadius=%.4f",
              bp.name.c_str(),
              partPos.x, partPos.y, partPos.z,
              part.velocity.x, part.velocity.y, part.velocity.z,
              part.mass, part.colliderRadius);

        ragdoll.parts.push_back(std::move(part));
    }

    // Spawn overlap check
    RLOGF("[RAGDOLL_SPAWN] total_parts=%zu", ragdoll.parts.size());

    auto partOfName = [&](const std::string& name) -> int {
        for (size_t i = 0; i < ragdoll.parts.size(); ++i)
            if (ragdoll.parts[i].name == name) return (int)i;
        return -1;
    };

    int jointCount = 0;
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

        // Spawn overlap: check if connected colliders overlap
        const auto& pa = ragdoll.parts[ia];
        const auto& pb = ragdoll.parts[ib];
        float sumRadii = pa.colliderRadius + pb.colliderRadius;
        bool overlaps = joint.restDistance < sumRadii;

        RLOGF("[RAGDOLL_SPAWN] joint %s<->%s: distance=%.4f rest=%.4f "
              "sum_radii=%.4f overlaps=%s",
              jd[0], jd[1], joint.restDistance, joint.restDistance,
              sumRadii, overlaps ? "YES" : "no");

        if (overlaps) {
            RLOGF("[RAGDOLL_SPAWN][WARN]  Connected colliders overlap at spawn! "
                  "%s.r=%.4f + %s.r=%.4f = %.4f > distance=%.4f",
                  jd[0], pa.colliderRadius, jd[1], pb.colliderRadius,
                  sumRadii, joint.restDistance);
        }

        ragdoll.joints.push_back(joint);
        jointCount++;
    }

    ragdoll.partMeshes = victim.physicalBody.partMeshes;

    RLOGF("[RAGDOLL] death complete parts=%zu joints=%d",
          ragdoll.parts.size(), jointCount);

    mRagdolls.push_back(std::move(ragdoll));
}

void RagdollDeathSystem::update(
    float dt, const World& world, Player& player, NpcSystem& npcs)
{
    MIMITA_PERF_SCOPE("Ragdoll::Update");
    (void)player;
    (void)npcs;
    const auto& cfg = RagdollConfig::instance().data();
    if (!cfg.enabled) return;

    ++gLogFrame;

    for (auto it = mRagdolls.begin(); it != mRagdolls.end();) {
        RagdollInstance& ragdoll = *it;
        ragdoll.age += dt;

        if (ragdoll.age >= ragdoll.lifetime) {
            RLOGF("[RAGDOLL] lifetime expired id=%s age=%.1f",
                  ragdoll.id.c_str(), ragdoll.age);
            it = mRagdolls.erase(it);
            continue;
        }

        float fadeEnd = ragdoll.lifetime;
        float fadeStart = fadeEnd - FADE_DURATION;
        if (ragdoll.age > fadeStart) {
            float fadeT = (ragdoll.age - fadeStart) / FADE_DURATION;
            ragdoll.fade = std::clamp(fadeT, 0.0f, 1.0f);
        }

        // --- Step A: Gravity + Integration ---
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

        // --- Step B: Joint Constraint Solving ---
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

                // NaN check
                if (std::isnan(totalForce)) {
                    RLOGF("[RAGDOLL][ERROR] Joint force is NaN! "
                          "%s<->%s iter=%d displacement=%.4f spring=%.2f damp=%.2f",
                          a.name.c_str(), b.name.c_str(), iter,
                          displacement, springForce, dampingForce);
                    joint.broken = true;
                    continue;
                }

                if (std::abs(totalForce) > cfg.jointBreakForce) {
                    joint.broken = true;
                    RLOGF("[RAGDOLL] Joint broke %s<->%s force=%.1f "
                          "(displacement=%.4f spring=%.2f damp=%.2f)",
                          a.name.c_str(), b.name.c_str(), totalForce,
                          displacement, springForce, dampingForce);
                    continue;
                }

                float invMassA = 1.0f / a.mass;
                float invMassB = 1.0f / b.mass;
                float totalInvMass = invMassA + invMassB;

                glm::vec3 impulse = dir * totalForce * dt / totalInvMass;

                // NaN check
                if (std::isnan(impulse.x) || std::isnan(impulse.y) || std::isnan(impulse.z)) {
                    RLOGF("[RAGDOLL][ERROR] Joint impulse is NaN! "
                          "%s<->%s totalForce=%.2f dt=%.6f totalInvMass=%.4f",
                          a.name.c_str(), b.name.c_str(), totalForce, dt, totalInvMass);
                    joint.broken = true;
                    continue;
                }

                glm::vec3 va_before = a.velocity;
                glm::vec3 vb_before = b.velocity;

                a.velocity += impulse * invMassA;
                b.velocity -= impulse * invMassB;

                // Joint force log (throttled)
                if (DebugConfig::DEBUG_RAGDOLL && gLogFrame % 10 == 0) {
                    RLOGF("[RAGDOLL_JOINT] iter=%d pair=%s<->%s "
                          "dist=%.4f rest=%.4f error=%.4f "
                          "spring=%.2f damp=%.2f total=%.2f "
                          "vel_a=(%.2f %.2f %.2f)->(%.2f %.2f %.2f) "
                          "vel_b=(%.2f %.2f %.2f)->(%.2f %.2f %.2f)",
                          iter, a.name.c_str(), b.name.c_str(),
                          dist, joint.restDistance, displacement,
                          springForce, dampingForce, totalForce,
                          va_before.x, va_before.y, va_before.z,
                          a.velocity.x, a.velocity.y, a.velocity.z,
                          vb_before.x, vb_before.y, vb_before.z,
                          b.velocity.x, b.velocity.y, b.velocity.z);
                }
            }
        }

        // --- Step C: Clamp velocities after joints ---
        for (auto& part : ragdoll.parts) {
            float speed = glm::length(part.velocity);
            if (speed > 50.0f) {
                RLOGF("[RAGDOLL][WARN] Post-joint speed clamp: %s speed=%.2f -> 50.0",
                      part.name.c_str(), speed);
                part.velocity *= 50.0f / speed;
            }
        }

        // --- Sleep Check ---
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

        // --- Step D: World Collision ---
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
                        float penetration = r - dist;
                        part.position += normal * penetration;

                        float velDot = glm::dot(part.velocity, normal);

                        if (glm::isnan(velDot)) {
                            RLOGF("[RAGDOLL][ERROR] Collision velDot is NaN! "
                                  "part=%s dist=%.6f r=%.4f penetration=%.4f",
                                  part.name.c_str(), dist, r, penetration);
                            continue;
                        }

                        // Log collision every 5th frame
                        if ((gLogFrame % 5) == 0) {
                            float speedBefore = glm::length(part.velocity);
                            RLOGF("[RAGDOLL_COLLISION] part=%s "
                                  "vel_before=(%.4f %.4f %.4f) speed_before=%.4f "
                                  "normal=(%.4f %.4f %.4f) "
                                  "penetration_depth=%.4f "
                                  "restitution=1.15",
                                  part.name.c_str(),
                                  part.velocity.x, part.velocity.y, part.velocity.z,
                                  speedBefore,
                                  normal.x, normal.y, normal.z,
                                  penetration);
                        }

                        if (velDot < 0.0f) {
                            glm::vec3 tangent =
                                part.velocity - normal * velDot;

                            // Log velocity change
                            glm::vec3 vBefore = part.velocity;

                            part.velocity -=
                                normal * velDot * (1.0f + 0.15f);
                            part.velocity += tangent * 0.8f;
                            part.angularVelocity = glm::vec3(0.0f);

                            float speedAfter = glm::length(part.velocity);
                            float energyBefore = 0.5f * part.mass *
                                glm::dot(vBefore, vBefore);
                            float energyAfter = 0.5f * part.mass *
                                glm::dot(part.velocity, part.velocity);
                            float energyChange = energyAfter - energyBefore;

                            if ((gLogFrame % 5) == 0) {
                                RLOGF("[RAGDOLL_COLLISION] part=%s "
                                      "vel_after=(%.4f %.4f %.4f) speed_after=%.4f "
                                      "energy_before=%.4f energy_after=%.4f "
                                      "energy_change=%.4f "
                                      "energy_change_speed=%.4f",
                                      part.name.c_str(),
                                      part.velocity.x, part.velocity.y,
                                      part.velocity.z, speedAfter,
                                      energyBefore, energyAfter,
                                      energyChange,
                                      speedAfter*speedAfter -
                                      glm::dot(vBefore, vBefore));
                            }

                            if (energyChange > 0.01f) {
                                RLOGF("[RAGDOLL][WARN] Collision ADDED energy! "
                                      "part=%s change=%.4f restitution>1.0 likely cause",
                                      part.name.c_str(), energyChange);
                            }
                        }
                        break;
                    }
                }
            }
        }

        // --- Energy Audit ---
        float totalKinetic = 0.0f;
        float maxSpeed = 0.0f;
        int sleepingCount = 0;
        float maxPenetration = 0.0f;

        for (const auto& part : ragdoll.parts) {
            float speed = glm::length(part.velocity);
            totalKinetic += 0.5f * part.mass * speed * speed;
            maxSpeed = std::max(maxSpeed, speed);
            if (part.sleeping) sleepingCount++;
        }

        // Log energy audit every 10 frames
        if (gLogFrame % 10 == 0) {
            float energyDiff = totalKinetic - ragdoll.prevKineticEnergy;

            RLOGF("[RAGDOLL_ENERGY] id=%s age=%.1f "
                  "total_kinetic=%.4f energy_change=%.4f "
                  "max_speed=%.4f sleeping=%d/%zu "
                  "max_penetration=%.4f",
                  ragdoll.id.c_str(), ragdoll.age,
                  totalKinetic, energyDiff,
                  maxSpeed, sleepingCount, ragdoll.parts.size(),
                  maxPenetration);

            if (energyDiff > 1.0f && ragdoll.prevKineticEnergy > 0.01f) {
                RLOGF("[RAGDOLL][WARN] Energy spike! +%.4f "
                      "(from %.4f to %.4f) "
                      "- system is self-powering",
                      energyDiff, ragdoll.prevKineticEnergy, totalKinetic);
            }
        }

        ragdoll.prevKineticEnergy = totalKinetic;

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
    RLOGF("[RAGDOLL] system clear — %zu ragdolls removed", mRagdolls.size());
    mRagdolls.clear();
}

void RagdollDeathSystem::destroyRagdoll(size_t index)
{
    if (index < mRagdolls.size())
        mRagdolls.erase(mRagdolls.begin() + index);
}
