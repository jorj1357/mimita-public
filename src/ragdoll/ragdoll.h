#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "entities/player.h"

struct World;
class Camera;
class NpcSystem;

struct RagdollPart {
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float mass = 1.0f;
    float colliderRadius = 0.2f;
    float sleepTimer = 0.0f;
    bool sleeping = false;

    int meshIndex = -1;
    glm::mat4 localOffset{1.0f};
    glm::vec3 tintColor{1.0f};
};

struct RagdollJoint {
    int partA = 0;
    int partB = 0;
    float restDistance = 0.0f;
    bool broken = false;
};

struct RagdollInstance {
    std::string id;
    std::vector<RagdollPart> parts;
    std::vector<RagdollJoint> joints;
    float age = 0.0f;
    float lifetime = 30.0f;
    bool alive = true;
    float fade = 0.0f;

    std::vector<Mesh> partMeshes;
};

class RagdollDeathSystem {
public:
    static RagdollDeathSystem& instance();

    void spawnFromPlayer(Player& victim,
                         const glm::vec3& deathImpulse,
                         const std::string& actorId);

    void update(float dt, const World& world, Player& player, NpcSystem& npcs);
    void render(const Camera& camera) const;

    void clear();
    void destroyRagdoll(size_t index);

    const std::vector<RagdollInstance>& ragdolls() const { return mRagdolls; }
    std::vector<RagdollInstance>& ragdolls() { return mRagdolls; }

private:
    RagdollDeathSystem() = default;

    std::vector<RagdollInstance> mRagdolls;
    uint32_t mNextSerial = 0;
};
