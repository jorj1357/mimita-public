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

// Why a reconciliation decision was made. A hot-code generation mismatch is a
// sync/bootstrap condition, never an ordinary position correction. This enum is
// cold-side classification only; it is not stored in the payload so the struct
// size stays ABI-stable for a running executable.
enum GameReconcileReasonV1 : std::uint32_t {
    GAME_RECONCILE_REASON_NONE = 0,
    GAME_RECONCILE_REASON_LIFECYCLE_SNAP = 1,      // spawn, respawn, teleport
    GAME_RECONCILE_REASON_POSITION_DIVERGENCE = 2, // genuine distance error
    GAME_RECONCILE_REASON_GENERATION_BOOTSTRAP = 3 // client/server hot code sync
};

// Correction modes (numeric values are the cross-boundary contract).
enum GameReconcileModeV1 : std::uint32_t {
    GAME_RECONCILE_MODE_NONE = 0,
    GAME_RECONCILE_MODE_SMOOTH = 1,
    GAME_RECONCILE_MODE_MEDIUM = 2,
    GAME_RECONCILE_MODE_SNAP = 3,
    GAME_RECONCILE_MODE_HARD_RESET = 4
};

// `hardReset` values. 2 = generation bootstrap: no position change, hand off to
// the generation-bootstrap sync path. Encoding it here keeps the payload size
// unchanged so an already-running executable can hot-load this policy safely.
enum GameReconcileHardResetV1 : std::uint32_t {
    GAME_RECONCILE_HARD_RESET_NONE = 0,
    GAME_RECONCILE_HARD_RESET_APPLY = 1,
    GAME_RECONCILE_HARD_RESET_BOOTSTRAP = 2
};

// `GameReconcileV1.reserved` carries the apply mode the cold client must honor.
// This moves "what to do with the correction" into the hot policy without
// changing the payload size (the running executable keeps a valid layout).
enum GameReconcileApplyV1 : std::uint32_t {
    GAME_RECONCILE_APPLY_NONE = 0,        // keep local prediction
    GAME_RECONCILE_APPLY_SMOOTH_ONCE = 1, // apply server state once, then resume
    GAME_RECONCILE_APPLY_SNAP = 2         // hard apply server state + velocity
};

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
    std::uint32_t correctionMode;      // GameReconcileModeV1
    std::uint32_t hardReset;           // GameReconcileHardResetV1
    std::uint32_t replayInputs;
    std::uint32_t handled;
    std::uint32_t reserved;
};

static constexpr std::uint64_t GAME_EVENT_RECONCILE = gameHash("net.reconcile");
