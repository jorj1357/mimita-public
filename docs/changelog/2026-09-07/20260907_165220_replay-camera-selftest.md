# Replay Camera Export Selftest — 2026-09-07 12:52 EST

## Branch and commit

- Branch: `8292026stash`
- HEAD during work: `bc3987a`
- Completion timestamp: `2026-09-07T16:52:20Z`

## Request and scope

The user requested an automated test for replay export that checks where
`sceneFrames` and camera data are recorded, then reports the output. This was
implemented as a narrow extension of the existing `--replay-export-selftest`.
It does not change production replay behavior or the live `P` input path.

## Pre-existing changes

The worktree already contained unrelated edits before this task: modified
`config/analytics.json`, modified `docs/regressions/regressions-v1.md`, and
untracked VIP/documentation/gold files. The prior replay regression entry and
investigation changelog were also pre-existing. Only the test assertions and
camera fixtures described below were changed in this task.

## Exact implementation

File: `src/game/game-cli.cpp`

- In the existing `--replay-export-selftest` synthetic replay fixture, each of
  the 10 `ReplaySceneFrame` values now receives a non-zero moving camera:
  `position = {100 + i*3, 200, 300}`, rotation yaw `90 + i*2`, and FOV `100`.
- Added checks that tick 0 contains the expected recorded camera, tick 5 has a
  camera position greater than the starting position, and tick 9 continues
  moving beyond the saved tick-5 position.
- The tick-5 camera X value is copied before seeking to tick 9 because the
  player returns a pointer to a reusable interpolated frame; retaining that
  pointer across a seek would compare the same mutable object against itself.

## Why this test matters

The test now verifies the replay-data portion of the camera chain:

`ReplaySceneFrame.camera` → JSON save/load → `ReplayPlayer` scene-frame lookup
→ camera reconstruction.

It also retains the existing Media Foundation and FFmpeg output checks. It
does not yet start a live match, move a real player, synthesize a real keyboard
`P` press, or inspect a production quick-export JSON generated from that live
session. Those remain human/runtime acceptance work.

## Validation

- Build command: `python build_agent.py`
- Build result: `Status: SUCCESS`; one source file compiled and `mimita.exe`
  was relinked.
- Test command: `mimita.exe --replay-export-selftest --timeout 60
  --no-coordinator`
- Test result: `26/26 passed, 0 failed`.
- Passed camera checks included:
  - camera at tick 0 matches `(100, 200, 300)`;
  - camera position advances at tick 5;
  - camera continues moving at tick 9.
- Existing export checks also passed: Media Foundation initialization,
  synthetic frames, final MP4, H.264 video, AAC audio, FFmpeg encoding, and
  non-empty MP4 output.
- `git diff --check` reported only the pre-existing trailing-whitespace note
  on the informal login text at `docs/regressions/regressions-v1.md:387`.

## Focused skills

- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The test
  aligns with the replay specification's required local camera reconstruction.
- `docs/skills/logging-checker-v1.md`: PASS_WITH_HUMAN_REVIEW. Existing replay
  diagnostics remain available; this test uses deterministic assertions for
  saved and reconstructed camera state.

## Remaining human review

A live in-game reproduction is still required to prove the complete path:
play/move the camera, press `P`, locate the newly created replay JSON, inspect
its `sceneFrames[].camera.position` values, and compare those values with the
export subprocess camera logs and the resulting MP4.

