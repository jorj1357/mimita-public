#include "weapon-viewmodel.h"
#include "weapon-types.h"
#include "weapon-config.h"

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
        } else if (def->id == "grenade_launcher") {
            radius = 0.25f;
            length = 0.65f;
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
    if (modelPath.empty()) {
        if (!loadedModelPath.empty() || vao || vbo || !heldMesh.verts.empty())
            unload();
        modelLoadAttempted = true;
        return;
    }

    if (modelLoadAttempted && modelPath == loadedModelPath)
        return;

    if (modelLoadAttempted || vao || vbo || !heldMesh.verts.empty())
        unload();

    modelLoadAttempted = true;
    loadedModelPath = modelPath;
    printf("[Weapon] Loading model: %s\n", modelPath.c_str());
    heldMesh = loadGLB(modelPath);
    if (heldMesh.verts.empty()) {
        printf("[Weapon ERROR] Failed to load model:\n  %s\n", modelPath.c_str());
        loadedModelPath.clear();
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
    printf("[VIEWMODEL] model loaded verts=%zu vao=%u\n", heldMesh.verts.size(), vao);
}

void WeaponViewModel::unload() {
    if (vbo) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    heldMesh = Mesh{};
    modelGrip = glm::vec3(0.0f);
    modelMuzzle = glm::vec3(0.0f, 0.0f, 0.7f);
    modelCollisionRadius = 0.12f;
    hasModelBounds = false;
    modelLoadAttempted = false;
    loadedModelPath.clear();
}

void WeaponViewModel::update(const Camera& camera, Player& player, float dt,
                             const WeaponDefinition* def, bool updatePlayerPose,
                             const World* world) {
    // Attempt to load weapon config for this weapon (supports hot reload)
    WeaponConfig& wc = WeaponConfig::instance();
    wc.pollHotReload();
    bool hasConfig = false;
    const WeaponViewModelConfig* vmcfg = nullptr;
    if (def && !def->id.empty()) {
        vmcfg = wc.get(def->id);
        hasConfig = (vmcfg != nullptr);
    }

    // Use config modelPath if available, otherwise fall back to definition
    std::string modelPath;
    if (hasConfig && vmcfg && !vmcfg->modelPath.empty())
        modelPath = vmcfg->modelPath;
    else if (def)
        modelPath = def->modelPath;

    loadModel(modelPath);

    // Resolve tint: viewmodel config overrides weapon definition
    mTint = glm::vec3(1.0f);
    if (def)
        mTint = def->tint;
    if (hasConfig && vmcfg)
        mTint = vmcfg->color;

    // Store collision config from weapon definition
    if (def) {
        player.weaponCollisionConfig = def->collision;

        // Generate fallback colliders if enabled but no explicit colliders configured
        if (player.weaponCollisionConfig.enabled && player.weaponCollisionConfig.colliders.empty()) {
            WeaponColliderConfig fb;
            fb.name = def->id;
            fb.shape = WeaponColliderShape::Box;
            fb.pushPlayerRoot = true;
            fb.supportPlayerWeight = true;
            fb.blocksWorld = true;

            if (def->id == "shotgun") {
                fb.position = glm::vec3(0.0f, 0.0f, -0.4f);
                fb.size = glm::vec3(0.20f, 0.20f, 1.8f);
            } else if (def->id == "revolver") {
                fb.position = glm::vec3(0.0f, 0.0f, -0.2f);
                fb.size = glm::vec3(0.16f, 0.16f, 1.0f);
            } else if (def->id == "rocket_launcher") {
                fb.position = glm::vec3(0.0f, 0.0f, -0.3f);
                fb.size = glm::vec3(0.30f, 0.30f, 1.6f);
            } else if (def->id == "grenade_launcher") {
                fb.position = glm::vec3(0.0f, 0.0f, -0.3f);
                fb.size = glm::vec3(0.30f, 0.30f, 1.6f);
            } else if (def->id == "swordsword") {
                fb.position = glm::vec3(0.0f, 0.0f, -0.5f);
                fb.size = glm::vec3(0.10f, 0.10f, 1.8f);
            } else if (def->id == "godball") {
                fb.shape = WeaponColliderShape::Capsule;
                fb.position = glm::vec3(0.0f, 0.0f, 0.0f);
                fb.size = glm::vec3(0.4f, 0.4f, 0.4f);
            } else {
                // Generic fallback from model bounds or viewmodel size
                float len = 0.6f;
                float rad = 0.14f;
                if (hasModelBounds) {
                    glm::vec3 half = (modelGrip + modelMuzzle) * 0.5f;
                    fb.position = -half;
                    glm::vec3 ms = modelMuzzle - modelGrip;
                    len = std::max(glm::length(ms), 0.3f);
                }
                fb.position = glm::vec3(0.0f, 0.0f, -len * 0.4f);
                fb.size = glm::vec3(rad, rad, len);
            }
            player.weaponCollisionConfig.colliders.push_back(fb);
            // printf("[WEAPON_COLLISION_CONFIG] weapon=%s using generated fallback collider\n"
            //        "  edit config/weapons.json -> %s.collision.colliders to tune it\n",
            //        def->id.c_str(), def->id.c_str());
        } else if (player.weaponCollisionConfig.enabled) {
            // printf("[WEAPON_COLLISION_CONFIG] weapon=%s using JSON colliders from config/weapons.json -> %s.collision.colliders\n",
            //        def->id.c_str(), def->id.c_str());
        }
    }

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
            offset = def->attachmentOffset;
            rotEuler = def->attachmentRotation;
        }
        glm::mat4 customRot(1.0f);
        if (glm::length(rotEuler) > 0.001f) {
            customRot = glm::rotate(customRot, glm::radians(rotEuler.x), glm::vec3(1,0,0));
            customRot = glm::rotate(customRot, glm::radians(rotEuler.y), glm::vec3(0,1,0));
            customRot = glm::rotate(customRot, glm::radians(rotEuler.z), glm::vec3(0,0,1));
        }

        // Build the base weapon transform from arm attachment
        weaponTransform = part.worldTransform *
                           glm::translate(glm::mat4(1.0f), handPoint) *
                           glm::mat4_cast(gripRotation) *
                           customRot *
                           glm::translate(glm::mat4(1.0f), offset) *
                           glm::translate(glm::mat4(1.0f), -collisionGrip);

        // Apply JSON config viewmodel transforms (local space: +Y = forward/barrel, +X = right, +Z = up)
        if (hasConfig && vmcfg && vmcfg->enabled) {
            glm::mat4 confTransform(1.0f);
            confTransform = glm::translate(confTransform, vmcfg->positionOffset);
            confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.x), glm::vec3(1,0,0));
            confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.y), glm::vec3(0,1,0));
            confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.z), glm::vec3(0,0,1));
            confTransform = glm::scale(confTransform, vmcfg->scale);
            weaponTransform = weaponTransform * confTransform;

            // ── Per-weapon animation blending ──────────────────────────
            // Priority: Reload pose > Fire animation > Idle

            // Check if weapon is reloading
            bool isReloading = false;
            if (def && player.weaponRuntimes.count(def->id))
                isReloading = player.weaponRuntimes.at(def->id).isReloading;

            // Fire animation timer
            if (hasConfig && vmcfg->hasFireAnim && recoil > recoil * 0.5f && !isReloading) {
                // Detected a fire event (recoil snapped up)
                mFireTimer = vmcfg->fireAnim.duration;
            }
            mFireTimer = std::max(0.0f, mFireTimer - dt);

            // Reload pose blend target
            mReloadBlend = isReloading && vmcfg->hasReloadPose ? 1.0f : 0.0f;

            // Spring-smooth reload blend
            float springK = 12.0f;
            float springDamp = 6.0f;
            float force = springK * (mReloadBlend - mReloadBlendCurrent) - springDamp * mReloadBlendVelocity;
            mReloadBlendVelocity += force * dt;
            mReloadBlendCurrent += mReloadBlendVelocity * dt;
            mReloadBlendCurrent = glm::clamp(mReloadBlendCurrent, 0.0f, 1.0f);

            // Apply fire animation offset (local space)
            if (mFireTimer > 0.0f && vmcfg->hasFireAnim) {
                float progress = 1.0f - (mFireTimer / vmcfg->fireAnim.duration);
                float easeOut = 1.0f - (1.0f - progress) * (1.0f - progress);
                glm::vec3 firePos = vmcfg->fireAnim.positionOffset * (1.0f - easeOut);
                glm::vec3 fireRot = vmcfg->fireAnim.rotationOffset * (1.0f - easeOut);
                weaponTransform = glm::translate(weaponTransform, firePos);
                weaponTransform = glm::rotate(weaponTransform, glm::radians(fireRot.x), glm::vec3(1,0,0));
                weaponTransform = glm::rotate(weaponTransform, glm::radians(fireRot.y), glm::vec3(0,1,0));
                weaponTransform = glm::rotate(weaponTransform, glm::radians(fireRot.z), glm::vec3(0,0,1));

                if (DebugConfig::DEBUG_WEAPON_VIEWMODEL) {
                    printf("[WeaponAnim] %s State: Fire Animation: fire Progress: %.2f\n",
                           def ? def->id.c_str() : "?", easeOut);
                }
            }

            // Apply reload pose blend (overrides idle, fire anim stops if reload started)
            if (vmcfg->hasReloadPose && mReloadBlendCurrent > 0.001f) {
                float b = mReloadBlendCurrent;
                glm::vec3 rpPos = vmcfg->reloadPose.position * b;
                glm::vec3 rpRot = vmcfg->reloadPose.rotation * b;
                weaponTransform = glm::translate(weaponTransform, rpPos);
                weaponTransform = glm::rotate(weaponTransform, glm::radians(rpRot.x), glm::vec3(1,0,0));
                weaponTransform = glm::rotate(weaponTransform, glm::radians(rpRot.y), glm::vec3(0,1,0));
                weaponTransform = glm::rotate(weaponTransform, glm::radians(rpRot.z), glm::vec3(0,0,1));

                if (DebugConfig::DEBUG_WEAPON_VIEWMODEL) {
                    printf("[WeaponAnim] %s State: Reload Pose: reload_pose Blend: %.2f\n",
                           def ? def->id.c_str() : "?", b);
                }
            }
        }

        muzzle = glm::vec3(weaponTransform * glm::vec4(collisionMuzzle, 1.0f));
        forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));

        if (world) {
            muzzle = glm::vec3(weaponTransform * glm::vec4(collisionMuzzle, 1.0f));
            forward = glm::normalize(glm::vec3(weaponTransform * glm::vec4(modelDirection, 0.0f)));
            Capsule weaponCap = weaponCapsuleFromTransform(
                weaponTransform, collisionGrip, collisionMuzzle, collisionRadius);
            player.collision.hasWeaponCollisionCapsule = true;
            player.weaponCollisionCapsule = weaponCap;

            // Base transform from arm to weapon model space (no viewmodel config)
            glm::mat4 armToWeapon = glm::translate(glm::mat4(1.0f), handPoint) *
                glm::mat4_cast(gripRotation) * customRot *
                glm::translate(glm::mat4(1.0f), offset) *
                glm::translate(glm::mat4(1.0f), -collisionGrip);

            // Include viewmodel config and animation transforms so physics
            // recompute matches the rendered weapon position exactly.
            glm::mat4 extraTransform(1.0f);
            if (hasConfig && vmcfg && vmcfg->enabled) {
                glm::mat4 confTransform(1.0f);
                confTransform = glm::translate(confTransform, vmcfg->positionOffset);
                confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.x), glm::vec3(1,0,0));
                confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.y), glm::vec3(0,1,0));
                confTransform = glm::rotate(confTransform, glm::radians(vmcfg->rotationDegrees.z), glm::vec3(0,0,1));
                confTransform = glm::scale(confTransform, vmcfg->scale);
                extraTransform = confTransform;

                if (mFireTimer > 0.0f && vmcfg->hasFireAnim) {
                    float progress = 1.0f - (mFireTimer / vmcfg->fireAnim.duration);
                    float easeOut = 1.0f - (1.0f - progress) * (1.0f - progress);
                    glm::vec3 firePos = vmcfg->fireAnim.positionOffset * (1.0f - easeOut);
                    glm::vec3 fireRot = vmcfg->fireAnim.rotationOffset * (1.0f - easeOut);
                    extraTransform = glm::translate(extraTransform, firePos);
                    extraTransform = glm::rotate(extraTransform, glm::radians(fireRot.x), glm::vec3(1,0,0));
                    extraTransform = glm::rotate(extraTransform, glm::radians(fireRot.y), glm::vec3(0,1,0));
                    extraTransform = glm::rotate(extraTransform, glm::radians(fireRot.z), glm::vec3(0,0,1));
                }
                if (vmcfg->hasReloadPose && mReloadBlendCurrent > 0.001f) {
                    float b = mReloadBlendCurrent;
                    glm::vec3 rpPos = vmcfg->reloadPose.position * b;
                    glm::vec3 rpRot = vmcfg->reloadPose.rotation * b;
                    extraTransform = glm::translate(extraTransform, rpPos);
                    extraTransform = glm::rotate(extraTransform, glm::radians(rpRot.x), glm::vec3(1,0,0));
                    extraTransform = glm::rotate(extraTransform, glm::radians(rpRot.y), glm::vec3(0,1,0));
                    extraTransform = glm::rotate(extraTransform, glm::radians(rpRot.z), glm::vec3(0,0,1));
                }
            }
            player.weaponLocalToArm = armToWeapon * extraTransform;
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
    glUniform3f(glGetUniformLocation(shader, "uTint"), mTint.r, mTint.g, mTint.b);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);
    for (const Mesh::Batch& batch : heldMesh.batches) {
        glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
        glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        diagRenderCountWeaponDraw();
    }
    glBindVertexArray(0);
}
