# Restore grounding, jumping, and walk animations in the collision package

- EST timestamp: 2026-09-17 13:00:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Fix the report: "no animations, I can't move, I can't jump, I can't leave the
ground" after the collision package migration, while keeping the package as the
single hot collision owner and making the tuning live-editable.

## Root cause

Three separate defects in `collision.main` starved `grounded`, so the movement
system never armed a jump and the animation policy never saw a grounded actor:

1. A capsule resting on the floor sits at exactly `distance == radius`, and the
   narrowphase discarded it (`dist >= radius`). Grounding flickered off, gravity
   was applied, and the actor sank/pushed in a loop.
2. A fast landing stopped a fraction above the floor (no settle), so no ground
   contact existed at rest.
3. The result had no contact/ground hysteresis, so a single missing contact tick
   dropped `grounded` even when the old cold path would have held it.

An additional defect: ground contacts used the bounce policy, so a floor landing
reflected the actor upward instead of settling.

Note: the dash/freeze/down-dash effects reappearing was not a new feature. Those
spawn calls run after a *successful* collision solve, so when the package started
reporting `handled` again they resumed firing.

## Changes

- `src/hot-reload/packages/collision/collision-package-world.{h,cpp}`
  - `gatherSphereHits` takes a `tolerance` and returns touching-only hits
    (`penetration 0`, `touching = 1`) as well as penetrating hits.
- `src/hot-reload/packages/collision/collision-package-solver.cpp`
  - Live-editable constants: `kContactTolerance = 0.02f`,
    `kGroundMaxHeightAboveFeet = 0.15f`, `kWalkableSlopeDot = 0.80f`,
    `kContactHysteresisSeconds = 0.033f`, `kStableGroundGraceSeconds = 0.08f`,
    `kGroundSettleDistance = 0.25f`, `kGroundSettleEpsilon = 0.005f`.
  - Ground classification now uses the old cold rule: walkable normal AND the
    contact point near the feet.
  - Ground contacts settle (cancel into-ground velocity) instead of bouncing;
    bounce is reserved for walls, ceilings, and body parts.
  - `settleToGround` reproduces old cold `doGroundSnap`: a walkable surface
    within `kGroundSettleDistance` below the feet pulls the actor to resting
    contact, so a fast landing rests exactly on the floor.
  - Contact/ground hysteresis per stable EntityId (never a pointer), matching old
    `Player::GroundState`; ground persists only through the grace window after
    real ground, never from a wall contact alone.
  - `collisionResetRuntimeState()` clears bounce cooldowns and hysteresis so the
    candidate self-test starts clean.
- `src/hot-reload/packages/collision/collision-package-world.cpp`
  - A partial page-in (fewer triangles than the probe reported) no longer
    publishes a short index as ready; the solve declines and retries instead of
    colliding against missing geometry.
- `src/hot-reload/modules/movement-system.cpp`
  - The package-decline path integrates plainly and explicitly sets
    `grounded = 0`, `collided = 0`, so state is never left undefined and there is
    still exactly one collision owner.
- `src/hot-reload/packages/collision/collision-package-selftest.cpp`
  - New test 11: a resting capsule must stay `grounded` and `worldContact` across
    12 solves with no flicker and must not sink.
  - Test 8 now falls over multiple ticks and asserts a landing.
  - `carryOver` advances root and collider positions together; tests no longer
    carry over before the first solve.

## Evidence

- Standalone harness against the package sources reproduced the exact bug
  (actor hovering at `z = 0.602`, `grounded = 0`) and then confirmed the fix
  (settles at `z = 0.505`, `grounded = 1` on every tick).
- DLL build: `python build_game_dll.py` succeeded for the self-test path and
  `python devscripts/live-build.py` produced
  `build/hotreload/mimita-live-g000046.dll`; no `MiMITA.exe` is written.
- `mimita-20260917T151237.exe --live-code-selftest` reported
  `[LIVE CODE SELFTEST] PASS` with `[ok] GameAPI load + ABI + self-test`
  (includes the collision-package floor/wall/slope/gather/large/cache/invalid/
  fast-fall/multi-part/resting tests).
- `git diff --check` passed (exit 0).
- No `.exe` build was required: no manifest `cold` source changed, and no running
  process was killed, restarted, or replaced.

## Important build note

`--live-code-selftest` loads `build/mimita-game.dll`, which
`devscripts/live-build.py` does **not** refresh (it writes
`build/hotreload/mimita-live-gNNNNNN.dll`). After editing collision sources, run
`python build_game_dll.py` before the self-test or verification will use a stale
DLL. This cost several confusing iterations and is recorded here deliberately.

## Human verification still required

- Walk animation plays and the actor can leave the ground and jump.
- Falling from height lands and stays grounded; no void fall.
- The dash/freeze/down-dash effects still fire (they should, and now they do).
- Change `kContactTolerance` or `kGroundSettleDistance` in
  `collision-package-solver.cpp` live and confirm the behavior changes.

## Not done (later phases)

- Entity-vs-entity collision, NPCs, remote players, projectiles, weapons, props,
  ragdolls, triggers.
- The 0.02 tolerance and 0.25 settle are first-pass values; they are live-editable
  for tuning and may need adjustment on slopes and stairs.
