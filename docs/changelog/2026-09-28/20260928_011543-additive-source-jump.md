# Additive Source Jump

- Timestamp: 2026-09-28 01:15:43 -04:00 (EST/EDT local project time)
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Make Source-mode jumping additive instead of replacing vertical velocity, while
keeping the old replacement behavior selectable and documented.

## Implementation

- `src/physics/movement/movement-types.h`
  - Added `MovementJumpMode::{Replace, Additive}`.
  - Added `MovementConfig::jumpMode`, defaulting to `Replace` so missing or
    older presets retain the legacy fallback.
- `src/config/movement-config.cpp`
  - Added JSON parsing for `jump_mode: "replace"` or `"additive"`.
- `src/physics/movement/movement-step.cpp`
  - Added one shared `applyJumpVerticalVelocity` owner.
  - Ground and air jumps now use `+= jumpVelocity` when the mode is additive;
    replacement remains `= jumpVelocity`.
- `config/movement/movement-source.json`
  - Set the active Source preset to `"jump_mode": "additive"`.
  - Added the switch-back comment: change it to `"replace"`.
- `tests/movement-basic-kernel-test.cpp`
  - Added direct coverage for additive and legacy replacement jump results.

## Specification and focused review

- Read `docs/ROUTER.md`.
- Read `docs/specs/movement/movement.md`.
- Read `docs/architecture/collision/collision.md`.
- Read `docs/skills/spec-behavior-review-v1.md`.
- Finding: the requested additive behavior is a deliberate movement-mode
  extension. The existing code assigned `baseVelocity.z` in both jump paths;
  the new config policy keeps one shared owner and preserves an explicit legacy
  mode. No new timer, packet, collision query, or authority path was added.

## Validation

- `python devscripts/run-movement-tests.py`: PASS; bhop 18/18, CSGO 21/21,
  Source parity 33/33.
- Standalone C++ syntax checks: PASS for
  `src/physics/movement/movement-step.cpp`,
  `src/config/movement-config.cpp`, and
  `tests/movement-basic-kernel-test.cpp`.
- `git diff --check`: PASS.
- Full cold executable build: not run because active MiMITA processes were
  detected; no process was stopped or restarted.

## Human review still required

Playtest a falling jump, an upward-moving jump, and Source auto-bhop/air-jump
chaining in the running game. Build/runtime and user-visible feel remain
unproven by the focused tests.

## Pre-existing work preserved

Unrelated edits in `config/accounts/default.json`, `config/analytics.json`,
`src/physics/movement/actor-triangle-solver.cpp`,
`src/physics/movement/physics-collision-core.cpp`, and
`src/physics/movement/physics-collision-mesh.cpp`, plus the pre-existing
untracked changelog, were not modified.
