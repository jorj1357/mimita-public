// 2026-09-07T16:13:40-04:00 (2026-09-07T20:13:40Z)

# Replay rocket and leg transform parity

## Result

Implemented the approved shared replay rocket simulation and parent-relative body-part transform plan.

## Repository state

- Branch: `8292026stash`
- Commit observed at completion: `a045b37`
- Pre-existing changes were preserved. At the beginning of this implementation session, unrelated worktree changes included `config/playervisuals.json`, player-visual configuration files, and website/database work; they were not reverted or claimed.
- No commit was created by this session.

## Exact behavior changes

### Rocket simulation

Old behavior in `src/engine/engine-tick-camera.cpp`: a `projectile_spawn` event created an `EffectPart` with `replayType = "replay_rocket"`, which had no live rocket collision, explosion, or shared projectile state.

New behavior:

- `RocketLauncherState::Rocket` is created once per replay event ID.
- Replay advances `WeaponRocketLauncher::update` at fixed `1.0f / 60.0f` steps.
- `WeaponRocketLauncher::render` renders the shared rocket state using `projectileVisualConfigForWeapon` and `renderProjectile`.
- The live world collision, orientation, smoke, lifetime, and explosion visual paths are reused.
- `presentationOnly=true` suppresses damage, knockback, health, kill, and authority mutations.
- Replay suppresses live in-air/explosion audio because recorded replay sounds remain authoritative.

Files/functions: `src/combat/weapon-rocket-launcher.h`, `src/combat/weapon-rocket-launcher.cpp`, `src/combat/explosion-fx.h`, `src/combat/explosion-fx.cpp`, `src/engine/engine-tick-camera.h`, `src/engine/engine-tick-camera.cpp`, and `src/engine/engine-tick-render.cpp`.

### Left-leg transform

Old behavior in `captureReplayBodyParts()` and `Player::applyReplayPose()`: each part was flattened to root-local position/rotation and replay reconstructed it directly from the root, bypassing the original skeleton parent chain.

New behavior:

- `parentPartId` records the nearest recorded body-part ancestor.
- Capture decomposes `inverse(parentWorld) * partWorld`.
- Replay applies `parentWorld * localTransform` in two passes, independent of GLB child ordering.
- Missing metadata (`0xFF`) retains root-local compatibility for older replay files.

Files/functions: `src/replay/replay-scene.h`, `src/replay/replay-recorder.cpp`, `src/replay/replay-io.cpp`, `src/replay/replay-player-load.cpp`, `src/replay/replay-player-interp.cpp`, and `src/entities/player-render.cpp`.

### Automated coverage

`src/game/game-cli.cpp` now extends the existing replay export self-test with stable rocket event identity/weapon metadata and body-part parent metadata checks.

## Specification and focused review

- `docs/specs/replays/replay-editor-and-export-v2.md`: replay must reproduce recorded gameplay tick by tick, including projectiles, effects, and camera state.
- `docs/specs/effects/effects.md`: replay should use the same event/effect implementation path and tick-based lifetime behavior as live gameplay.
- `docs/skills/spec-behavior-review-v1.md`: PASS for the implemented source behavior; visual and live acceptance remain open.
- `docs/operations/build-and-exe/build-and-exe.md`: followed canonical executable build procedure.
- `docs/operations/task-completion/task-completion.md`: followed build, validation, changelog, and human-review separation requirements.

## Validation

- `git diff --check`: passed; only line-ending normalization warnings were reported.
- `python build_agent.py`: canonical `C:\mimita-priv-v8\mimita.exe`, `Status: SUCCESS`.
- `mimita.exe --replay-export-selftest --timeout 60 --no-coordinator`: `28/28 passed, 0 failed`.

## Remaining human acceptance

Fresh in-game export remains required to verify no tracer appears for rockets, sound/projectile same-tick timing, live-speed travel, wall collision, one explosion, smoke decay, no repeated effects/audio, first/second/third camera initialization, and correct left/right leg orientation. The automated test does not yet render a real wall collision or compare live and replay leg world-axis vectors.
