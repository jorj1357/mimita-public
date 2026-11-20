# Build-wait wake review and wake-delivery evidence

## Date

2026-11-20

## Agent

`build-wait-reviewer`

## Summary

Bounded agent-wake activation. Reviewed two automatically created
build-friction wait issues and replied to two coordination messages. No source
edits and no builds started.

- `issue-e9f3b2727a9a` (`exe-build-queue` at `build_pending`, 61.113s) is an
  idle/launch-pending false positive. The wait record has `request_ids: []`,
  `started_utc: ""`, and reason `idle; waiting for a source change or explicit
  build/launch request`. Code path: `dev-loop.py:2612` `_queue_build_if_needed`
  arms `build_pending` for the stale launch/start/restart paths
  (`dev-loop.py:2839`, `3017`, `3170`) without a request id or non-idle detail,
  then `check_wait_budget` (`dev-loop.py:3286`) reports an `exe-build-queue`
  wait. Recommended owner fix recorded; not applied.
- `issue-dff7e8c0f758` (`exe-build` at `setup`, 60.833s) overlaps the
  `game-dll-20261120T160245254559Z-669fad` attempt, which failed 0.431s earlier
  with `WinError 5` renaming `build/game/progress.json.tmp` to
  `build/game/progress.json`. `.dev/current/mimita.exe` remains the
  07:47:09Z artifact, so generation 19 EXE never published.
- Added an independent confirmation sample to `issue-62ab3daa615d`: the
  delivered user text was only the prompt's first line while the request file
  held the full assigned-issue prompt.

## Evidence

- `.dev/agent-wake/requests/wake-667af71a82dc.json` (assigned wake)
- `.dev/build-timings/game-dll-20261120T160245254559Z-669fad.jsonl`
- `build/game/progress.json`, `.dev/current/mimita.exe`
- `agent-coord issue review issue-e9f3b2727a9a`
- `agent-coord issue review issue-dff7e8c0f758`
- `agent-coord issue suggest issue-62ab3daa615d`

## Not proven

Build success is not behavior proof. These are review and evidence records;
the suggested wait-policy fixes still need owner implementation and a live
pinned proof.
