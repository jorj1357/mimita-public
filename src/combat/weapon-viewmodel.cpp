#include "weapon-viewmodel.h"
#include "weapon-types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "camera.h"
#include "config.h"
#include "debug/debug-visuals.h"
#include "debug/debug-diag.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"

static float customParamOr(const WeaponDefinition* def, const char* key, float fallback)
{
    if (!def)
        return fallback;
    auto it = def->customParams.find(key);
    return it != def->customParams.end() ? it->second : fallback;
}

static void fallbackWeaponShape(
    const WeaponDefinition* def,
    glm::vec3& grip,
    glm::vec3& muzzle,
    float& radius)
{
    float length = 0.45f;
    radius = 0.08f;

    if (def) {
        if (def->id == "shotgun") {
            length = 1.0f;
            radius = 0.10f;
        } else if (def->id == "revolver") {
            length = 0.45f;
            radius = 0.08f;
        } else if (def->id == "swordsword") {
            length = customParamOr(def, "range", 4.0f) * 0.35f;
            radius = 0.08f;
        } else if (def->id == "godball") {
            radius = def->projectileRadius > 0.0f
                ? def->projectileRadius
                : customParamOr(def, "ballRadius", 0.5f);
            length = radius * 2.0f;
        } else if (def->id == "rocket_launcher") {
            radius = 0.25f;
            length = 0.6f;
        }
    }

    length = std::max(length, 0.08f);
    radius = std::clamp(radius, 0.12f, 0.50f);
    grip = glm::vec3(0.0f);
    muzzle = glm::vec3(0.0f, 0.0f, length);
}

static Capsule weaponCapsuleFromTransform(
    const glm::mat4& transform,
    const glm::vec3& grip,
    const glm::vec3& muzzle,
    float radius)
{
    Capsule cap;
    cap.a = glm::vec3(transform * glm::vec4(grip, 1.0f));
    cap.b = glm::vec3(transform * glm::vec4(muzzle, 1.0f));
    cap.r = radius;
    return cap;
}

extern Renderer* gRenderer;
extern TextureStore gTextures;

void WeaponViewModel::loadModel(const std::string& modelPath) {
    printf("[VMTRACE] loadModel entered: path=\"%s\" modelLoadAttempted=%d vao=%u verts=%zu\n",
           modelPath.c_str(), (int)modelLoadAttempted, vao, heldMesh.verts.size());
    if (modelLoadAttempted || modelPath.empty()) {
        printf("[VMTRACE] loadModel EARLY RETURN: modelLoadAttempted=%d empty=%d\n",
               (int)modelLoadAttempted, (int)modelPath.empty());
        modelLoadAttempted = true;
        return;
    }
    modelLoadAttempted = true;
    printf("[Weapon] Loading model: %s\n", modelPath.c_str());
    heldMesh = loadGLB(modelPath);
    printf("[VMTRACE] loadGLB returned: verts=%zu batches=%zu\n", heldMesh.verts.size(), heldMesh.batches.size());
    if (heldMesh.verts.empty()) {
        printf("[Weapon ERROR] Failed to load model:\n  %s\n", modelPath.c_str());
        return;
    }
    printf("[Weapon] Loaded successfully: %s (verts=%zu)\n", modelPath.c_str(), heldMesh.verts.size());

    glm::vec3 boundsMin = heldMesh.verts.front().pos;
    glm::vec3 boundsMax = boundsMin;
    for (const Vertex& vertex : heldMesh.verts) {
        boundsMin = glm::min(boundsMin, vertex.pos);
        boundsMax = glm::max(boundsMax, vertex.pos);
    }
    glm::vec3 size = boundsMax - boundsMin;
    printf("[VMTRACE] model bounds: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f) size=(%.3f,%.3f,%.3f)\n",
           boundsMin.x, boundsMin.y, boundsMin.z,
           boundsMax.x, boundsMax.y, boundsMax.z,
           size.x, size.y, size.z);
    int axis = size.y > size.x ? 1 : 0;
    if (size.z > size[axis])
        axis = 2;
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    modelGrip = center;
    modelMuzzle = center;
    modelGrip[axis] = boundsMin[axis];
    modelMuzzle[axis] = boundsMax[axis];
    int crossA = axis == 0 ? 1 : 0;
    int crossB = axis == 2 ? 1 : 2;
    float smallerAxis = std::min(size[crossA], size[crossB]);
    modelCollisionRadius = std::clamp(smallerAxis * 0.5f, 0.12f, 0.18f);
    hasModelBounds = true;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    printf("[VMTRACE] Created VAO=%u VBO=%u\n", vao, vbo);
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
    printf("[VIEWMODEL] model loaded: path=%s verts=%zu vao=%u batches=%zu\n",
           modelPath.c_str(), heldMesh.verts.size(), vao, heldMesh.batches.size());
}

