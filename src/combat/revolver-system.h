#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include "combat/weapon-types.h"
#include "map/map_common.h"

class Camera;
class Player;
class NpcSystem;
struct World;

class RevolverSystem {
public:
    RevolverSystem();
    void update(const Camera& camera, Player& player, float dt);
    void render(const Camera& camera, const Player& player) const;
    RevolverShotResult fire(const Camera& camera, Player& shooter, NpcSystem& npcs, const World& world);
    void disturb(float amount);

    glm::vec3 muzzlePosition() const { return mMuzzle; }
    glm::vec3 muzzleForward() const { return mForward; }

    void addKill(const std::string& line);
    const std::vector<std::string>& killfeed() const { return mKillfeed; }

private:
    void loadHeldModel();

    Mesh mHeldMesh;
    glm::mat4 mWeaponTransform{1.0f};
    glm::vec3 mModelGrip{0.0f};
    glm::vec3 mModelMuzzle{0.0f, 0.0f, 0.7f};
    unsigned int mVao = 0;
    unsigned int mVbo = 0;
    bool mModelLoadAttempted = false;
    glm::vec3 mForward{0.0f, 1.0f, 0.0f};
    glm::vec3 mMuzzle{0.0f};
    float mRecoil = 0.0f;
    float mDisturbance = 0.0f;
    float mTime = 0.0f;
    std::vector<std::string> mKillfeed;
};
