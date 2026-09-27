# Source Air Movement JSON Live Path

Date: 2026-09-22
Status: hot-code fix built; human live acceptance pending

## Work performed

- Kept `config/movement/movement-source.json` unchanged as the requested
  movement source.
- Kept `movement-cs.json` out of the active path.
- Preserved the shared JSON path resolver so `source` resolves directly to
  `config/movement/movement-source.json`.
- Changed the hot v206 air policy so airborne WASD cannot launch an actor from
  rest and speed gain comes from projection against existing horizontal motion,
  matching the requested CS/GoldSrc-style air-strafe feel.
- Added the missing final horizontal speed clamp to hot `movement.main` after
  dash, down-dash, jump, and collision response, so JSON
  `speed_limit_enabled: true` and `speed_limit: 50.0` apply to the live path.
- Changed the hot landing/bhop branch to skip ground acceleration while jump is
  held, preserving emergent Quake-style behavior without remembering the held
  movement direction.
- Added a regression record documenting the behavior break and the human
  understanding/rapid-work concern.

## Evidence

- Source evidence: `config/movement.json` selects JSON preset `source`.
- Source evidence: `movement.main` passes the resolved tuning to the hot air
  policy.
- Source evidence: `movement-air.cpp` now contains the no-launch projection
  branch.
- Build evidence: hot candidate generation 5 built successfully as
  `build/hotreload/p24400/gen5/mimita-live-g000005.dll`; the follow-up speed
  clamp candidate generation 6 built successfully as
  `build/hotreload/p24400/gen6/mimita-live-g000006.dll`; `mimita.exe` was not
  written by the live-build path.
- Follow-up build evidence: generation 7 built successfully as
  `build/hotreload/p21184/gen7/mimita-live-g000007.dll`.
- Runtime evidence: activation in a running game and in-game review are still
  pending.
- Human acceptance: pending.

## Limits

This change does not claim that the movement now feels correct until the human
tests it in the running game. The required test is standstill jump plus WASD,
then existing horizontal movement plus mouse turning and A/D strafing.

## Collision follow-up: v2.0.6 body samples

Status: hot candidate built; live activation and human collision acceptance pending

- Restored the v2.0.6 player-body collision shape in the hot path: three
  authoritative sphere samples along each animated head, torso, arm, and leg.
- Kept those samples in the same `collision.main` solve as the root capsule,
  so world-triangle contacts can contribute to the player's correction.
- Increased the hot request collider capacity from 16 to 32 so the root
  capsule, weapon shape, and all 18 body samples are not silently truncated.
- The current weapon shape path remains hot and is still included before the
  body samples.

Evidence:

- Historical source: `d05c12e` built body samples from player model-node bounds
  in `src/physics/movement/physics-collision.cpp:2677-2723`, queried world
  triangles with `gatherGLBTrianglesForSphere`, and tested them with
  `sphereTriangleContact`.
- Current source: `src/hot-reload/modules/movement-system.cpp` now emits the
  equivalent three-sphere body samples from the live socket center/orientation
  into `collision.main`.
- Build evidence: hot candidate generation 18 built successfully as
  `build/hotreload/mimita-live-g000018.dll`; result status was `ok`; the
  running executable was not written or restarted.
- Runtime evidence: candidate activation, contact logs, and human testing of
  limbs pushing/hanging against world geometry remain pending.

## Hot afad20a animation and exact-body-bounds migration

Status: hot animation candidate active; exact model-part bounds require the
next intentional EXE build before they can be supplied to the already-running
process.

- Added the `afad20a` JSON animation compatibility path: `layers.animations`,
  `durationTicks` at 60 Hz, `return_to_idle`, and `weapons.*.poses.*` arm
  translations/rotations are normalized into the existing hot pose system.
- Added the JSON-only walk-release transition: movement input selects `walk`,
  release selects authored `return_to_idle`, and completion falls back to idle.
  C++ mode keeps the built-in fallback behavior.
- Added a stable read-only `mesh.part-bounds` capability. The renderer now
  retains per-model-part local AABBs, while the hot movement system uses those
  bounds to compute the old three-sphere samples and keeps the current
  approximation only when the loaded EXE does not yet expose the capability.
