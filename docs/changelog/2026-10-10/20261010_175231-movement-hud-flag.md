# Movement HUD flag

- EST timestamp: 2026-10-10 17:52:31 -04:00
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and final state

The movement/camera diagnostic line shown after `entity_visibility on` is now
independently controlled. It is off by default and is no longer enabled by the
broader render/entity debug flag.

## Exact source changes

- `src/config.h`, `DebugConfig`: added `DEBUG_MOVEMENT_HUD = false`; added
  `DEBUG_MOVEMENT_HUD = false` to `ResetAll()`.
- `src/engine/engine-tick-ui-game-hud.cpp`, the diagnostic `uiDrawText()` block:
  changed the gate from `DebugVis::render()` to
  `DebugConfig::DEBUG_MOVEMENT_HUD`.
- `src/terminal/terminal-debug-toggles.cpp`, debug command registration: added
  `debug.movement_hud <0|1>` to toggle only this diagnostic.

Old behavior: `DebugVis::render()` controlled the line, so `entity_visibility
on` could make it appear by setting `DEBUG_RENDER = true`.

New behavior: `DEBUG_MOVEMENT_HUD` alone controls the line. `entity_visibility
on`, `debug.render 1`, and broader render diagnostics do not enable it.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/gui/gui.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Specification/behavior review: no specification conflict found. The requested
behavior is a narrow presentation/debug-gating change. No regression record was
created because this was a requested correction, not a newly confirmed
regression.

## Validation

- `git diff --check`: the touched files are clean; repository-wide output still
  reports pre-existing whitespace in unrelated zombie-tower files.
- First canonical build attempt: failed from compiler memory exhaustion under
  default parallelism (`cc1plus.exe: out of memory`). No source error was
  reported.
- Second canonical build: `MIMITA_BUILD_JOBS=2 python build_agent.py` passed,
  compiled the affected code, linked `mimita.exe`, and returned code `0`.
- Existing MiMITA processes at `C:\mimita-v9\.dev\builds\1910\mimita.exe`
  were not terminated or modified.

## Pre-existing edits preserved

The following pre-existing edits were left untouched: account, leaderboard,
zombie-tower map/dev-loop/spec files, `src/engine/engine-tick-setup.cpp`, and
`src/gamemode/map-config.cpp`, plus the existing
`docs/changelog/2026-10-10/20261010_173832-dev-loop-zombie-tower-launch.md`.

## Human review still needed

Run the newly linked executable, execute `entity_visibility on`, and confirm the
line stays hidden. Then run `debug.movement_hud 1` and confirm the line appears;
run `debug.movement_hud 0` and confirm it disappears while other entity/render
diagnostics remain available.
