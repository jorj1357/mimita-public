#pragma once

#include <string>
#include <memory>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include "map/map_common.h"

class Camera;
class Player;
struct WeaponDefinition;
struct WeaponViewModelConfig;
struct PendingWeaponModel;

struct WeaponViewModel {
    Mesh heldMesh;
    glm::mat4 weaponTransform{1.0f};
    glm::vec3 modelGrip{0.0f};
    glm::vec3 modelMuzzle{0.0f, 0.0f, 0.7f};
    float modelCollisionRadius = 0.12f;
    bool hasModelBounds = false;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    bool modelLoadAttempted = false;
    std::string loadedModelPath;
    glm::vec3 muzzle{0.0f};
    glm::vec3 forward{0.0f, 1.0f, 0.0f};
    float recoil = 0.0f;
    float disturbance = 0.0f;

    // Shared async-loaded model asset; all viewmodels of the same weapon path
    // reference the same GPU buffers.
    std::shared_ptr<PendingWeaponModel> mModelAsset;
    unsigned int mSyncedVao = 0;

    glm::vec3 mTint{1.0f};  // current tint for rendering
    float mEmptyFlashTimer = 0.0f;  // accumulator for empty-magazine flash

    // Per-weapon animation state
    float mFireTimer = 0.0f;
    float mReloadBlend = 0.0f;       // 0=idle, 1=reload_pose (spring target)
    float mReloadBlendCurrent = 0.0f; // spring-smoothed actual value
    float mReloadBlendVelocity = 0.0f;

    void loadModel(const std::string& modelPath);
    void syncFromAsset();
    void update(const Camera& camera, Player& player, float dt,
                const WeaponDefinition* def, bool updatePlayerPose = true,
                const class World* world = nullptr);
    void render(const Camera& camera, const Player& player, int equippedSlot) const;
    void unload();
};
