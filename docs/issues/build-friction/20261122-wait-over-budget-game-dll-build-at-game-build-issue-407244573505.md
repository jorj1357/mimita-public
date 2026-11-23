# issue-407244573505: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-no-fix)

### 2026-11-22T21:32:58.408Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence of the recurring game-dll-build-at-game_build wait class. Prior substantive analysis: issue-a236165fa6fc review by build-wait-reviewer (2026-11-22T14:44:49Z); this review adds the pinned evidence for this occurrence only.

Occurrence evidence (direct from .dev/build-timings/game-dll-20261122T205638368599Z-230ae3.json and .jsonl):
- wait_id game-dll-build:2026-11-22T20:56:38.347Z; elapsed 60.488s vs flat budget 60.0s (+0.488s); generation 523; source_generation 15; request auto-15.
- Attempt started 20:56:38.368Z, flagged ~20:57:38.8Z, then completed 20:57:43.858Z success=true, total 65.477s; generation-523 DLL published.
- At the flag the build was mid-link, not stalled: link.lld_linker started 20:57:24.123Z and completed 20:57:42.855Z (18.732s). Main phases: changed_source_preflight 38.136s (recast-navigation.cpp 38.06s), compile 1.494s, link 18.829s.
- Verdict: not_a_stall; healthy slow incremental build. No infrastructure fault and no missing reproduction/acceptance work.

Root cause (same as a236165fa6fc, high confidence): devscripts/dev-loop.py:3486-3547 flags game-dll-build on total elapsed since worker start with a flat WAIT_ISSUE_SECONDS default of 60. That is below normal incremental game-DLL latency here (this build 65.5s; the next observed build generation 525 took 76.2s), so healthy builds are flagged.

Secondary defect: wait.reason is a truncated JSON tail (e.g. "5649.368,\"build_attempt_id\":...") instead of a readable stage/path; already tracked in issue-6e08d89b55e3.

Noise/flood: build_friction.record_wait_incident keys uniqueness on wait_id, so each fresh over-60s build opens another identical-titled issue. At review time the board held 122 open issues titled "Wait over budget: game-dll-build at game_build" out of 157 build-friction issues. This occurrence is a duplicate of the known condition; no new investigation or code change was made in this review.

Recommended (not claimed here; design owner issue-9a4503f6ed68, wait-fix tracking issue-e044a931c54b, reason fix issue-6e08d89b55e3):
1. Make the game-dll-build check progress-aware (flag only when no build progress event for N seconds) or raise the game-dll-build budget above normal latency, keeping 60s for queue waits.
2. Emit a clean human-readable reason from the last structured stage/file event.
3. Aggregate recurring identical game-dll-build-at-game_build waits into one rolling issue with occurrences instead of one issue per wait_id.

Confidence: high; all timings read directly from the pinned attempt timeline.
