# Real-path air-movement parity harness (shared function exercised by both paths)

- EST timestamp: 2026-09-15 11:18:38 EDT (UTC 2026-09-15T15:18:38Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` for the harness assertions; **full air parity NOT achieved**
  (reported as `[warn]`). Full suite 26/26.

## 1. Real path harness
New `--air-movement-parity-selftest` (`src/network/air-movement-parity-selftest.cpp`)
drives the two REAL paths with identical initial state, input, dt, and aligned
air tuning:
- **A. Server**: the real three-phase sequence
  `applyPreCollisionBasicMovement` -> `applySpecialMovementPreCollision` ->
  `applyPostCollisionMovementWithSpecials` (the walk/air step invokes the hot
  `movement.air-accelerate` hook).
- **B. Prediction**: the real `movement.main` system via
  `GAME_DOMAIN_GAMEPLAY`, read through the movement override.
No test-only shortcut bypasses the adapters.

## 2-3. Same initial conditions / air-only case
- Both start at (0,0,1000), velocity (5,0,0), wish dir (0.707,0.707), airborne,
  no world geometry (no collision), dt = 1/60, 120 ticks, server air tuning
  aligned to the local `source` preset (walkSpeed 20, airAccel 12).

## 9. Divergence diagnostics (honest)
- Early agreement: max deviation over the first 30 ticks = **1.9e-6** — both
  paths feed the shared function identical inputs.
- Full sequence: max deviation = **1.6121**; first tick exceeding 1e-3 = **84**;
  at tick 120 serverVel=(16.6421,11.6421) vs clientVel=(16.5878,11.6041).
- The harness records the first divergent tick and the per-tick deviation and
  prints a `[warn]` line. It does **not** raise the tolerance to hide the gap.

## 5-6. One-edit-changes-both
- Structural: one definition (`MimitaHotMovement::airAccelerate`) with two call
  sites (server hook handler + `movement.main`). A single edit changes both.
  **Not live-observed.**

## Success bar
- #1 real server air path exercised — yes.
- #2 real client `movement.main` air path exercised — yes.
- #3 aligned over multi-tick — **partial** (tight for 30 ticks; diverges later).
- #4 collision case — not exercised (air-only by design; both use the generic
  collision primitive but the client path also applies `physics.move`).
- #5 one edit changes both — structural only.
- #6 parity after edit — not tested.
- #7 generation/hash comparison — not added this pass.
- #8 no duplicated air algorithm — yes (single definition).

## Status labels
- SELFTEST PROVEN: the real server movement adapter and the real local prediction
  system both execute the shared hot air function and agree tightly over the
  early ticks; the harness exercises the real adapters, not the function in
  isolation.
- AIR MOVEMENT PARITY PROVEN: **no**. Divergence begins at tick 84 and grows to
  1.61 by tick 120; exact tick/diagnostics recorded.
- ONE-EDIT-CHANGES-BOTH PROVEN: **structural only** (one definition, two call
  sites); not live-observed.
- LIVE MOVEMENT HOT-EDIT PROVEN: no.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game air movement feel and prediction/authority
  alignment.

## Honest limits / next
- Root-cause the late air divergence (as the projected speed approaches the
  wish-speed cap, something besides the shared function differs between the
  server post step and `movement.main` — e.g. a speed cap/preservation or a
  wish-speed derivation difference). Do not loosen tolerance.
- Add server/client movement generation/hash observability.
- Then migrate ground acceleration/friction with the same shared-policy pattern,
  one function per pass.

## Files changed
`src/network/air-movement-parity-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`; docs + this changelog.
