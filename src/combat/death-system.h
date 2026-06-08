#pragma once

#include <string>
#include <vector>

#include "entities/player.h"
#include "replay/replay-scene.h"

class Camera;
class NpcSystem;
struct World;

struct RagdollPart {
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};
    float radius = 0.25f;
    glm::mat4 worldTransform{1.0f};
};

struct RagdollCorpse {
    std::vector<RagdollPart> parts;
    std::vector<Mesh> partMeshes;
    std::string id;
    std::string name;
    float age = 0.0f;
    float blackness = 0.0f;
    float fade = 0.0f;
};

class DeathSystem {
public:
    static DeathSystem& instance();

    bool kill(Player& victim,
              const std::string& actorId,
              const std::string& actorType,
              const std::string& killer,
              const glm::vec3& shotDirection,
              float lethalForce = 18.0f);

    void update(World& world, Player& player, NpcSystem& npcs,
                bool instantRespawnPressed, float dt);
    void render(const Camera& camera) const;
    void appendReplayActors(std::vector<ReplayActorState>& actors) const;

    const std::vector<RagdollCorpse>& corpses() const { return mCorpses; }

private:
    void respawn(Player& actor, const std::string& actorId, const World& world);
    void updateRagdollPhysics(RagdollPart& part, const World& world, float dt);

    static glm::vec3 closestPointOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    std::vector<RagdollCorpse> mCorpses;
    unsigned int mCorpseSerial = 0;
};
