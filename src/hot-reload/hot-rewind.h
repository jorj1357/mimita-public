// 09 15 2026
/* purpose
* Generic rewind/lag-compensation policy payload shared between the cold history
* mechanism (which stores samples, owns timestamps, and executes the historical
* lookup/collision query) and hot policy (which owns the rewind target tick,
* latency + interpolation-delay compensation, max-rewind clamp/reject, and the
* interpolate-vs-nearest decision). POD only: no Player, Npc, Weapon, Rocket,
* snapshot, or connection pointers.
* Does NOT own history storage, sample insertion, timestamps, or collision.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

struct GameRewindPolicyV1 {
    // in: timing + history facts
    std::uint32_t currentTick;
    std::uint32_t commandTick;
    std::uint32_t acceptedClientTick;
    std::uint32_t acceptedServerTick;
    std::uint32_t historyOldestTick;
    std::uint32_t historyNewestTick;
    float measuredLatencySeconds;
    float interpolationDelaySeconds;
    float compensationSeconds;
    std::uint32_t maxRewindTicks;
    std::uint32_t historyAvailable;
    std::uint64_t attackerGeneration;
    std::uint64_t targetGeneration;
    std::uint64_t currentGeneration;
    // out: policy decision
    std::uint32_t handled;
    std::uint32_t allow;
    std::uint32_t targetTick;
    std::uint32_t clamped;
    std::uint32_t reject;
    std::uint32_t interpolate;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_REWIND = gameHash("net.rewind");
