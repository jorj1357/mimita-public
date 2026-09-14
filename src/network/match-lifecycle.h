// 09 14 2026
/* purpose
* Defines the generic match-lifecycle policy payload shared between the kernel
* (which fills authoritative defaults) and hot mode systems (which decide
* policy). Addressed by the runtime event id gameHash("match.lifecycle").
* POD only: no STL, no pointers. This is an event payload, NOT a GameAPI field.
* Does NOT own the phase state machine mechanism or any transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

struct GameMatchLifecycleV1 {
    std::uint64_t matchEntity;
    std::uint32_t tick;
    std::uint32_t phase;             // DuelStatePhase value
    std::uint32_t participantCount;
    std::uint32_t reserved;

    // in: current authoritative defaults (kernel)
    float countdownSeconds;
    float goSeconds;
    float intermissionSeconds;
    float resultsSeconds;
    float timeLimitSeconds;
    float respawnSeconds;
    std::uint32_t respawnsEnabled;

    // out: mode policy, applied only when handled != 0. A zero duration means
    // "leave unchanged"; respawns are set through outRespawnsEnabled.
    std::uint32_t handled;
    std::uint32_t outRespawnsEnabled;
    float outRespawnSeconds;
    float outCountdownSeconds;
    float outGoSeconds;
    float outIntermissionSeconds;
    float outResultsSeconds;
    float outTimeLimitSeconds;
};

static constexpr std::uint64_t GAME_EVENT_MATCH_LIFECYCLE =
    gameHash("match.lifecycle");
