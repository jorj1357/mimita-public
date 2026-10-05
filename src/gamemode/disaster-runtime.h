// Generic JSON-defined gamemode runtime - disaster runtime state + rules.
//
// Owns the pure, deterministic part of a disaster: which disaster runs, the
// per-actor weapon assignment, and the timeout resolution. It does NOT touch
// the world, damage, networking, or rendering. The authoritative server calls
// these functions and replicates the resulting state through DisasterStatePacket.
//
// Every decision is a pure function of (seed, stable actor id, stable weapon
// pool), so the server and every client can agree without extra traffic.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "gamemode/mode-pack.h"

namespace MimitaGamemode {

// Per-match disaster state. Stored by the server's ServerGamemodeState; the
// client mirror keeps the replicated subset.
struct DisasterState
{
    bool configured = false;   // a disaster is declared for the active match
    bool active = false;       // the disaster is currently running
    std::string packId;        // owning mode pack id
    std::string disasterId;    // selected disaster id
    std::string name;          // display name
    std::string description;   // display description
    uint32_t seed = 0;         // authoritative round seed
    uint32_t startTick = 0;    // tick the disaster became active
    uint32_t durationTicks = 0;// bounded duration (60 Hz ticks)
    std::string winPolicy;     // "last_actor_alive"
    std::vector<std::string> weaponPool;
    // Deterministic per-actor assignment produced at disasterBegin.
    std::unordered_map<uint32_t, std::string> weaponByActor;
    bool resolved = false;
    uint32_t winnerActor = 0;
    uint8_t resolveSource = 0; // 0=none, 1=last_alive, 2=timeout
};

// Choose which declared disaster runs. Deterministic in (seed, pack id, the set
// of disaster ids). Returns nullptr when the pack declares no disaster.
const DisasterDefinition* disasterSelect(const ModePack& pack, uint32_t seed);

// Configure `state` from the selected disaster. `fallbackWeaponPool` is used
// when the manifest omits a weapon_pool. No-op when the pack has no disaster.
void disasterConfigure(DisasterState& state, const ModePack& pack, uint32_t seed,
                       const std::vector<std::string>& fallbackWeaponPool = {});

// Enter the active phase: assign each actor a weapon deterministically.
void disasterBegin(DisasterState& state, const std::vector<uint32_t>& actors,
                   uint32_t startTick);

// Weapon declared for an actor this disaster, or nullptr when none.
const std::string* disasterWeaponForActor(const DisasterState& state, uint32_t actorId);

// True when the bounded duration has elapsed. Does not decide the winner.
bool disasterDurationElapsed(const DisasterState& state, uint32_t tick);

// Deterministic timeout winner from the surviving actor ids. Stable regardless
// of the order the survivors are provided in. Returns 0 for an empty list.
uint32_t disasterPickTimeoutWinner(uint32_t seed, const std::vector<uint32_t>& survivors);

// World-independent selftest for the pure disaster rules. Returns true on
// success and fills `report` with a human-readable summary.
bool disasterRuntimeSelfTest(std::string& report);

} // namespace MimitaGamemode
