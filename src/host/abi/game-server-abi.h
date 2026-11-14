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

using GetGameServerFn = const GameServerV1* (*)(std::uint32_t abiVersion);

}  // namespace mimita::host
