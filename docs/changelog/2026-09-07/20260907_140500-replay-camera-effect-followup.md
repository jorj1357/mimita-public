# Replay camera/effect fidelity follow-up

- Branch: `8292026stash`
- HEAD before this session: `c97e782`
- Session timestamp: `2026-09-07T18:05:00Z`
- Scope: replay export camera initialization and regression evidence only; no automated tests were added for the reported effect or leg issues.
- Pre-existing worktree changes: the other agent's replay/effect edits, `config/analytics.json`, and the existing untracked replay-fidelity changelog were preserved. They were not authored or claimed by this session.

## Changed files

### `src/replay/replay-export-subprocess.cpp`

- Old behavior: after `REPLAY_PLAYER.seekToTick(0)`, capture relied on the first engine camera pass to overwrite the subprocess's live camera. The initial capture could therefore use an uninitialized `(0,0,0)` camera even when the replay scene frame already contained a valid camera.
- New behavior: immediately after `seekToTick(0)`, guarded code copies the first `ReplaySceneFrame.camera` position, pitch, yaw, and FOV into `gpCamera`, calls `updateVectors()`, and emits a centralized Replay warning containing the source tick and camera values. Missing scene-frame and null-camera cases emit explicit errors.
- Reason: make the first export frame independent of startup ordering while preserving recorded-camera authority.

## Documentation and evidence

- Read `docs/ROUTER.md`, `docs/specs/replays/replay-editor-and-export-v2.md`, `docs/specs/effects/effects.md`, `docs/regressions/regressions-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, and `docs/operations/task-completion/task-completion.md`.
- Appended `2026-09-07T17:43:06Z` to `docs/regressions/regressions-v1.md`.
- Existing evidence remains: the later replay JSON has valid moving camera frames and eight projectile-spawn events, while export logs showed one historical projectile event being processed repeatedly. Replay code still reconstructs projectile/effects through replay-only branches, which disagrees with the shared live-effect requirement in the effects specification.

## Validation

- `git diff --check`: passed.
- `python build_agent.py`: passed; `Status: SUCCESS`; one changed source compiled and `mimita.exe` was linked.
- No new automated replay-effect tests were added, per the requested scope.
- Human live export remains required to verify the first-export camera symptom and inspect the new `[EXPORT-SUBPROCESS] camera seeded` log.

## Remaining review

- Implement and live-test shared replay projectile simulation/collision and event identity delivery.
- Route replay muzzle flash, tracer, damage number, dynamic light, hit sphere, and hit burst through the same lifetime/decay owners as live gameplay.
- Compare right- and left-leg recorded local transforms with the model/rest-pose basis before changing rotation math.
- Do not mark the effect or left-leg regressions resolved based on this build alone.
