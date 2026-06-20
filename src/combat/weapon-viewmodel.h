#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include "map/map_common.h"

class Camera;
class Player;
struct WeaponDefinition;

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
    glm::vec3 muzzle{0.0f};
    glm::vec3 forward{0.0f, 1.0f, 0.0f};
    float recoil = 0.0f;
    float disturbance = 0.0f;

    void loadModel(const std::string& modelPath);
    void update(const Camera& camera, Player& player, float dt,
                const WeaponDefinition* def, bool updatePlayerPose = true,
                const class World* world = nullptr);
    void render(const Camera& camera, const Player& player, int equippedSlot) const;
    void unload();
};