void WeaponViewModel::update(const Camera& camera, Player& player, float dt,
                             const WeaponDefinition* def, bool updatePlayerPose,
                             const World* world) {
    printf("[VMTRACE] update entered: def=%p id=%s slot=%d modelPath=%s\n",
           (void*)def, def ? def->id.c_str() : "(null)",
           def ? def->slot : -1,
           def ? def->modelPath.c_str() : "(null)");
    loadModel(def ? def->modelPath : "");
    printf("[VMTRACE] update after loadModel: vao=%u hasModelBounds=%d verts=%zu batches=%zu\n",
           vao, (int)hasModelBounds, heldMesh.verts.size(), heldMesh.batches.size());

    // FORCED DEBUG: render the loaded mesh at the player position using the main shader
    // to verify the mesh renders at all, independent of weapon attachment.
    if (vao && !heldMesh.verts.empty() && gRenderer && gRenderer->shaderProgram && def && def->id == "rocket_launcher") {
        printf("[VMTRACE] FORCED DEBUG RENDER at player pos\n");
        const unsigned int shader = gRenderer->shaderProgram;
        glm::mat4 view = camera.getView();
        glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
        glm::mat4 model = glm::translate(glm::mat4(1.0f), player.pos + glm::vec3(0, 2, 0));
        model = glm::scale(model, glm::vec3(2.0f));
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
        glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
        glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(vao);
        for (const Mesh::Batch& batch : heldMesh.batches) {
            GLuint tex = batch.texture ? batch.texture : gTextures.get("default");
            glBindTexture(GL_TEXTURE_2D, tex);
            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }
        glBindVertexArray(0);
        printf("[VMTRACE] FORCED DEBUG RENDER done\n");
    }

    if (def && def->id == "rocket_launcher" && !def->modelPath.empty())
        printf("[Weapon] Rocket launcher using: %s\n", def->modelPath.c_str());
    if (world)
        player.collision.hasWeaponCollisionCapsule = false;
    if (updatePlayerPose)
        player.updateModelWorldTransforms();

    glm::vec3 collisionGrip = modelGrip;
    glm::vec3 collisionMuzzle = modelMuzzle;
    float collisionRadius = modelCollisionRadius;
    if (!hasModelBounds || glm::length(collisionMuzzle - collisionGrip) < 0.001f)
        fallbackWeaponShape(def, collisionGrip, collisionMuzzle, collisionRadius);

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

        glm::vec3 modelDirection = glm::normalize(collisionMuzzle - collisionGrip);
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
                           glm::translate(glm::mat4(1.0f), -collisionGrip);
        muzzle = glm::vec3(weaponTransform * glm::vec4(collisionMuzzle, 1.0f));
        forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));

        if (world) {
            muzzle = glm::vec3(weaponTransform * glm::vec4(collisionMuzzle, 1.0f));
            forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));
            // Weapon collision capsule tracking (visual-only, resolved in draw)
            Capsule weaponCap = weaponCapsuleFromTransform(
                weaponTransform, collisionGrip, collisionMuzzle, collisionRadius);
            player.collision.hasWeaponCollisionCapsule = true;
            player.weaponCollisionCapsule = weaponCap;

            // Store local-space weapon data so physics can recompute
            // the capsule from a fresh right-arm transform (eliminating
            // the one-frame latency between viewmodel update and collision).
            player.weaponLocalToArm = glm::translate(glm::mat4(1.0f), handPoint) *
                glm::mat4_cast(gripRotation) * customRot *
                glm::translate(glm::mat4(1.0f), offset) *
                glm::translate(glm::mat4(1.0f), -collisionGrip);
            player.weaponGripLocal = collisionGrip;
            player.weaponMuzzleLocal = collisionMuzzle;
            player.weaponRadiusLocal = collisionRadius;
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
    printf("[VMTRACE] render entered: slotMatch=%d gRenderer=%p shader=%u vao=%u verts=%zu batches=%zu\n",
           (int)(player.equippedSlot == equippedSlot),
           (void*)gRenderer,
           gRenderer ? gRenderer->shaderProgram : 0,
           vao, heldMesh.verts.size(), heldMesh.batches.size());

    if (player.equippedSlot != equippedSlot || !gRenderer || !gRenderer->shaderProgram || !vao || heldMesh.verts.empty()) {
        printf("[VMTRACE] render SKIP: slotEq=%d gR=%d shader=%d vaoNZ=%d vertsNonEmpty=%d\n",
               (int)(player.equippedSlot == equippedSlot),
               (int)(gRenderer != nullptr),
               (int)(gRenderer && gRenderer->shaderProgram != 0),
               (int)(vao != 0),
               (int)(!heldMesh.verts.empty()));
        return;
    }

    const unsigned int shader = gRenderer->shaderProgram;
    glm::mat4 view = camera.getView();
    glm::mat4 projection = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUseProgram(shader);

    // Log the weapon transform for every rocket frame
    {
        glm::vec3 trans = glm::vec3(weaponTransform[3]);
        glm::vec3 scale;
        scale.x = glm::length(glm::vec3(weaponTransform[0]));
        scale.y = glm::length(glm::vec3(weaponTransform[1]));
        scale.z = glm::length(glm::vec3(weaponTransform[2]));
        printf("[VMTRACE] weaponTransform: pos=(%.3f,%.3f,%.3f) scale=(%.3f,%.3f,%.3f)\n",
               trans.x, trans.y, trans.z, scale.x, scale.y, scale.z);
    }

    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &weaponTransform[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
    for (const Mesh::Batch& batch : heldMesh.batches) {
        GLuint tex = batch.texture ? batch.texture : gTextures.get("default");
        printf("[VMTRACE] drawing batch: first=%zu count=%zu texture=%u\n", batch.first, batch.count, tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        diagRenderCountWeaponDraw();
    }
    glBindVertexArray(0);
    printf("[VMTRACE] render DONE\n");
}
