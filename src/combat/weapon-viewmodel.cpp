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
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "map/map_loader.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "world/world.h"

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

void WeaponViewModel::update(const Camera& camera, Player& player, float dt, const WeaponDefinition* def) {
    loadModel(def ? def->modelPath : "");
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
        glm::vec3 offset = def ? def->viewModelOffset : glm::vec3(0.0f);
        weaponTransform = part.worldTransform *
                           glm::translate(glm::mat4(1.0f), handPoint) *
                           glm::mat4_cast(gripRotation) *
                           glm::translate(glm::mat4(1.0f), offset) *
                           glm::translate(glm::mat4(1.0f), -modelGrip);
        muzzle = glm::vec3(weaponTransform * glm::vec4(modelMuzzle, 1.0f));
        forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));
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
    }
    glBindVertexArray(0);
}