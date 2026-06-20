#include "weapon-viewmodel.h"
#include "weapon-types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "config.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"

// ─── Weapon-world collision ─────────────────────────────────────────
static AABB makeTriAABB(const CollisionTriangle& tri) {
    AABB b;
    b.min = glm::min(tri.a, glm::min(tri.b, tri.c));
    b.max = glm::max(tri.a, glm::max(tri.b, tri.c));
    return b;
}

static bool overlaps(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

static glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }
    float denom = 1.0f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

static bool sphereTriContact(glm::vec3 center, float radius, const CollisionTriangle& tri, Contact& contact) {
    glm::vec3 closest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
    glm::vec3 diff = center - closest;
    float dist2 = glm::dot(diff, diff);
    if (dist2 >= radius * radius) return false;
    float dist = std::sqrt(dist2);
    if (dist < 0.000001f) {
        contact.point = closest; contact.normal = tri.normal; contact.penetration = radius;
    } else {
        contact.point = closest; contact.normal = diff / dist; contact.penetration = radius - dist;
    }
    return true;
}

static void projectRootVelocity(Player& player, const glm::vec3& normal) {
    float intoVel = glm::dot(player.vel, normal);
    if (intoVel < 0.0f)
        player.vel -= normal * intoVel;

    float intoImp = glm::dot(player.externalImpulse, normal);
    if (intoImp < 0.0f)
        player.externalImpulse -= normal * intoImp;
}

static glm::mat4 resolveWeaponCollision(const World& world, const glm::mat4& weaponTransform,
                                        const glm::vec3& modelGrip, const glm::vec3& modelMuzzle,
                                        float weaponRadius = 0.12f, const char* debugName = "weapon",
                                        glm::vec3* outCorrection = nullptr,
                                        float* outPenetration = nullptr,
                                        int* outContactCount = nullptr)
{
    if (outCorrection) *outCorrection = glm::vec3(0.0f);
    if (outPenetration) *outPenetration = 0.0f;
    if (outContactCount) *outContactCount = 0;
    if (world.collisionMesh.triangles.empty()) return weaponTransform;
    glm::vec3 worldGrip = glm::vec3(weaponTransform * glm::vec4(modelGrip, 1.0f));
    glm::vec3 worldMuzzle = glm::vec3(weaponTransform * glm::vec4(modelMuzzle, 1.0f));
    glm::vec3 capAxis = worldMuzzle - worldGrip;
    float capLen = glm::length(capAxis);
    if (capLen < 0.001f) return weaponTransform;
    capAxis /= capLen;
    Capsule weaponCap;
    weaponCap.a = worldGrip; weaponCap.b = worldMuzzle; weaponCap.r = weaponRadius;
    AABB queryBounds;
    queryBounds.min = glm::min(worldGrip, worldMuzzle) - glm::vec3(weaponRadius);
    queryBounds.max = glm::max(worldGrip, worldMuzzle) + glm::vec3(weaponRadius);
    std::vector<int> candidates;
    candidates.reserve(64);
    // Broadphase: gather triangles via chunks
    {
        const auto& tris = world.collisionMesh.triangles;
        if (!world.collisionChunks.empty() && world.collisionChunkSize > 0.001f) {
            auto chunkCoord = [&](const glm::vec3& p) {
                return glm::ivec3((int)std::floor(p.x / world.collisionChunkSize),
                                  (int)std::floor(p.y / world.collisionChunkSize),
                                  (int)std::floor(p.z / world.collisionChunkSize));
            };
            glm::ivec3 c0 = chunkCoord(queryBounds.min), c1 = chunkCoord(queryBounds.max);
            c0 = glm::clamp(c0, glm::ivec3(-1000), glm::ivec3(1000));
            c1 = glm::clamp(c1, glm::ivec3(-1000), glm::ivec3(1000));
            std::unordered_set<int> seen;
            for (int x = c0.x; x <= c1.x; ++x)
            for (int y = c0.y; y <= c1.y; ++y)
            for (int z = c0.z; z <= c1.z; ++z) {
                auto it = world.collisionChunks.find(glm::ivec3(x, y, z));
                if (it == world.collisionChunks.end()) continue;
                for (int idx : it->second)
                    if (seen.insert(idx).second) {
                        AABB tb = makeTriAABB(tris[idx]);
                        if (overlaps(queryBounds, tb)) candidates.push_back(idx);
                    }
            }
        } else {
            for (int i = 0; i < (int)tris.size(); ++i) {
                AABB tb = makeTriAABB(tris[i]);
                if (overlaps(queryBounds, tb)) candidates.push_back(i);
            }
        }
    }
    constexpr int MAX_CANDIDATES = 128;
    if ((int)candidates.size() > MAX_CANDIDATES) candidates.resize(MAX_CANDIDATES);
    float maxPen = 0.0f;
    glm::vec3 avgNormal(0.0f);
    int penCount = 0;
    constexpr int SAMPLES = 3;
    for (int ti : candidates) {
        const CollisionTriangle& tri = world.collisionMesh.triangles[ti];
        for (int s = 0; s < SAMPLES; ++s) {
            float st = (float)s / (float)(SAMPLES - 1);
            glm::vec3 sp = weaponCap.a + (weaponCap.b - weaponCap.a) * st;
            Contact ct;
            if (sphereTriContact(sp, weaponCap.r, tri, ct)) {
                if (ct.penetration > maxPen) { maxPen = ct.penetration; avgNormal = ct.normal; }
                penCount++;
            }
        }
    }
    constexpr float MIN_WEAPON_PEN = 0.003f;
    if (maxPen > MIN_WEAPON_PEN && penCount > 0) {
        glm::vec3 correction = avgNormal * maxPen;
        glm::mat4 corrected = weaponTransform;
        corrected[3][0] += correction.x; corrected[3][1] += correction.y; corrected[3][2] += correction.z;
        if (outCorrection) *outCorrection = correction;
        if (outPenetration) *outPenetration = maxPen;
        if (outContactCount) *outContactCount = penCount;
        if (DebugConfig::DEBUG_COLLISION_LIMB) {
            char label[64]; snprintf(label, sizeof(label), "%s_push", debugName);
            DebugVis::recordDepenetration(glm::vec3(corrected[3]), correction, label);
        }
        return corrected;
    }
    return weaponTransform;
}



