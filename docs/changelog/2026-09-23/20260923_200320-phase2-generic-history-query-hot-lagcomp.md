# Phase 2: generic history.query + hot lag-compensation selection

Date (UTC): 2026-09-23T20:03:20Z
Status: implemented; build and selftests verified; live two-process run pending

## Scope

Phase 2 of the hot server/networking migration
(`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/specs/networking/networking.md`): move lag-compensation ordering behind the
hot boundary over an EXE-owned history store. The EXE keeps the bounded player/
NPC history samples, timestamps, and generation tags; a hot policy owns the
interpolate / nearest / cross-generation-clamp decision. Fully generic: no
`GameplayContextV1`/`GameAPI` field, no per-actor call site.

## Changes

1. **Shared ABI** (`src/hot-reload/hot-history.h`, new, in the manifest headers):
   - `GameHistoryQueryV1` (entity, domain Player/Npc, target/current tick; out
     pose + selection + bracketing ticks + generation ids);
   - `GameHistorySampleV1` and `GameHistorySelectV1` (raw bracketing samples;
     out selection/fraction/pose);
   - `HistoryDomainV1`, `HistorySelectionV1` (exact/interpolated/nearest/generation
     clamp);
   - capability id `history.query` and event id `history.select`.

2. **Cold store + capability** (`src/network/server-history.{h,cpp}`, new):
   `serverHistoryRawSamples` extracts the bracketing samples from the real
   `ServerPlayer::posHistory` / `ServerNpc::posHistory` via the server context;
   `serverHistoryQuery` dispatches the hot `history.select` policy and writes the
   result, with a cold nearest/interpolate fallback. Registered as the kernel
   capability `history.query` (signature `sig.history.query.v1`).

3. **Hot selection policy** (`src/hot-reload/modules/history-select-policy.cpp`,
   new): `history.select` owns exact/interpolated/nearest-older and the
   generation-boundary clamp. This is the logic that was previously hardcoded in
   `getPlayerPoseAtTick` / `getNpcPoseAtTick`.

4. **Live rewind paths routed through the policy**: `getPlayerPoseAtTick`
   (`server-players.cpp`) and `getNpcPoseAtTick` (`server-npcs.cpp`) now build the
   bracketing samples once and consult the hot `history.select` seam before the
   cold fallback. This makes hitscan, melee, and the previously-known-gap
   **explosion victim rewind** all use the same hot-owned selection, since they
   all call these two functions.

5. **Cold/hot oracle** (`src/network/lagcomp-history-selftest.{h,cpp}`, new,
   `--lagcomp-history-selftest`): builds real `ServerPlayer`/`ServerNpc` history,
   runs the cold pose lookup and the hot policy on identical inputs, and asserts
   parity for exact, interpolated, clamped, and F/G-boundary cases.

## Evidence

- Source changes: `hot-history.h`, `modules/history-select-policy.cpp`,
  `server-history.{h,cpp}`, `server-players.cpp`, `server-npcs.cpp`,
  `live-behavior.cpp`, `lagcomp-history-selftest.{h,cpp}`, `game-cli.cpp`,
  `hot-modules.json`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T160236.exe`). A running `mimita.exe` was not touched.
- Automated tests (test evidence):
  - `--lagcomp-history-selftest` -> PASS (8 checks: cold exact/interp/clamp;
    hot exact/interp/generation-clamp/nearest; cold/hot NPC parity).
  - `--live-code-selftest` -> PASS, incl. `history.select` registered.
  - `--rewind-policy-selftest`, `--reconciliation-policy-selftest`,
    `--interpolation-policy-selftest`, `--hitscan-target-selftest`,
    `--hitscan-outcome-selftest`, `--movement-algorithm-selftest`,
    `--packet-codec-selftest`, `--transport-generation-selftest` -> PASS.
- Runtime evidence: none. No live two-process session was observed.
- Human acceptance: pending.

## Known, unrelated failures

`--hot-combat-selftest` still fails its animation/phase2 checks (7 failures:
unequip harden, slash, lunge, death, respawn, arm poses). These are in
`modules/presentation/tool-visuals.cpp` / animation paths modified concurrently
by another session, are unrelated to rewind, and their count varies between runs.

## Not done

- No live edit proof (change `history-policy.cpp` and observe rewind ordering
  change in a running session).
- The rewind **target tick** policy (`net.rewind`) and this **selection** policy
  (`history.select`) are separate seams; both are hot, but no single live edit
  changes both at once.
- Live two-process lag-compensation trace remains.
