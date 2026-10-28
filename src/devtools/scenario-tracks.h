#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// Shared schema-2 scenario track vocabulary. Client recording, server
// authoritative capture, merge, and playback all use this one representation so
// an actor track cannot silently change meaning between the two processes.
namespace ScenarioTracks {

constexpr int kSchemaV2 = 2;
constexpr int kTickRate = 60;

struct BodyPartBox {
    std::string name;
    glm::vec3 center{0.0f};
    glm::vec3 half{0.0f};
};

struct WeaponShape {
    bool valid = false;
    glm::vec3 previousA{0.0f};
    glm::vec3 previousB{0.0f};
    glm::vec3 currentA{0.0f};
    glm::vec3 currentB{0.0f};
    float radius = 0.0f;
};

struct ActorState {
    std::string trackId;
    std::string kind; // "player" or "npc"
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 aim{1.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float sizeScale = 1.0f;
    int health = 100;
    int maxHealth = 100;
    int team = -1;
    bool dead = false;
    bool grounded = false;
    uint32_t spawnGeneration = 0;
    uint16_t transformEpoch = 0;
    std::string weaponId;
    int equippedSlot = 0;
    int weaponState = 0;
    bool actionActive = false;
    std::string actionId;
    uint32_t actionStartTick = 0;
    int actionPhaseTick = 0;
    uint32_t nextAllowedTick = 0;
    std::vector<BodyPartBox> bodyParts;
    WeaponShape shape;
};

struct ActorIdentity {
    std::string trackId;
    std::string kind;
    uint32_t entityId = 0;
    std::string name;
    std::string avatar;
    uint32_t spawnGeneration = 0;
    uint16_t transformEpoch = 0;
};

struct LifecycleEvent {
    std::string trackId;
    std::string event; // spawn, despawn, respawn, death
    uint32_t serverTick = 0;
    uint32_t clientTick = 0;
    uint32_t spawnGeneration = 0;
    uint16_t transformEpoch = 0;
    int health = 0;
    glm::vec3 position{0.0f};
};

struct MeleeRevision {
    std::string weaponId;
    uint32_t revision = 0;
    uint32_t hash = 0;
};

nlohmann::json vec3Json(const glm::vec3& value);
glm::vec3 jsonVec3(const nlohmann::json& value);

nlohmann::json actorStateJson(const ActorState& state);
ActorState parseActorState(const nlohmann::json& value);

nlohmann::json identityJson(const ActorIdentity& identity);
ActorIdentity parseIdentity(const nlohmann::json& value);

nlohmann::json lifecycleJson(const LifecycleEvent& event);
LifecycleEvent parseLifecycle(const nlohmann::json& value);

nlohmann::json meleeRevisionsJson(const std::vector<MeleeRevision>& revisions);
std::vector<MeleeRevision> parseMeleeRevisions(const nlohmann::json& value);

// Rejects paths that escape the repository scenario directory or are not the
// expected JSONL artifact.
bool validateScenarioPath(const std::string& path, std::string* error);

// Combines the client recording (header/tick/lifecycle/footer) and the server
// sidecar (header/server_tick/lifecycle/footer) into one schema-2 JSONL file.
// The destination is replaced atomically through a temporary file.
bool mergeClientAndServer(const std::string& clientPath,
                          const std::string& serverPath,
                          const std::string& outputPath,
                          std::string* error);

} // namespace ScenarioTracks
