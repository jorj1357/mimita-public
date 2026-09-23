// 09 23 2026
/* purpose
* Define the generic, hot-replaceable respawn rule (initial death timer and the
* per-tick countdown) shared by players and NPCs, and the ONE implementation
* shared by the cold EXE fallback and the hot provider. The EXE owns the
* dead/revive state machine, spawn selection, and the loadout reset; a hot
* module owns whether respawns happen and how long they take.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own actor state, spawn selection, or transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

struct GameRespawnRuleV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t respawnsEnabled;
    std::uint32_t instantRespawnRequested;
    float respawnSeconds;      // configured delay
    float timer;               // current countdown timer
    float dt;
    // out
    float outRespawnSeconds;   // initial timer on death (-1 when disabled)
    float outTimer;            // countdown result
    std::uint32_t stayDead;    // one-life: never revive
    std::uint32_t instantApplied;
    std::uint32_t readyToRespawn;
    std::uint32_t result;
};

using GameRespawnInitialFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameRespawnRuleV1* request);
using GameRespawnTickFn = void (MIMITA_GAME_CALL *)(void* host,
                                                    GameRespawnRuleV1* request);

struct GameRespawnPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameRespawnInitialFn initialTimer;
    GameRespawnTickFn tick;
    const char* name;
};

using GameRespawnLookupFn = const GameRespawnPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_RESPAWN = gameHash("net.respawn");
static constexpr std::uint64_t GAME_SIG_RESPAWN = gameHash("sig.net.respawn.v1");

// ── The single shared implementation ────────────────────────────────
namespace HotRespawnImpl {

// Initial death timer: the configured delay, or -1 so the pump never revives.
inline void initialTimer(GameRespawnRuleV1& r)
{
    r.outRespawnSeconds = r.respawnsEnabled ? r.respawnSeconds : -1.0f;
    r.result = 1u;
}

// Per-tick countdown. One-life modes stay dead; an instant request zeroes the
// timer; otherwise the timer decreases and reports readiness at <= 0.
inline void tick(GameRespawnRuleV1& r)
{
    r.stayDead = 0u;
    r.instantApplied = 0u;
    r.readyToRespawn = 0u;
    r.outTimer = r.timer;
    r.result = 1u;

    if (!r.respawnsEnabled) {
        r.stayDead = 1u;
        return;
    }
    if (r.instantRespawnRequested) {
        r.instantApplied = 1u;
        r.outTimer = 0.0f;
    }
    r.outTimer -= r.dt;
    if (r.outTimer <= 0.0f) {
        r.outTimer = 0.0f;
        r.readyToRespawn = 1u;
    }
}

} // namespace HotRespawnImpl

} // namespace MimitaNet
