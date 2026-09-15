// 09 15 2026
/* purpose
* Generic interpolation-policy payload shared between the cold sample-buffer
* mechanism (which stores samples, owns the clock, and performs the numeric mix)
* and hot policy (which owns delay retargeting, extrapolation allow/deny+cap,
* stale/buffer-dry handling, snap/reset, and the interpolation alpha decision).
* POD only: no Player, Npc, snapshot, or network-client pointers.
* Does NOT own sample storage, packet decode, or clock access.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

struct GameInterpolateV1 {
    // in: samples + timing facts
    float aPosition[3];
    float aVelocity[3];
    std::uint32_t aTick;
    float bPosition[3];
    float bVelocity[3];
    std::uint32_t bTick;
    std::uint32_t oldestTick;
    std::uint32_t newestTick;
    std::uint32_t bufferDepth;
    std::uint32_t allowExtrapolation;
    std::uint32_t bufferDry;
    std::uint32_t packetGapTicks;
    std::uint32_t lifecycleChanged;
    double renderTick;         // global render tick (before delay)
    double delaySeconds;       // current interpolation delay
    double alpha;              // cold-computed alpha (0 if not yet known)
    std::uint64_t aGeneration;
    std::uint64_t bGeneration;
    std::uint64_t currentGeneration;
    // out: policy decision
    std::uint32_t handled;
    std::uint32_t mode;        // 0 interpolate, 1 extrapolate, 2 hold, 3 snap, 4 reset
    std::uint32_t hardSnap;
    std::uint32_t shouldResetBuffer;
    double outAlpha;
    double outDelaySeconds;
    double outExtrapolationMs;
    float outPosition[3];
    float outVelocity[3];
    // adaptive-delay query facts (delayQuery != 0): cold gathers measurements,
    // hot owns the desired interpolation delay.
    std::uint32_t delayQuery;
    float currentAdaptiveDelaySeconds;
    float baseDelaySeconds;
    float estimatedJitterMs;
    float recentLossFraction;
    float minDelaySeconds;
    float maxDelaySeconds;
    float increaseRateMsPerSecond;
    float decreaseRateMsPerSecond;
    float jitterMultiplier;
    float lossDelayBudgetSeconds;
    float deltaSeconds;
};

static constexpr std::uint64_t GAME_EVENT_INTERPOLATE = gameHash("net.interpolate");
