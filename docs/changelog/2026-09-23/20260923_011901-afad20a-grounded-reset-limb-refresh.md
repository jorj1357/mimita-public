# afad20a movement/collision fixes — grounded reset + limb transform refresh

Date: 2026-09-23
Status: hot-code built; deterministic tests pass; human live acceptance pending

Related specification: `docs/specs/movement/movement.md`,
`docs/architecture/collision/collision.md`

Related regression: `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Reported symptoms

- Q down-dash on the ground was no longer repeatable: pressing Q repeatedly did
  nothing, but moving/WASD caused abilities to reset.
- Limbs still pass through world geometry and do not push the player root.

## afad20a vs on-disk comparison (movement + collision)

- `src/physics/movement/movement-step.cpp`: on disk is the afad20a kernel plus
  hot-dispatch seams (`LiveBehavior::dispatchGameplayEvent64` for dash,
  down-dash, freeze, gravity, jump, walk, speed policy/clamp). The cold
  functions themselves still match afad20a, including the synthetic Ground
  contact in `consumeMovementContacts` (`contacts.empty() && collision.onGround`).
- `src/physics/movement/physics-collision-body.cpp`: on disk is the afad20a
  per-limb producer (`computeBodyPartCenter`, one sphere per
  `Player::physicalBody.parts` entry, `worldTransform`/`previousWorldTransform`
  sweep, radius `max(half-extents, 0.15)` clamped to `0.35`) plus a hot tool
  transform override. The producer logic matches; it is only reachable on the
  cold fallback path.
- The **hot** path (`movement.main` + `collision.main`) had diverged:
  1. Ability reset was gated only on `st.collided` (solver `worldContact`),
     missing afad20a's synthetic-ground rule, so a standing player did not reset
     down-dash/dash availability.
  2. The physical body was not refreshed before the collision read unless a
     stored hot pose existed, so `Player::physicalBody.parts` could be stale or
     empty and the per-limb colliders were wrong/absent.

## Fixes

- `src/hot-reload/modules/movement-system.cpp` and
  `actor-movement-system.cpp`: the post-collision reset now fires on
  `st.collided || st.grounded`, matching afad20a's synthetic Ground contact. A
  grounded player therefore restores dash/down-dash/freeze/jump availability
  every tick, so a fresh Q press down-dashes again. Logging of the reset record
  was narrowed to real availability transitions or a fired down-dash.
- `src/hot-reload/modules/movement-system.cpp`
  `applyStoredPoseBeforeCollision`: now always calls `skeleton.apply`, even with
  an empty pose, so `Player::updateModelWorldTransforms()` runs at the collision
  boundary. This is the afad20a ordering (it called `updateModelWorldTransforms`
  at the start of the body/weapon pass) and ensures `physicalBody.parts` are
  current for the `body.parts` collider source. An existing non-empty stored pose
  is still applied verbatim, so nothing is cleared.
- Limb diagnostics: the throttled `movement.collision` record now includes
  `limbSrc=body.parts|socket`, `limbCols`, `limbHits`, `weaponCols`, and
  `weaponHits`, so the live log shows which limb source ran and whether limb
  contacts were produced.

## Evidence

Build evidence:

- Hot DLL: `python build_game_dll.py` -> `DLL build success`
  (`build/mimita-game.dll`, 82 sources). No cold rebuild required.

Test evidence (deterministic, headless):

- `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--collision-selftest`:
  PASS.

Runtime evidence:

- Not performed. The `limbSrc`/`limbHits` fields were added for the user to
  confirm on a live run; this session did not drive the game.

Human acceptance:

- Pending. Repeated ground Q down-dash and limb-vs-wall authority need a human
  playtest.

## Limits

- The grounded reset uses the solver's grounded flag (which has a short
  hysteresis window), so it resets slightly before a hard ground contact is
  returned. This matches afad20a's synthetic-contact intent.
- Limb collision still uses one proxy sphere per part (afad20a's producer); the
  visible mesh can still visually overlap geometry before the proxy contacts.
- If the live log shows `limbSrc=socket`, `Player::physicalBody.parts` was
  unavailable (model/skeleton not loaded for that entity) and the fallback path
  was used; that is the next thing to trace.
