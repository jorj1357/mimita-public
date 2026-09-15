// 09 14 2026
/* purpose
* Shared hot HUD/UI state. One generic `MatchHudState` dynamic component carries
* the data a hot ui.frame system needs to compose a match HUD (timer, scores,
* phase). The mode that owns match state writes it; the HUD reads it. Hot-only
* header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_MATCH_HUD_COMPONENT = gameHash("MatchHudState");

struct HotMatchHudStateV1 {
    float timerSeconds;
    std::int32_t scoreA;
    std::int32_t scoreB;
    std::uint32_t phase;   // 0 none, 1 countdown, 2 active, 3 intermission, 4 results
    char phaseText[32];
    char labelA[24];
    char labelB[24];
};
