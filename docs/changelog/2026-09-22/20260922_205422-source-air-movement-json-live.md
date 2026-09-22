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