- Build evidence: hot candidate generation 20 succeeded with status `ok`,
  code hash `94a13b361f33fb478ef939da99440aa3d75853ae6dfee635d702876c558b6ad7`.
  The automatic per-process candidate with the same hash activated as
  generation 16 in PID 21184; the EXE and running world stayed alive.
- Runtime evidence: `events.jsonl` reports `source=json`, movement generation
  16, and collision requests with 19 colliders / 18 body samples. It does not
  yet prove exact bounds because PID 21184's already-loaded EXE predates the
  new `mesh.part-bounds` registration.
- Human acceptance is still pending: idle/walk/return-to-idle, weapon pose and
  reload checks, plus wall/floor limb contact checks.

## Activate pasted afad20a animation JSON

Status: JSON installed and validated; visual acceptance pending.

- Replaced `config/animations.json` with the supplied `afad20a` animation data.
- Enabled `"behaviorSource": "json"` so the hot loader selects it instead of
  the built-in C++ clips.
- The active file now contains `layers.animations.walk` with 30 fixed ticks,
  `return_to_idle` with 10 ticks, reload overlay data, weapon poses, and sway.
- Source validation passed through the JSON parser. The running client log
  continues to show JSON movement active; animation visual acceptance still
  requires observing the actor walk and release transitions in-game.

## Live afad20a pose smoothing

Status: hot candidate built and automatically activated; human speed check
pending.

- Added the missing hot pose smoothing step after JSON sampling, matching the
  old animation's translation/rotation spring stage instead of applying the
  target pose directly.
- Hot candidate generation 21 built successfully. The running client PID
  15708 activated the matching candidate as generation 2; its JSONL continues
  to report `source=json` and 19 collision colliders / 18 body samples.

## Hot afad20a world-contact ability reset and down-dash modes

Status: hot candidate active; interactive down-dash acceptance pending.

- Routed player and generic-actor ability resets through the shared
  `collision.main` world-contact result. Body-part classification remains part
  of collision response, but it is no longer a separate ability-reset signal.
- Preserved Q edge behavior: Q-down fires only when available; Q-up does
  nothing; a valid world contact restores the ability with no cooldown.
- Added two hot down-dash behaviors in `movement-dash.cpp`: `additive`
  preserves vertical momentum and remains the default; `set` replaces the
  vertical velocity with the authored down-dash speed. The optional preset key
  is `down_dash_mode`.
- Build evidence: generation 22 completed with status `ok`, code hash
  `6959cc7f70cdcbccd5bb2501cda32b0228568c3e7a0a8c16d6492cafc184d1a3`.
  The running client PID 15708 automatically activated its generation 4
  candidate without restarting the EXE or session.
- Runtime evidence: the client JSONL reports generation 4, `worldContact=1`,
  `grounded=1`, `colliders=19`, and `parts=18` after activation. A live
  down-dash -> contact -> re-press sequence still needs human input to prove
  the visible feel.

## Hot collision.json ground response modes

Status: JSON bounce mode active in the running client; animation testing still
separate and pending.

- Made `config/collision.json` the hot collision response source with
  `behaviorSource` and `groundResponse` settings.
- Added `groundResponse: "settle"` for the old ground behavior and
  `groundResponse: "bounce"` for treating ground like other surfaces.
- Kept the C++ fallback available when `behaviorSource` is `cpp` or the JSON
  cannot be read. The current JSON mode is `bounce`.
- The shared `collision.main` solver now reloads bounce enabled/strength,
  friction, minimum speed, maximum speed, cooldown, and ground mode while the
  EXE remains running.
- Build evidence: generation 23 completed with status `ok`. The running client
  PID 15708 automatically activated generation 5 with code hash
  `fa758dfe5cc070f6f0e9dc392aebda91ba5ed7f1093d682d17f9862fd3f02265`.
- Runtime evidence: JSONL reports `source=json`, generation 5, 19 colliders,
  18 body parts, and active world contacts after activation.
- Follow-up: test down-dash into the floor and then test the slow, full-stride
  `afad20a` walk animation separately. Animation timing, keyframe range, and
  spring/weapon overlays still need focused testing.

## Grounded-source collision diagnostics

Status: hot diagnostic candidate built; live bounce acceptance pending.

