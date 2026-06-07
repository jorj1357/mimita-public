#pragma once

#include <string>
#include <vector>

#include "entities/player.h"
#include "replay/replay-scene.h"

class Camera;
class NpcSystem;
struct World;

struct CorpseActor {
    explicit CorpseActor(const Player& source) : body(source) {}

    std::string id;
    std::string name;
    Player body;
    float age = 0.0f;
    float blackness = 0.0f;
    float fade = 0.0f;
    bool collidable = true;
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

    const std::vector<CorpseActor>& corpses() const { return mCorpses; }

private:
    void respawn(Player& actor, const std::string& actorId);

    std::vector<CorpseActor> mCorpses;
    unsigned int mCorpseSerial = 0;
};
