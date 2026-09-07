# Source movement ground/air strafe separation

- Branch: current working branch (not changed by this session)
- Commits: none created
- Timestamp: 2026-09-07T20:51:24Z
- Request: establish Source mode as the TF2/Source-like movement baseline and
  ensure air-strafe logic is airborne-only.

## Pre-existing work

The worktree already contained unrelated modified files before this session,
including `config/accounts/default.json`, `config/analytics.json`,
`docs/regressions/regressions-v1.md`, `src/game/game-cli.cpp`,
`src/network/multiplayer-projectiles.cpp`, prior replay-related files, and
existing untracked replay changelog files. Those changes were preserved and
were not claimed as part of this task.

## Changes

### `docs/specs/movement/movement.md`

- Added the exact ISO 8601 UTC comment at lines 8-12 establishing Source mode
  as the fundamental TF2/Source-like model and air strafing as airborne-only.
- Replaced the conflicting “Ground and air use the same instant-control
  behavior” rule at lines 377-379 with wording that preserves horizontal input
  while separating Source ground and air rules.
- Added the Source mode baseline at lines 412-446. It defines ground friction
  and acceleration, ordinary grounded lateral WASD movement, the prohibition
  on grounded PM_AirAccelerate/air-strafe logic, the airborne branch, optional
  layered mechanics, the dispatcher contract, and jump/autobhop transition
  behavior.

### `src/physics/movement/movement-types.h`

- Added `MovementAirDebug::Branch` with `None`, `Ground`, and `Air` values at
  lines 169-179.
- Updated the adjacent diagnostic ownership comment so the debug state records
  the selected Source movement branch, not only air acceleration results.

### `src/physics/movement/movement-step.cpp`

- `applySourceMovement()` now records the ground branch at lines 1366-1374 and
  the air branch at lines 1375-1377.
- `applySourceAir()` now records the air branch and defensively returns if it is
  ever called while grounded at lines 1253-1258. This protects the contract at
  the air-function boundary in addition to the dispatcher branch.
- Grounded A/D remains ordinary Source ground movement; no ground tuning or
  lateral movement was removed.

### `src/terminal/movement-commands.cpp`

- The existing `movement_air` diagnostic now prints `branch=ground|air|none`
  beside the mode and grounded state at lines 240-252, making the branch
  decision visible without adding per-frame logging.

### `tests/movement-source-parity-test.cpp`

- Added a grounded A test at lines 195-206 verifying that grounded A selects
  the ground branch, still provides ordinary lateral ground movement, and does
  not report air-strafe acceleration.

## Validation

- `python devscripts\\run-movement-tests.py`: PASS
  - movement-bhop-test: 18/18
  - movement-csgo-test: 21/21
  - movement-source-parity-test: 33/33
- `git diff --check`: PASS; only line-ending normalization warnings were
  reported for existing Windows working-copy files.
- `python build_agent.py`: PASS
  - canonical output: `C:\mimita-priv-v8\mimita.exe`
  - build changelog status: `SUCCESS`
  - compiled 198 objects and linked the executable successfully.
- Focused documentation/specification behavior review: applied. The spec now
  explicitly separates grounded Source movement from airborne air strafing.
- Documentation checker: applied to the changed specification. No routing or
  unsupported-claim issue was found in the changed sections.
- Logging checker: applied to the changed diagnostic path. The existing
  terminal command remains user-triggered rather than per-frame, and now shows
  the branch decision and grounded state.

## Remaining human review

Human playtesting is still needed to confirm that the active Source preset
feels TF2-like in a live game: normal grounded A/D walking, airborne A/D air
strafing, jump/autobhop transitions, and speed gain should be checked visually
and by feel. The automated tests prove the shared-kernel branch contract, not
the final subjective movement feel.

## Regression record

No confirmed regression entry was appended. The change adds focused movement
coverage and formalizes the requested Source-mode behavior; existing unrelated
edits to `docs/regressions/regressions-v1.md` were preserved.
