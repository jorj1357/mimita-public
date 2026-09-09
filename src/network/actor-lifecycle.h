// Shared authoritative actor lifecycle data.
// Transitional player and NPC structs use this boundary while the repository
// gradually moves lifecycle data into ECS-style components.
#pragma once

#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace MimitaNet {

enum class ActorKind : uint8_t {
    Player = 1,
    Npc = 2,
};

enum class ActorSpawnReason : uint8_t {
    InitialJoin = 1,
    Reconnect = 2,
    Respawn = 3,
    NpcCreate = 4,
    DuelStart = 5,
    GamemodeStart = 6,
    MapChange = 7,
    RespawnAll = 8,
    Terminal = 9,
};

struct ActorSpawnEvent {
    uint32_t entityId = 0;
    ActorKind actorKind = ActorKind::Player;
    ActorSpawnReason reason = ActorSpawnReason::Respawn;
    uint32_t spawnGeneration = 0;
    uint16_t transformEpoch = 0;
    uint32_t serverTick = 0;
    glm::vec3 position{0.0f};
    glm::vec3 lookDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
};

// Emits the single lifecycle diagnostic/evidence boundary. Network adapters
// still send their existing player/NPC-specific state after this event.
void emitActorSpawnEvent(const ActorSpawnEvent& event,
                         const char* actorName = nullptr);

// Finalizes the shared gameplay part of a new life. The caller then performs
// only its player/NPC-specific replication work.
ActorSpawnEvent finalizeActorSpawn(ActorSpawnEvent event,
                                   const char* actorName = nullptr);

} // namespace MimitaNet
