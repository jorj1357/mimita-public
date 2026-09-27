# Live-code hot reload: working reference and the `* 1.1` rejection lesson

Date recorded: 2026-09-12 06:02 EDT (2026-09-12T10:02:00Z)
Branch: `8292026stash`
Working tree: dirty (uncommitted; many pre-existing edits preserved)

This is a reference example of a complete, running hot-reload result: what was
changed, how the data path works, one real failure observed in a live game, the
root cause, and the fix. It is written to be followed by a human or an AI.

## What works today

The game stays running while a hot source is edited, saved, compiled in the
background, validated, and activated at a safe tick. There is one reload system
(`GameAPI`), version 3, with named module tables. The EXE owns world, identity,
actor state, sockets, rendering, audio, and notifications. The DLL only receives
plain-data envelopes.

Confirmed by `mimita.exe --live-code-selftest`:

```text
[ok] utc millisecond format
[ok] sha256 empty string
[ok] journal active
[ok] journal line written
[ok] journal utc field
[ok] journal valid JSON object
[ok] GameAPI load + ABI + self-test
[ok] actor module present
[ok] actor chooseCommand
[ok] actor chooseRole
[ok] presentation damage format
[ok] presentation rocket trail
[LIVE CODE SELFTEST] PASS
```

## The user's live experiment

While the game was running (networking active), this edit was made in
`src/effects/effect-part.cpp`, inside `gameUpdateEffects`:

```cpp
// before
effect.position[0] += effect.velocity[0] * dt;

// after
effect.position[0] += effect.velocity[0] * dt * 1.1f;
```

Observed behavior:

- after Ctrl+S the game reported the candidate **failed**;
- reverting the edit made it work again;
- the live game, networking, and session were never interrupted.

That is the pipeline doing its job (rejecting a candidate and retaining the old
generation), but it rejected an edit that should have been accepted.

## Root cause

The effect module's deterministic smoke test pinned the exact numeric output of
the very function being edited. The old test ran the updater once and required
`position[0] == 0.1`. With `* 1.1` the result became `0.11`, so validation
failed and `tryActivateCandidate` rejected the candidate before activation.

A smoke test must prove the module is safe and deterministic, not freeze the
current gameplay constant. Pinning the constant makes every intended behavior
change fail validation.

## Fix

`gameSelfTest` in `src/effects/effect-part.cpp:65` now checks invariant and
determinism properties instead of one constant:

- two identical inputs produce identical outputs (determinism);
- position and velocity are finite;
- positive +x velocity still advances forward within a sane bound;
- the effect stays alive below max lifetime;
- a sticky effect does not move.

`* 1.1` now passes. Re-verified by temporarily applying `* 1.1`, rebuilding the
DLL, and running `--live-code-selftest` (PASS), then reverting.

## How the pipeline works (code map)

1. `HotReloadSystem::startup()` — `src/hot-reload/hot-reload-system.cpp:49`.
   Loads the existing `build/mimita-game.dll` as generation 1, then starts the
   worker thread (`:77`).
2. `HotReloadSystem::pollAndAdvance()` — `:80`, called each frame at the top of
   the fixed tick from `engineTickSetup` (`src/engine/engine-tick-setup.cpp`).
   This is the safe activation boundary.
3. Change detection — every 15 frames, `computeSourceHash()` SHA-256-hashes the
   files in `src/hot-reload/hot-modules.json`. A changed hash calls
   `beginBuild()` (`:100`).
4. Worker — `workerMain()` (`:147`) runs `build_game_dll.py` in generation mode
   via `runBuild()` (`:172`). The game thread never blocks.
5. Candidate — the script compiles into `build/hotreload/gen<N>/mimita-game.dll`
   and writes `build-result.json`. The active DLL is never overwritten.
6. Validation/activation — `tryActivateCandidate()` (`:218`) rejects failures and
   calls `loadCandidateFromFile()` (`:277`), which copies to a unique temp,
   `LoadLibrary`s it, checks the GameAPI version/size, calls `onReload`, then the
   module self-test.
7. Retirement/rollback — `previous_` (the prior generation) stays loaded for
   instant `rollback()` (`:359`); the generation before that is freed by
   `retireRecord()` (`:343`), guaranteeing no in-flight call is unloaded.
8. Evidence — every stage writes JSONL via `LiveEventJournal::record`
   (`src/live-code/live-journal.cpp:103`) to
   `logs/features/live-code/<yyyy-mm-dd>/live_events_<stamp>.jsonl`.
9. Feedback — `src/live-code/live-code-events.cpp` pushes notifications and
   plays `live/success` / `live/failure` (`notifyActivated` `:70`,
   `notifyCompileFailed` `:35`).

## The module contract

`src/hot-reload/game-api.h` (v3) defines:

- `GameEnvelope { stateType, stateVersion, byteSize, data, tick, codeGeneration }`;
- `GameModuleDescriptor { name, abiVersion, structSize, functions }`;
- typed POD payloads: `ActorStateV1`, `ActorCommandV1`, `ActorEventV1`,
  `DamageNumberStyleV1`, `RocketTrailStyleV1`;
- `GameSelfTestFn` for deterministic candidate validation.

No STL containers, owning pointers, live engine objects, or unstable class
layouts cross the boundary.

## The shared Actor path (Phase 3)

- `src/hot-reload/modules/actor-behavior.cpp` implements the hot functions:
  `chooseActorCommand` (`:19`), `updateActorEmotion` (`:47`),
  `chooseActorRole` (`:81`).
- The EXE adapter `applyLiveActorBehavior` (`src/npc/npc.cpp:411`, called at
  `:1179`) projects an NPC into `ActorStateV1`, runs emotion update and role
  choice, then scales the shared movement input by the returned `speedScale`.
- The adapter falls back to the existing behavior when no actor module is
  active. State (health, velocity, emotion, role) stays in the EXE.
- `Npc::liveRoleId` (`src/npc/npc.h`) is the persistent hot role field.

Editing `actor-behavior.cpp` changes NPC decision behavior live while the fight
continues.

## Live visual/UI behavior (Phase 4)

- `src/hot-reload/modules/presentation.cpp` implements `formatDamageNumber`
  (`:17`) and `rocketTrail` (`:40`).
- Damage numbers: `src/effects/effect-part-particles.cpp:28` builds a base
  `DamageNumberStyleV1` from config, lets the hot module override it (crit size
  and gold color), and uses the result.
- Rocket trail: `src/combat/weapon-rocket-launcher.cpp:318` lets the hot module
  override smoke emission rate, size, lifetime, alpha, color, speed, and spread.

## How to run the live loop

1. `python build_agent.py` (builds the EXE and the DLL).
2. Launch `mimita.exe` and enter a game.
3. Edit a hot source listed in `src/hot-reload/hot-modules.json`
   (`effect-part.cpp`, `actor-behavior.cpp`, `presentation.cpp`), then save.
4. Watch for the notifications and the success sound. `hotreload status` shows
   generations/hashes; `hotreload rollback` reactivates the previous one.
5. Read `logs/features/live-code/<date>/live_events_*.jsonl` for exact UTC
   evidence.

## Lessons

- A deterministic smoke test must assert invariants and determinism, never the
  current tuned constant, or it blocks legitimate behavior changes.
- Rejecting a candidate and keeping the old generation is correct; the failure
  must still be visible (notification + journal) so the developer knows why.
- Hash-based detection plus generation-stamped files is what makes "revert and
  it works again" safe without touching the running session.