extern Renderer* gRenderer;
extern TextureStore gTextures;

void WeaponViewModel::loadModel(const std::string& modelPath) {
    if (modelLoadAttempted || modelPath.empty()) {
        modelLoadAttempted = true;
        return;
    }
    modelLoadAttempted = true;
    heldMesh = loadGLB(modelPath);
    if (heldMesh.verts.empty()) {
        printf("[VIEWMODEL] model failed to load: %s\n", modelPath.c_str());
        return;
    }

    glm::vec3 boundsMin = heldMesh.verts.front().pos;
    glm::vec3 boundsMax = boundsMin;
    for (const Vertex& vertex : heldMesh.verts) {
        boundsMin = glm::min(boundsMin, vertex.pos);
        boundsMax = glm::max(boundsMax, vertex.pos);
    }
    glm::vec3 size = boundsMax - boundsMin;
    int axis = size.y > size.x ? 1 : 0;
    if (size.z > size[axis])
        axis = 2;
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    modelGrip = center;
    modelMuzzle = center;
    modelGrip[axis] = boundsMin[axis];
    modelMuzzle[axis] = boundsMax[axis];

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, heldMesh.verts.size() * sizeof(Vertex), heldMesh.verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glBindVertexArray(0);
    printf("[VIEWMODEL] model loaded verts=%zu\n", heldMesh.verts.size());
}

