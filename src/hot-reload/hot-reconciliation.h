// 09 15 2026
/* purpose
* Generic reconciliation-policy payload shared between the cold reconciliation
* mechanism (which gathers predicted/authoritative facts and applies the chosen
* correction) and hot policy (which owns the error metric, thresholds, and
* snap/smooth/hard-reset decision). POD only: no Player, client, or snapshot
* pointers, no generation-owned object pointers.
* Does NOT own packet receipt, history storage, or correction application.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

struct GameReconcileV1 {
    // in: facts
    float predictedPosition[3];
    float predictedVelocity[3];
    float authoritativePosition[3];
    float authoritativeVelocity[3];
    float positionError;
    float velocityError;
    float smallDistance;
    float mediumDistance;
    float majorDistance;
    std::uint32_t predictedTick;
    std::uint32_t authoritativeTick;
    std::uint32_t gapTicks;            // ticks since last authoritative snapshot
    std::uint32_t lifecycleChanged;    // spawn/respawn/teleport/reconnect pending
    std::uint64_t predictedGeneration; // hot behavior generation of predicted state
    std::uint64_t authoritativeGeneration;
    // out: policy decision
    std::uint32_t shouldCorrect;
    std::uint32_t correctionMode;      // 0 none, 1 small, 2 medium, 3 major, 4 hardReset
    std::uint32_t hardReset;
    std::uint32_t replayInputs;
    std::uint32_t handled;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_RECONCILE = gameHash("net.reconcile");
