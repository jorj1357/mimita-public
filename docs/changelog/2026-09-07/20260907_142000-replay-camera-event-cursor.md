# Replay camera serialization and export event cursor

- Branch: `8292026stash`
- Starting commit: `fe983bc`
- Session timestamp: `2026-09-07T18:20:00Z`
- Scope: camera POV serialization and duplicate replay-event delivery during export.
- Pre-existing changes: the prior replay/effect implementation and prior replay regression/changelog entries were preserved. This session did not claim authorship of those changes.

## Exact changes

### `src/replay/replay.h`

- Old declaration: `void seekToTick(uint32_t tick);`
- New declaration: `void seekToTick(uint32_t tick, bool resetEvents = true);`
- Purpose: allow export redraws to rebuild the requested frame without resetting historical event-delivery identity, while preserving reset behavior for normal editor/user seeks.

### `src/replay/replay-player.cpp`

- Old behavior: every `seekToTick()` assigned `mLastEventTick = tick - 1`, cleared `mDeliveredEventIds`, and reset delivery counters.
- New behavior: those event-state resets occur only when `resetEvents` is true. The default remains true for existing callers.

### `src/engine/engine-tick-replay.cpp`

- Old export call: `gReplayPlayer.seekToTick(seekTick);`
- New export call: `gReplayPlayer.seekToTick(seekTick, false);`
- Reason: the capture loop can redraw the same historical tick while the encoder is busy; redraws must not replay the same event.
- Old camera serialization: `glm::vec3(camera.pitch, 0.0f, player.yaw)`.
- New camera serialization: `glm::vec3(camera.pitch, camera.roll, camera.yaw)`.
- Reason: replay export must preserve the actual rendered player camera, including camera yaw and roll.

## Documentation and skills

- Read `docs/ROUTER.md`, `docs/specs/replays/replay-editor-and-export-v2.md`, `docs/specs/effects/effects.md`, `docs/regressions/regressions-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, and `docs/operations/task-completion/task-completion.md`.
- Appended the confirmed cursor regression and validation evidence to `docs/regressions/regressions-v1.md`.

## Validation

- `git diff --check`: passed before build.
- `python build_agent.py`: passed; `Status: SUCCESS`; canonical `mimita.exe` linked.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: passed `26/26`, including replay camera advancement and valid MP4 output.
- No fresh human three-export visual acceptance was available in this session.

## Remaining human and implementation review

- Verify first, second, and third live exports after a fresh process. If source JSON still starts at zero, inspect the new camera-ready recording logs; export seeding cannot repair invalid source frames.
- Replace replay-only projectile/effect reconstruction with shared gameplay projectile and hit-effect entry points. The current pure `simulateProjectileTick()` API needs a replay-safe gameplay adapter that applies recorded outcomes without live authority side effects.
- Repair shared effect lifetime/pool behavior and compare left/right leg transforms against the model rest basis.
