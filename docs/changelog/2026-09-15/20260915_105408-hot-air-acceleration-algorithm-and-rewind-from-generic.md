# Hot air-acceleration algorithm + rewind/history samples generic Transform

- EST timestamp: 2026-09-15 10:54:08 EDT (UTC 2026-09-15T14:54:08Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 6/6 +
  `--server-spatial-authority-selftest` 9/9 + full suite 25/25)

## 1. Concurrent movement-state audit
The concurrent agent's phase-1 work (uncommitted) is the local-player ability
state: `MovementRuntimeStateComponent` (`actor-entities.cpp`),
`movement-system.cpp` (hot `movement.main`), and the component bridge in
`live-behavior.cpp`. None of those files were edited by this pass. Ownership for
this pass: `movement-step.cpp` (shared algorithm), `server-players.cpp`
(integrator/rewind), `hot-movement-policy.h` + `movement-air.cpp` (new hot
algorithm).

## 5-6. One real movement algorithm is now hot (function, not constants)
- New `src/hot-reload/hot-movement-policy.h`: `GameAirAccelerateV1` (plain-number
  inputs + `outVelocity`) and `GAME_EVENT_MOVEMENT_AIR_ACCELERATE`.
- `applySourceAir` (movement-step.cpp) fills the inputs from the shared movement
  state and dispatches the fact. If a hot handler handles it, its velocity
  replaces the built-in formula.
- New hot `movement-air.cpp` owns the algorithm: project velocity onto wishdir,
  derive remaining headroom, apply a diminishing-returns gain, modify the
  velocity vector. This differs from the cold built-in (constant gain cap), so
  behavior is function-determined.
- Selftest proves: the hot handler runs; its output matches its function; it
  differs from the cold formula for the same input; it is deterministic; and the
  same plain-number function works for any (generic) actor state.

## 7. Rewind/history samples generic Transform
- `pushPositionHistory` now reads the generic authoritative `TransformComponent`/
  `VelocityComponent` (fallback to typed) as the history source, and the
  server-sim smoothing is derived from it. The rewind algorithm itself is
  unchanged.

## 11. Snapshot compatibility
- Wire snapshot format unchanged; its spatial values derive from the same
  generic authority after the bridge. Relevance already reads generic Transform.

## 9 / generic reuse
- The hot algorithm takes numbers only (no `ServerPlayer*`, `Npc*`, or mode
  type), so it is directly reusable by server simulation, prediction, and any
  runtime entity. A generic numeric state is covered by the selftest.

## 13. Tests
`--movement-algorithm-selftest` PASS 6/6: hot package active; hot algorithm
handles the step; output matches the projected-gain function; differs from the
cold built-in; deterministic; works for a generic numeric actor state.
`--server-spatial-authority-selftest` PASS 9/9 (unchanged). Full suite 25/25.

## 14. Live hot movement edit proof
Not run. `LIVE MOVEMENT HOT-EDIT PROVEN: no`.

## Status labels
- SELFTEST PROVEN: hot air-acceleration algorithm ownership (function-level),
  deterministic, differs from cold, generic numeric reuse; rewind samples
  generic Transform; typed<->generic spatial bridge.
- COMPILED INTEGRATION: `applySourceAir` consumes the hot result; hot
  `movement-air` registered; rewind source switched to generic; full suite 25/25.
- LIVE MULTIPLAYER PROVEN: no.
- LIVE MOVEMENT HOT-EDIT PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game movement feel under the new hot air
  algorithm, and client-prediction/server parity (the local hot `movement.main`
  currently uses a different math path).

## Honest limits (success-bar gaps)
- The movement **integrator** still runs on the typed working copy
  (`p.pos/p.vel`, `npc.body`); generic is read early and projected late. It does
  **not** yet integrate directly on generic state (success-bar items 1/2).
- Collision (`resolveWorldCollision` and the step functions) still consumes/
  returns typed state (item 4).
- The hot air algorithm changes authoritative movement behavior; this is
  intentional hot ownership but requires human verification, and the local hot
  movement path is not yet unified with it (prediction parity is the next owner).
- Wire snapshot payload remains the typed compatibility format (item 8 partial).

## Files changed
`src/hot-reload/hot-movement-policy.h` (new),
`src/hot-reload/modules/movement-air.cpp` (new),
`src/physics/movement/movement-step.cpp` (hot algorithm hook),
`src/network/server-players.cpp` (rewind generic source),
`src/network/movement-algorithm-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`; docs + this changelog.

## Next (auto-selected)
Convert the movement integrator (player + NPC) to integrate directly on generic
Transform/Velocity (typed projection only), make collision consume/return generic
movement state, and unify the server + local hot movement generation for
prediction/reconciliation parity.
