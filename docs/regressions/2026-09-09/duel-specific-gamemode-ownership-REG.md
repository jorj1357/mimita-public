# Duel-Specific Ownership Breaks General Gamemodes

Time created: 2026-09-09T12:20:00Z
Time last updated: 2026-09-09T12:20:00Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/gamemodes.md`

Related changelog:
`docs/changelog/2026-09-09/20260909_121356-general-gamemode-runtime.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-09T00:00:00Z`

### Expected Behavior

All gamemodes use one general server gamemode runtime. Shared lifecycle,
participant, spawn, score, countdown, results, map, inventory, and GUI behavior
is implemented once and selected by gamemode JSON data.

### Actual Behavior

The shared runtime was named and structured as `server-duel`, with
`ServerDuelState`, `serverDuelTick`, `assignDuelSpawns`, and other duel-named
functions serving FFA, TDM, Bomb Tag, and community map behavior.

### Why This Is Bad

Duel-specific ownership encourages separate mode paths and caused real failures:
FFA deleted NPCs before participant assignment, countdown behavior diverged,
and changing the community mode changed a label without resetting the active
runtime. New modes cannot safely reuse a lifecycle whose API says it only serves
duels.

### Specification

`docs/specs/gamemodes/gamemodes.md`

Relevant requirement:

Gamemode behavior must be JSON-ID-driven and shared; functions must not be
written for only one named gamemode when the behavior is common.

### Wrong Code

File:
`src/network/server-duel.h`

```cpp
struct ServerDuelState
{
    // shared FFA/TDM/Bomb Tag fields were stored in duel-owned state
};

ServerDuelState& serverDuelState();
void serverDuelTick(/* shared player/NPC gamemode lifecycle */);
```

File:
`src/network/server-duel.cpp`

```cpp
// Drop practice NPCs.
npcs.clear();
npcSystem.destroyAll();
npcIdsAlive.clear();
assignMatchParticipants(d, players, &npcs);
```

### Confirmed Cause

The owner and API described the first duel implementation rather than the
shared behavior now required by community gamemodes. The resulting lifecycle
contained duel-only deletion and phase paths that were reached by FFA/TDM.

Evidence:

`assignMatchParticipants` already had an NPC-aware parameter, but the surrounding
FFA/TDM path deleted the NPC collection immediately before calling it. The same
runtime also contained FFA/TDM and Bomb Tag phase branches under duel-named state
and tick functions.

### Attempted Fix 1

Time:
`2026-09-09T12:20:00Z`

Change:

Renamed the primary server owner to `server-gamemode`, renamed the central state
and shared APIs to gamemode terminology, removed active `PRE_MATCH`, preserved
NPC participants, and added deferred live mode switching through a five-second
results phase.

Result:

Source changes and canonical build validation are pending. Human multiplayer
confirmation is still required; this regression is not marked solved.

### Corrected Code

File:
`src/network/server-gamemode.h`

```cpp
struct ServerGamemodeState
{
    // shared gamemode lifecycle and participant state
};

ServerGamemodeState& serverGamemodeState();
void serverGamemodeTick(/* shared player/NPC gamemode lifecycle */);
```

File:
`src/network/server-gamemode.cpp`

```cpp
assignGamemodeSpawns(d, world);
assignMatchParticipants(d, players, &npcs);
beginMatchCountdown(d, players, tick);
```

### Fix

Use general ownership and names for shared behavior. Keep only genuinely
mode-specific rules narrow; route all shared lifecycle operations through the
general gamemode manager and JSON gamemode ID.

### Proof

Human review:

Pending live FFA/TDM/Bomb Tag testing.

Automated proof:

Pending canonical build and focused validation.

### Solution

Not yet confirmed. A solution requires human verification that all supported
gamemodes share the renamed lifecycle without regressions.
