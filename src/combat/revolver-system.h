#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Camera;
class Player;
class NpcSystem;
struct World;

struct RevolverShotResult {
    bool fired = false;
    bool hitEntity = false;
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    std::string bodyPart;
    float damage = 0.0f;
};

class RevolverSystem {
public:
    void update(const Camera& camera, Player& player, float dt);
    void render(const Camera& camera, const World& world) const;
    RevolverShotResult fire(Player& shooter, NpcSystem& npcs, const World& world);
    void disturb(float amount);

    glm::vec3 muzzlePosition() const { return mMuzzle; }
    glm::vec3 muzzleForward() const { return mForward; }

    void addKill(const std::string& line);
    const std::vector<std::string>& killfeed() const { return mKillfeed; }

private:
    glm::vec3 mPosition{0.0f};
    glm::vec3 mForward{0.0f, 1.0f, 0.0f};
    glm::vec3 mMuzzle{0.0f};
    float mRecoil = 0.0f;
    float mDisturbance = 0.0f;
    float mTime = 0.0f;
    std::vector<std::string> mKillfeed;
};
