#pragma once

#include <string>
#include <vector>

#include "entities/player.h"
#include "replay/replay-scene.h"

class Camera;
class NpcSystem;
struct World;

// Single rigid dead body — replaces multi-part ragdoll for stability.
// The body is ONE physics object with a frozen skeleton pose.
struct DeadBody {
    // Physics state (single rigid object)
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 angularVelocity{0.0f};

    // Capsule collider (matches player dimensions)
    float capsuleRadius = 0.7f;
    float capsuleHeight = 3.6f;

    // Frozen skeleton pose — captured at death, rendered relative to root
    struct FrozenPart {
        std::string name;
        int nodeIndex = -1;
        glm::mat4 worldTransform{1.0f};
    };
    std::vector<FrozenPart> frozenParts;
    std::vector<Mesh> partMeshes;

    // Corpse metadata
    std::string id;
    std::string name;
    float age = 0.0f;
    float blackness = 0.0f;
    float fade = 0.0f;
    bool sleeping = false;
    float sleepTimer = 0.0f;

    glm::vec3 spawnPosition{0.0f};
    glm::vec3 transferredVelocity{0.0f};
    glm::vec3 deathImpulse{0.0f};

    // Death freeze: body is frozen for this duration before physics activates
    float deathFreezeTimer = 0.1f;
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

    const std::vector<DeadBody>& corpses() const { return mCorpses; }

private:
    void respawn(Player& actor, const std::string& actorId, const World& world);
    static void updateDeadBodyPhysics(DeadBody& body, const World& world, float dt);
    static bool trySleepBody(DeadBody& body, float dt);

    std::vector<DeadBody> mCorpses;
    unsigned int mCorpseSerial = 0;
};
