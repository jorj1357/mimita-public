# Replay Camera Export Investigation — 2026-09-07 12:46 EST

## Branch and commit

- Branch: `8292026stash`
- HEAD at investigation: `bc3987a`
- Timestamp: `2026-09-07T16:46:52Z`

## Scope

Investigation only, requested because pressing `P` after gameplay or a server
kill reportedly exports an MP4 whose camera is stuck near `(0,0,0)` instead of
following the exporting player's camera. No C++ source, configuration, build
output, or runtime executable was changed.

## Pre-existing changes

The worktree already contained unrelated edits before this investigation:
`config/analytics.json` modified, `docs/regressions/regressions-v1.md`
modified, and untracked VIP/documentation files under the 2026-09-07
changelog/gold paths. Those changes were preserved. The regression file was
already modified by prior work; this session appended one replay entry only.

## Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/replays/replay-editor-and-export-v2.md`
- `docs/regressions/regressions-v1.md`
- 2026-09-06 and 2026-09-07 changelog files, including the existing 9/6 replay
  camera regression and its claimed 9/6 confirmation
- `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW
- `docs/skills/logging-checker-v1.md` — PASS_WITH_HUMAN_REVIEW
- `docs/operations/task-completion/task-completion.md`

## Finding

- Severity: high
- Type: spec-code disagreement at runtime; source path currently contains the
  prior fix, so the exact recurrence cause remains unresolved
- Specification: the replay specification requires quick export to recreate
  the local player's experience, including local camera, local camera
  transform, and camera mode; the hard correctness test requires correct local
  camera and camera mode.
- Actual behavior: user reports a completed export with a static camera near
  the world origin.
- Expected behavior: the exported camera must use the exporting player's
  recorded camera transform and move as that camera moved during the captured
  ticks.

## Exact implementation evidence

- `src/engine/engine-tick-replay.cpp:303-304` currently permits recording during
  export with `gReplayRecorder.isRecording() && (!replayPlaybackActive ||
  isReplayExportActive())`.
- `src/engine/engine-tick-replay.cpp:352-356` currently copies live
  `camera.pos`, rotation, and FOV into every scene frame.
- `src/replay/replay-export-subprocess.cpp:337-345` currently avoids
  `beginPlayback()` and starts export playback with `seekToTick(0)`.
- `src/engine/engine-tick-camera.cpp:628-636` currently passes the current
  scene frame to the replay camera controller.
- `src/replay/replay-player.cpp:276-286` currently assigns the recorded scene
  frame camera position to `camera.pos` in recorded mode.

The data flow is therefore correct only when the captured clip actually
contains valid, non-default camera fields. If the clip has default camera
fields, the exporter faithfully renders the origin camera and cannot recover
the live camera after the snapshot.

## Regression record

Appended, without rewriting earlier entries:
`docs/regressions/regressions-v1.md` entry
`2026-09-07T17:45:00Z — Replay quick-export camera stuck at world origin
(UNRESOLVED recurrence)`.

The entry links the current specification, exact source path, prior 9/6 root
cause, current source evidence, required diagnostic correlation, and the
remaining uncertainty between stale executable, invalid newly captured clip,
and another runtime state transition.

## Validation

- Read-only repository search and source inspection completed.
- `mimita.exe` exists and was last successfully built according to
  `build/changelog.txt` at 2026-09-07 12:31:59 local time; no rebuild was run
  because no code changed.
- `git diff --check` reports one pre-existing trailing-whitespace warning on
  the informal login note at `docs/regressions/regressions-v1.md:387`; the new
  replay entry introduced no whitespace warning.
- No fresh replay clip JSON, export log, or user reproduction session was
  available, so runtime camera movement is not proven in this investigation.

## Human review still required

Reproduce once with the current `mimita.exe`, then inspect the fresh clip's
first and last scene-frame camera positions and the export diagnostics for
subprocess pre-loop camera and camera-controller output. Only after that
comparison can the recurrence be assigned to recording, executable freshness,
or export playback.