- Added JSONL collision fields for `bodyContact`, `bounced`, and
  `groundSettled` to distinguish real world contact from the old floor-settle
  probe.
- The movement collision record now reports collider count, body-part count,
  world contact, body contact, bounce result, settle-probe result, contact
  count, and final vertical velocity.
- Runtime evidence before the newest candidate showed both paths: real floor
  contact with `bounced=1`, and `groundSettled=1` with `worldContact=0` and
  `bounced=0`. This explains the intermittent feel.
- Bounce mode now skips the settle fallback so it must use an actual world
  contact. Build generation 26 completed successfully; human down-dash testing
  remains required.

## Strict afad20a-style bounce mode

Status: hot candidate active; human down-dash repetition test pending.

- Bounce mode now disables both floor-settle paths: the settle probe cannot
  ground the actor, and the final grounded epsilon path cannot zero velocity.
- Collision summaries now report `groundMode=bounce|settle`.
- Build evidence: generation 27 completed successfully.
- Runtime evidence: client PID 15708 activated generation 8. JSONL now reports
  `groundMode=bounce`, `groundSettled=0`, and real `worldContact` records.

## Restore afad20a touch reset for every world contact

Status: hot candidate active; repeated dash/down-dash human acceptance pending.

- Added one shared hot `restoreTouchAbilities` helper in
  `src/hot-reload/hot-movement-policy.h`. It restores air jump, dash,
  down-dash, and clears the dash cooldown together.
- Changed both local-player and generic-actor movement to use the collision
  package's `worldContact` result after the solve. Grounded hysteresis is no
  longer used as a substitute reset signal.
- Preserved the requested input contract: Q/Shift down is one press, Q/Shift
  up does nothing, and another press is required after the contact reset.
- Added dash availability before/after fields to the local
  `movement.contact_ability` JSONL diagnostic.
- Source validation: `git diff --check` passed.
- Build evidence: live candidate generation 18 completed successfully without
  writing or restarting `mimita-20260922T174815.exe`.
- Runtime evidence: PID 15708 activated its process generation 10 and emitted
  `movement.contact_ability` records with `collided=1`, both dash availability
  values restored, and later `down_dash_fired=1` records.
- Human acceptance remains required: press Q, contact the world, release Q,
  press Q again; repeat the same test with Left Shift and verify immediate
  chaining in-game.

## Trace Q and Shift input edges

Status: hot input trace added; human key-sequence acceptance pending.

- Added edge-only `movement.input_edge` JSONL records for `q_down`, `q_up`,
  `shift_down`, and `shift_up`.
- Each record includes the fixed simulation frame/tick visible at the hot
  boundary and current dash/down-dash availability. The logger adds UTC
  `wall_time`, monotonic `t`, PID, process, and active generation.
- The hot context exposes the fixed simulation tick rather than a separate
  render-frame counter, so `frame` is intentionally the authoritative
  fixed-tick frame used by movement.
- This makes the full proof chain observable: input edge -> ability state ->
  collision reset -> next input edge -> action fired.

## Restore afad20a pose-before-collision ordering and root fallback

Status: hot candidate active; human wall/limb/down-dash acceptance pending.

- Restored the old collision ordering at the shared hot movement owner:
  the stored hot animation pose is re-published first, then model sockets and
  mesh bounds are read, then body samples are sent to `collision.main`.
- Kept the root capsule in the same solve for wall, ceiling, and fallback
  contacts. It is no longer discarded merely because authoritative body-part
  colliders exist, so a missed limb sample cannot remove the only world contact.
- Lowered the land-impact sound throttle from `0.15` seconds to `0.01`
  seconds in `src/hot-reload/modules/movement-system.cpp`.
- All three changes are in hot DLL code or hot JSON policy; no new EXE slot or
  process restart was used.
- Source validation: `git diff --check` passed.
- Build evidence: hot candidate generation 20 built successfully and did not
  write `mimita.exe`.
- Runtime evidence: PID 15708 remained alive and activated process generation
  13. The live JSONL continued reporting 19 colliders, 18 body parts, and
  world-contact records after activation.
- Human acceptance remains required: verify that a visibly animated limb and
  the root capsule both contact walls/floors, and repeat Q/Shift after each
  contact. Animation stride timing remains a separate focused test.
