// 2026-11-14
// Resident <-> whole-game DLL server cutover ABI.
//
// The resident host owns sockets, ICE, scheduling, and the durable player/NPC
// containers. The DLL owns server behavior: packet handling, player and NPC
// simulation, publish, and gamemode tick. This header crosses the boundary with
// opaque pointers only so it stays lightweight for the host side.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kGameServerAbiVersion = 1;
inline constexpr char kGetGameServerSymbol[] = "GetMimitaGameServerV1";
// Optional additive policy export. Old EXEs ignore it; a new EXE uses it to
// move server startup/config/world behavior into the replaceable generation
// without changing the GameServerV1 layout.
inline constexpr std::uint32_t kGameServerPolicyAbiVersion = 1;
inline constexpr char kGetGameServerPolicySymbol[] =
    "GetMimitaGameServerPolicyV1";

enum ServerFrameFlags : std::uint32_t {
    kServerFrameNone = 0,
    kServerFrameHazardScratch = 1u << 0,
};

struct ServerFrameV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerFrameV1);
    std::uint32_t tick = 0;
    std::uint32_t flags = kServerFrameNone;
    void* players = nullptr;
    void* npcs = nullptr;
    void* projectiles = nullptr;
    void* npcSystem = nullptr;
    void* world = nullptr;
    void* npcWorld = nullptr;
    void* vehicleState = nullptr;
    void* physicalEntityReplication = nullptr;
    void* transportStats = nullptr;
    void* disagreementRetransmit = nullptr;
    void* npcIdsAlive = nullptr;
    void* mirrorPlayer = nullptr;
    void* authenticatedPlayer = nullptr;
    void* claimedTransport = nullptr;
    void* pendingRemovals = nullptr;
    std::uint32_t* nextPlayerId = nullptr;
    std::uint32_t* nextEntityId = nullptr;
    std::uint32_t* nextProjectileId = nullptr;
    std::uint64_t* totalPacketsIn = nullptr;
    std::uint64_t* totalPacketsOut = nullptr;
    std::uintptr_t socket = 0;
    const char* procedural_mode = nullptr;
    std::uint32_t procedural_seed = 0;
    std::int32_t* procedural_start_queued = nullptr;
    void (*set_spawn_override)(void* context, std::int32_t enabled,
                               float x, float y, float z) = nullptr;
    void* spawn_override_context = nullptr;
    std::uint64_t simulate_players_us = 0;
    std::uint64_t simulate_world_us = 0;
    std::uint64_t publish_us = 0;
    std::uint64_t gamemode_us = 0;
};

struct ServerPacketResultV1 {
    std::uint32_t handled = 0;
    std::uint32_t transportConsumed = 0;
    std::uint32_t playerId = 0;
};

struct GameServerV1 {
    std::uint32_t abiVersion = kGameServerAbiVersion;
    std::uint32_t structSize = sizeof(GameServerV1);
    // Diagnostics: compile-time revision of the server behavior in this
    // generation. Change it when proving a behavior-changing swap.
    std::uint32_t runtimeRevision = 1;
    int (*process_packet)(void* frame, const void* event,
                          ServerPacketResultV1* out) = nullptr;
    int (*simulate_players)(void* frame) = nullptr;
    int (*simulate_world)(void* frame) = nullptr;
    int (*publish)(void* frame) = nullptr;
    int (*gamemode)(void* frame) = nullptr;
};

struct ServerConnectionIdV1 {
    std::uint32_t kind = 0;
    std::uint32_t reserved = 0;
    std::uint64_t value = 0;
};

struct ServerStartupV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerStartupV1);
    const char* mapName = nullptr;
    const char* mapPath = nullptr;
    const char* gameMode = nullptr;
    const char* gamemodeId = nullptr;
    const char* serverName = nullptr;
    const char* hostPlayerName = nullptr;
    const char* password = nullptr;
    const char* sessionToken = nullptr;
    const char* proceduralMode = nullptr;
    std::uint32_t proceduralSeed = 0;
    std::uint32_t npcCount = 0;
    std::uint32_t npcsEnabled = 0;
    std::int32_t weaponSetId = 0;
    std::uint32_t weaponSetExplicit = 0;
    std::uint32_t autoMapRotation = 0;
    std::uint32_t mapRotationMinutes = 0;
    std::uint32_t discordNotification = 0;
    std::uint32_t passwordProtected = 0;
    std::uint32_t maxPlayers = 0;
    std::uint32_t duel = 0;
    std::uint32_t spawnOverrideEnabled = 0;
    float spawnOverride[3] = {0.0f, 0.0f, 0.0f};
};