void WeaponViewModel::update(const Camera& camera, Player& player, float dt,
                             const WeaponDefinition* def, bool updatePlayerPose,
                             const World* world) {
    loadModel(def ? def->modelPath : "");
    if (updatePlayerPose)
        player.updateModelWorldTransforms();

    for (const PhysicalBodyPart& part : player.physicalBody.parts) {
        if (part.name != "rightArm")
            continue;

        glm::vec3 boundsSize = part.collider.localMax - part.collider.localMin;
        int axis = boundsSize.y > boundsSize.x ? 1 : 0;
        if (boundsSize.z > boundsSize[axis])
            axis = 2;
        glm::vec3 handPoint = (part.collider.localMin + part.collider.localMax) * 0.5f;
        float minDistance = std::fabs(part.collider.localMin[axis]);
        float maxDistance = std::fabs(part.collider.localMax[axis]);
        handPoint[axis] = maxDistance >= minDistance ? part.collider.localMax[axis] : part.collider.localMin[axis];
        glm::vec3 handDirection(0.0f);
        handDirection[axis] = handPoint[axis] >= 0.0f ? 1.0f : -1.0f;

        glm::vec3 modelDirection = glm::normalize(modelMuzzle - modelGrip);
        glm::quat gripRotation = glm::rotation(modelDirection, handDirection);
        glm::vec3 offset(0.0f);
        glm::vec3 rotEuler(0.0f);
        if (def) {
            if (def->id == "revolver") {
                offset = glm::vec3(gPlayerProcedural.revolverOffsetX,
                                   gPlayerProcedural.revolverOffsetY,
                                   gPlayerProcedural.revolverOffsetZ);
                rotEuler = glm::vec3(gPlayerProcedural.revolverRotX,
                                     gPlayerProcedural.revolverRotY,
                                     gPlayerProcedural.revolverRotZ);
            } else if (def->id == "shotgun") {
                offset = glm::vec3(gPlayerProcedural.shotgunOffsetX,
                                   gPlayerProcedural.shotgunOffsetY,
                                   gPlayerProcedural.shotgunOffsetZ);
                rotEuler = glm::vec3(gPlayerProcedural.shotgunRotX,
                                     gPlayerProcedural.shotgunRotY,
                                     gPlayerProcedural.shotgunRotZ);
            } else {
                offset = def->viewModelOffset;
            }
        }
        glm::mat4 customRot(1.0f);
        if (glm::length(rotEuler) > 0.001f) {
            customRot = glm::rotate(customRot, glm::radians(rotEuler.x), glm::vec3(1,0,0));
            customRot = glm::rotate(customRot, glm::radians(rotEuler.y), glm::vec3(0,1,0));
            customRot = glm::rotate(customRot, glm::radians(rotEuler.z), glm::vec3(0,0,1));
        }
        weaponTransform = part.worldTransform *
                           glm::translate(glm::mat4(1.0f), handPoint) *
                           glm::mat4_cast(gripRotation) *
                           customRot *
                           glm::translate(glm::mat4(1.0f), offset) *
                           glm::translate(glm::mat4(1.0f), -modelGrip);
        muzzle = glm::vec3(weaponTransform * glm::vec4(modelMuzzle, 1.0f));
        forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));

        // Weapon-world collision: push weapon back if it contacts geometry
        if (world) {
            float wpnRad = (def && def->id == "shotgun") ? 0.15f : 0.12f;
            glm::vec3 rootCorrection(0.0f);
            float penetration = 0.0f;
            int contactCount = 0;
            weaponTransform = resolveWeaponCollision(
                *world, weaponTransform, modelGrip, modelMuzzle, wpnRad,
                def ? def->id.c_str() : "weapon",
                &rootCorrection, &penetration, &contactCount);
            if (glm::dot(rootCorrection, rootCorrection) > 0.0000001f) {
                glm::vec3 rootBefore = player.pos;
                player.pos += rootCorrection;
                projectRootVelocity(player, glm::normalize(rootCorrection));
                player.updateModelWorldTransforms();
                DebugVis::recordDepenetration(rootBefore, rootCorrection, "weapon-root-response");
                Debug::logThrottled(Debug::Category::Collision, "weapon-root-response", 0.25f,
                    "[BODY COLLISION] part=Weapon:%s penetration=%.4f correction=(%.4f %.4f %.4f) hits=%d\n"
                    "[ROOT RESPONSE] rootCorrection=(%.4f %.4f %.4f) pos=(%.4f %.4f %.4f)\n",
                    def ? def->id.c_str() : "weapon", penetration,
                    rootCorrection.x, rootCorrection.y, rootCorrection.z, contactCount,
                    rootCorrection.x, rootCorrection.y, rootCorrection.z,
                    player.pos.x, player.pos.y, player.pos.z);
            }
            muzzle = glm::vec3(weaponTransform * glm::vec4(modelMuzzle, 1.0f));
            forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));
        }
        break;
    }

    DebugVis::recordMovement(player.pos, player.externalImpulse * 0.1f, "weapon-recoil-impulse");

    if (def && player.equippedSlot == def->slot && recoil > 0.01f) {
        for (PhysicalBodyPart& part : player.physicalBody.parts) {
            if (part.name == "rightArm") {
                part.pose.rotationEuler.x -= recoil * 4.0f;
                part.pose.translation.y -= recoil * 0.015f;
                part.pose.translation.z += recoil * 0.02f;
                break;
            }
            if (part.name == "leftArm") {
                part.pose.rotationEuler.x -= recoil * 2.0f;
                part.pose.translation.z += recoil * 0.01f;
                break;
            }
        }
    }
    recoil = std::max(0.0f, recoil - dt * 15.0f);
    disturbance = std::max(0.0f, disturbance - dt * 8.0f);
}

void WeaponViewModel::render(const Camera& camera, const Player& player, int equippedSlot) const {
    if (player.equippedSlot != equippedSlot || !gRenderer || !gRenderer->shaderProgram || !vao || heldMesh.verts.empty())
        return;

    const unsigned int shader = gRenderer->shaderProgram;
    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &weaponTransform[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
    for (const Mesh::Batch& batch : heldMesh.batches) {
        glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        diagRenderCountWeaponDraw();
    }
    glBindVertexArray(0);
}
