// 2026-11-20T06:49:00Z
/* purpose
* record agent work-time and issue-context tracking
* record the local AI visibility/target-selection source change
* separate tooling and compile evidence from runtime and human proof
* this file DOES NOT claim screenshot, collision, or 240 FPS completion
*/

# Work-time, issue readiness, and AI target visibility

## Task

- Task ID: `session-20261120-work-metrics`
- Issue: `issue-89c11130189a` (work-time), `issue-3100aa7b5807`
  (issue context), `issue-f8873319f12b` (local AI controller)
- Owner: `work-metrics`
- Status: tooling implemented and tested; AI source compiled; runtime proof
  pending.

## What changed

### Agent work-time

- Added `work_sessions` and `work_phases` tables.
- Added `agent-coord work start|phase|wait|resume|end|status|list|summary`.
- `begin` now opens one active work session and `end` closes it, so tracking is
  agent-owned and model-independent.
- `dev-loop.py --request build|game-dll --wait-for-build` automatically records
  `build_wait` with the request ID and resumes the previous phase on success,
  failure, or timeout. This is the default path for every agent.
- A session records the received message, task, issue, start/end UTC, total
  elapsed milliseconds, and each phase's elapsed milliseconds, wait reason,
  build ID, and run ID.
- `board`, `status`, `coordination_snapshot`, and the dashboard coordination
  panel now expose active work sessions and elapsed time.
- Active sessions and active phases report live elapsed milliseconds; summary
  aggregates completed and active phase time.

### Issue context and readiness

- Added durable issue fields for reproduction, expected behavior, acceptance,
  file paths, related build/run, and evidence.
- Added `issue context`, `issue readiness`, and `issue needs-info`.
- `issue show` and `observe` now include readiness and the exact missing fields.
- Backfilled context and acceptance for the current screenshot, 240 FPS, NPC
  audit, AI controller, culling, awareness, collision, work-time, and issue
  context issues.
- `issue needs-info` currently reports 33 older issues with missing context.
  They were not filled with invented reproduction or acceptance text.

### Local AI controller

- `src/actor/player-ai.cpp` now rejects the local actor when selecting a target.
- Target candidates are ranked by the existing distance/facing score, then only
  the best eight are checked for line of sight.
- A latched melee target is cleared when it becomes unseen.
- Unseen actors are no longer aimed at or fired at through walls.
- `docs/specs/dev-loop/agent-coordination.md`,
  `docs/specs/dev-loop/agent-self-improvement.md`, and
  `docs/specs/dev-loop/dashboard.md` document work-time and context readiness.

## Verification

- `python -m unittest devscripts.quality.test_agent_coord -q` passed: 37 tests,
  including distinct `build_wait`/automatic resume and automatic begin/end
  work sessions.
- `python -m py_compile devscripts/agent_coord.py devscripts/dev-loop.py
  devscripts/dev_loop/gui.py` passed.
- `src/actor/player-ai.cpp` compiled in game-DLL generation `397`
  (`game-dll-20261120T063933641878Z-848734`); build status is `published`.
- Build note `3da33855b15a` is attached to the build record.
- No runtime AI, screenshot, collision, or 240 FPS claim is made.

## Measured work-session state

The completed session reported:

```text
total    2236437 ms
received  160851 ms
planning 1020224 ms
editing   758590 ms
review    296772 ms
```

Planning was the largest phase. This measurement predates automatic build-wait
tracking, so its build wait remains inside editing. New sessions split that
wait automatically through the dev-loop path.

## Remaining

- `screenshot_at_tick` is still specified but not implemented.
- Arcade collision work remains owned by `arcade-collision-default`.
- Visible 240 FPS remains unproven; the current display is 60 Hz and the
  observed client still has large spikes.
- Older issues with missing context remain visible in `issue needs-info` for
  their owners or the user to clarify.
