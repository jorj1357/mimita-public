# Hot movement default + coordinated multiplayer switch-at-tick

- EST timestamp: 2026-09-14 11:12:04 EDT (UTC 2026-09-14T15:12:04Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); runtime human/multiplayer proof pending

## Part 1 — hot movement is the default

- `movement.main` now runs unless the new `GAME_MODE_FLAG_LEGACY_MOVEMENT` flag is
  set (`hotmovement 0` opts back into the built-in step; `hotmovement 1`/default
  is hot). Create mode still selects free-fly.
- Added approximate movement features to the hot step so it is a viable default:
  ground/air acceleration (air-strafe), ground friction, jump, **dash** (grounded,
  duration + cooldown), **down-dash** (air), and **freeze/hover** (zero velocity
  while held). All tuning remains hot-editable in `MovementTuning`.
- The double-gravity fix from step 4b stands: the capsule solve applies gravity
  once via `MovementStateV1.gravityScale`.

## Part 2 — coordinated switch at a tick (multiplayer)

- `HotReloadSystem` gained `candidateReady()`, `switchPending()`,
  `switchAtTick()`, `requestSwitchAtTick(tick)`, `candidateGeneration()`,
  `candidateCodeHash()`, and `pollAndAdvance(tick)`.
- `pollAndAdvance` holds a validated candidate while a switch is pending and
  activates only when `tick >= switchAtTick`, so the old generation stays live
  during the wait.
- Server (`server.cpp`): when a candidate is ready it schedules
  `switchTick = tick + 30`, announces `PACKET_CODE_GENERATION` phase 2 (SWITCH)
  with the candidate generation/hash and the shared tick, then activates at that
  tick.
- Client (`multiplayer-tick.cpp`): on a phase-2 announce it calls
  `requestSwitchAtTick(switchTick)`; the client poll
  (`engine-tick-setup.cpp`) passes `THE_PLAYER.movementSimulationTick`, so it
  activates at its local tick ≥ the shared tick.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`.
- All self-tests PASS: movement, movement-parity, creation, ragdoll-slice,
  live-code, hot-authoritative, entity-slice, project, phase456, telemetry.
- `-fsyntax-only` clean: `hot-reload-system.cpp`, `server.cpp`,
  `multiplayer-tick.cpp`, `engine-tick-setup.cpp`; DLL-side `movement-system.cpp`,
  `editor-behavior.cpp`.

## Honest limitations

- The hot step is not bit-parity with `physicsMainUpdate`; dash/freeze/air-strafe
  are simplified. The legacy opt-out exists for A/B comparison.
- Tick domains differ: the server announces a **server** tick; the client maps it
  against its **movement** tick without an exact server→client tick anchor yet, so
  switch alignment is approximate. A client whose candidate is not ready keeps
  the old generation (no crash, no desync guard beyond the existing hash warning).
- No runtime multiplayer proof yet.

## Next

- Runtime human proof (single-player) and a two-client switch proof.
- Precise server↔client switch-tick mapping via existing tick anchors; refuse
  peers that cannot match the logical manifest hash.
- Port remaining movement features to close parity, then remove the legacy flag.

## Files

Changed: `src/hot-reload/game-api.h`, `src/hot-reload/hot-reload-system.h/.cpp`,
`src/hot-reload/modules/movement-system.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`, `src/network/server.cpp`,
`src/network/multiplayer-tick.cpp`, `src/engine/engine-tick-setup.cpp`.
