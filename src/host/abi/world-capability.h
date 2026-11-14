// 2026-11-13
// Versioned read-only world capability for a game.dll generation.
//
// The resident host owns the authoritative collision world. The game generation
// reads bounded plain-data results; no World object, triangle container, or
// pointer into host/game memory is retained across a call.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kWorldCapabilityVersion = 2u;

struct WorldInfoV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t loaded;
    std::uint32_t triangleCount;
    std::uint32_t glbObjectCount;
    std::uint32_t spawnPointCount;
    float boundsMin[3];
    float boundsMax[3];
};

struct WorldRayHitV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t hit;
    std::uint32_t triangleIndex;
    float distance;
    float point[3];
    float normal[3];
};

struct WorldSpawnV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t found;
    std::uint32_t spawnIndex;  // 0xFFFFFFFF when derived, not authored
    float position[3];
    float yaw;
};

struct WorldAdapterV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    void* owner;

    HostResult (*info)(void* owner, WorldInfoV1* out);
    HostResult (*raycast)(void* owner, const float origin[3],
                          const float direction[3], float maxDistance,
                          WorldRayHitV1* out);
    HostResult (*pick_spawn)(void* owner, std::uint32_t playerId,
                             WorldSpawnV1* out);
};

struct WorldCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    HostResult (*info)(void* hostUserData, WorldInfoV1* out);
    HostResult (*raycast)(void* hostUserData, const float origin[3],
                          const float direction[3], float maxDistance,
                          WorldRayHitV1* out);
    HostResult (*pick_spawn)(void* hostUserData, std::uint32_t playerId,
                             WorldSpawnV1* out);
};

}  // namespace mimita::host
