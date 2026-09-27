# afad20a grounded rest + landing sound + random walk

Date: 2026-09-23
Status: hot DLL built; cold build succeeded; deterministic tests pass; human acceptance pending

Related specification: `docs/specs/movement/movement.md`
Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).
Related: `docs/changelog/2026-09-23/20260923_101537-afad20a-freeze-grounded-hot.md`
Gold reference written this session:
`docs/gold/2026-09-23-afad20a-behavior-parity-hot-reload-journey.md` (the
9/22-9/23 behavior-parity journey, the input-buffer lesson, and the hot-reload
direction).

## Reported symptoms

- Standing still on the ground bounced the actor up and down ~0.1 m forever.
- Each walking sound played in the same 1-2-3-4 order instead of randomly.
- The landing sound only played on hard impacts, not on every landing.

## afad20a vs current hot (ground/collision)

`afad20a` (`git show afad20a:...`):

- `applyCollisionContact` (physics-collision-core.cpp): a walkable contact
  (`normal.z > MAX_WALKABLE_SLOPE_DOT = 0.80`) near the feet grounds the actor and
  calls `respondVelocityAgainstNormal` (bounce if enabled/minSpeed/cooldown, else
  slide).
- `doGroundSnap` (snap down within 0.25, zero downward `vel.z`) and
  `doFloorRecovery` (lift out of an embedded floor within 0.5) **always run**,
  independent of bounce.
- `applySourceGround` (movement-step.cpp) landing snap:
  `if (groundSnap && |vel.z| <= velocityClipEpsilon) vel.z = 0`, with the Source
  preset `velocityClipEpsilon = 1.01f`, `groundSnap = true`.

Current hot before this change:

- `collision-package-solver.cpp` `settleToGround` (down-snap only) and the
  `|vel.z|` snap were gated `if (!groundBounce …)`, and the hot movement step had
  no vertical snap. In `groundResponse: "bounce"` mode the bounce velocity was
  never cancelled, so a resting actor rose, lost contact, re-applied gravity, and
  bounced again.

## Changes

- `src/hot-reload/packages/collision/collision-package-solver.cpp`:
  - `settleToGround` now handles both `doGroundSnap` (down, when
    `vel.z <= 1.0`) and `doFloorRecovery` (lift out when embedded, `-distance <
    0.5`), zeroing a downward `vel.z`.
  - The settle call and the `|vel.z|` snap now run regardless of `groundBounce`
    (they never bounce). This makes `groundResponse: "bounce"` use afad20a's
    collision + grounded logic.
- `src/hot-reload/modules/movement-system.cpp` and
  `actor-movement-system.cpp`: added afad20a's `applySourceGround` landing snap
  after the grounded ground-move (`m.groundSnap && |vz| <= m.velocityClipEpsilon`
  -> `vz = 0`). A resting actor no longer oscillates; real impacts (down-dash,
  hard fall) exceed the epsilon and still bounce.
- `src/hot-reload/modules/movement-system.cpp`: the land sound now fires only on
  the airborne -> grounded transition (`!wasGrounded && q.grounded`), with no
  cooldown. It runs once per tick, so it is naturally capped at one sound per
  tick. Volume still scales with impact speed. Player only.
- `src/hot-reload/modules/presentation/effect-composition.cpp`: the footstep
  sound variant is now random (`1 + rand()%4`) instead of the fixed
  `s_step++ % 4` cycle. Cadence/volume/pitch unchanged.
- `config/collision.json`: unchanged (`groundResponse: "bounce"` is now the
  afad20a mode).

## Evidence

Build evidence:

- Hot DLL: `python build_game_dll.py` -> `DLL build success` (82 sources).
- Cold build: `python build.py build-only` -> `BUILD SUCCESS`.

Test evidence (deterministic, headless):

- `--movement-selftest`: PASS, including the new
  `grounded rest: no idle vertical oscillation` check and the existing
  `grounded down-dash: bounces up` / `fires on each fresh press` checks.
- `--movement-parity-selftest`: PASS (`hot movement vertical rest`).
- `--collision-selftest`, `--afad20a-parity-selftest`,
  `--movement-algorithm-selftest`, `--movement-v206-parity-selftest`,
  `--air-movement-parity-selftest`, `--live-code-selftest`,
  `--capability-selftest`, `--generic-integrator-selftest`,
  `--npc-actor-state-selftest`, `--server-spatial-authority-selftest`: PASS.

Runtime evidence:

- Not performed. Needs a live playtest: stand still on the ground (no bounce),
  each landing plays the land sound, footsteps are random, dash/down-dash repeat
  still works.

Human acceptance:

- Pending.

## Limits

- `--hot-combat-selftest` fails on animation-policy checks (walk/idle/death
  selection, phase2 animation state). Verified pre-existing and unrelated: the
  same failures occur with this session's hot-module changes stashed.
- afad20a's threshold is a velocity (`velocityClipEpsilon = 1.01 m/s`), not a
  distance; the plan's distance heuristic was not used.
- `config/accounts/default.json` changed (persisted equipped slot) as a side
  effect of running the game/tests; left as-is.