struct ServerStartupResultV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerStartupResultV1);
    std::int32_t ok = 0;
    std::uint32_t resolvedMaxPlayers = 0;
    char resolvedMapName[128] = {};
    char resolvedMapPath[512] = {};
    char error[256] = {};
};

struct ServerWorldLoadV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerWorldLoadV1);
    const char* mapName = nullptr;
    const char* mapPath = nullptr;
    const char* gameMode = nullptr;
    void* headlessWorld = nullptr;
    void* npcWorld = nullptr;
};

struct ServerWorldLoadResultV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerWorldLoadResultV1);
    std::int32_t ok = 0;
    std::uint32_t mapLoaded = 0;
    std::uint32_t triangleCount = 0;
    std::uint32_t spawnCount = 0;
    std::uint32_t npcCollisionTriangles = 0;
    char error[256] = {};
};

struct ServerConfigureV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerConfigureV1);
    const char* mapName = nullptr;
    const char* gameMode = nullptr;
    const char* gamemodeId = nullptr;
    std::uint32_t npcCount = 0;
    std::uint32_t npcsEnabled = 0;
    std::int32_t weaponSetId = 0;
    std::uint32_t weaponSetExplicit = 0;
    std::uint32_t autoMapRotation = 0;
    std::uint32_t mapRotationMinutes = 0;
    std::uint32_t duel = 0;
    std::uint32_t spawnOverrideEnabled = 0;
    float spawnOverride[3] = {0.0f, 0.0f, 0.0f};
    void* headlessWorld = nullptr;
    void* npcs = nullptr;
    std::uint32_t* nextEntityId = nullptr;
};

struct ServerConfigureResultV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerConfigureResultV1);
    std::int32_t ok = 0;
    std::uint32_t arenaBallSpawned = 0;
    std::uint32_t startupNpcCount = 0;
    char error[256] = {};
};

struct ServerAuthIdentityV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerAuthIdentityV1);
    std::uint64_t userId = 0;
    char token[256] = {};
};

struct ServerProjectilePerfV1 {
    std::uint32_t version = 1;
    std::uint32_t structSize = sizeof(ServerProjectilePerfV1);
    std::uint64_t projectileSimUs = 0;
    std::uint64_t triangleQueryCount = 0;
    std::uint64_t triangleCandidateTotal = 0;
    std::uint32_t triangleCandidateMax = 0;
    std::uint64_t playerCapsuleCandidateTotal = 0;
    std::uint32_t playerCapsuleCandidateMax = 0;
    std::uint64_t correctionPackets = 0;
    std::uint64_t correctionBytes = 0;
    std::uint32_t activeProjectiles = 0;
    std::uint32_t movingProjectiles = 0;
    std::uint32_t sleepingProjectiles = 0;
};

struct GameServerPolicyV1 {
    std::uint32_t abiVersion = kGameServerPolicyAbiVersion;
    std::uint32_t structSize = sizeof(GameServerPolicyV1);
    std::uint32_t runtimeRevision = 1;
    int (*startup)(const ServerStartupV1*, ServerStartupResultV1*) = nullptr;
    int (*world_load)(const ServerWorldLoadV1*, ServerWorldLoadResultV1*) = nullptr;
    int (*configure)(const ServerConfigureV1*, ServerConfigureResultV1*) = nullptr;
    void* (*create_player)() = nullptr;
    void (*destroy_player)(void*) = nullptr;
    int (*make_udp_connection_id)(std::uint32_t addressNetwork,
                                  std::uint16_t portNetwork,
                                  ServerConnectionIdV1* out) = nullptr;
    int (*allocate_ice_connection_id)(ServerConnectionIdV1* out) = nullptr;
    int (*legacy_endpoint_for_connection)(const ServerConnectionIdV1* id,
                                          std::uint32_t* addressNetwork,
                                          std::uint16_t* portNetwork) = nullptr;
    int (*clear_actor_preset)(std::uint32_t playerId) = nullptr;
    int (*poll_reload)(void* npcSystem, std::uint32_t tick) = nullptr;
    int (*auth_identity)(ServerAuthIdentityV1* out) = nullptr;
    int (*projectile_perf)(ServerProjectilePerfV1* out) = nullptr;
    int (*clear_runtime_objects)(const char* reason) = nullptr;
    int (*coordinator_ice_done)(const char* roomCode) = nullptr;
    int (*shutdown)() = nullptr;
};

using GetGameServerFn = const GameServerV1* (*)(std::uint32_t abiVersion);
using GetGameServerPolicyFn =
    const GameServerPolicyV1* (*)(std::uint32_t abiVersion);

}  // namespace mimita::host
