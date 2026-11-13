// 2026-11-13
// Versioned gamemode/duel state capability.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kGamemodeCapabilityVersion = 1u;

enum class GamemodeOp : std::uint32_t {
    DuelStopQueue = 1,
    DuelCountdownActive = 2,
    DuelCountdownLeft = 3,
    DuelDefaultProfileId = 4,
    DuelOnEnemySpawn = 5,
    DuelOnState = 6,
    DuelOnMapChange = 7,
    DuelRequestRematchNow = 8,
    DuelRequestRematchWith = 9,
    DuelReturnToQueue = 10,
    DuelStartQueue = 11,
    DuelUpdate = 12,
};

struct GamemodeTextArgs {
    char s0[256];
    char s1[256];
    char s2[256];
    char maps[32][64];
    std::uint32_t mapCount;
    std::uint32_t u0;
    std::uint32_t u1;
    float f0;
};

struct GamemodeCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::int32_t (*invoke)(void* hostUserData, GamemodeOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
