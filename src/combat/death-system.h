#pragma once

#include <string>
#include <vector>

#include "entities/player.h"
#include "replay/replay-scene.h"

class Camera;
class NpcSystem;
struct World;

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

private:
    void respawn(Player& actor, const std::string& actorId, const World& world);
};
