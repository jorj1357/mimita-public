# issue-723326f11472: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (reviewed)

### 2026-11-20T20:14:32.631Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed the exact wait. No source edits and no file claims.

Wait identity
- wait_id exe-build:33:2026-11-20T19:50:40.561Z, kind exe-build, stage setup, reason "preparing build", elapsed 60.253s vs 60.0s budget, generation 33, source_generation 34.
- Issue auto-created 2026-11-20T19:51:40.840Z (coord.issue.opened seq 2209). Exactly one issue exists for this wait_id and no occurrence has been appended, so dedupe by wait_id worked for this occurrence.
- Reviewer wake wake-b63e0968d7b7 was queued once at 19:51:42.178Z for this issue. One codex attempt launched 19:51:43 then exited; the same wake retried on opencode at 19:56:30. That is one wake + fallback, not duplicate wakes.

What the wait was: a successful build under CPU contention, not a hang
- Generation 33 started 19:50:40.561Z and published artifact 2026-11-20T19-55-20.256758Z (build_id ...20be3bab..., source_generation 33). It succeeded ~4.4 min after the 60s check.
- build/build-timing.json and build/build-metrics.jsonl 2026-11-20T19:55:14Z: result success, mode release, jobs 12, compiled 1, skipped 86, elapsed_s 266.061, phase_seconds compile_s 171.983, link_s 6.063, staging_s 0.029.
- Same shape just before: 2026-11-20T19:45:36Z, elapsed_s 184.849, compile_s 130.017, link_s 5.111, success.
- Idle same-shape baseline: 2026-11-20T07:47:06Z debug, compiled 1, elapsed_s 46.59, compile_s 19.328.
=> One translation unit took 130-172s while another build ran, versus ~19s idle. The 60s budget is below achievable incremental EXE latency on this host under load. This is real build friction/latency, not a stuck build.

Likely cause (same chain as issue-00b133ae7e13)
- This EXE build ran with jobs=12 while the dev-loop game-DLL build was also running with 12 jobs (state game_build). build.py defaults BUILD_JOBS to os.cpu_count() (12), so the two builds can oversubscribe the 12 logical CPUs ~2x before agents/tests.
- Source churn: artifact 2026-11-20T19-55-20.256758Z has source_changed_during_build=true (worktree hash b15bd6... at start -> 9fd9ed... at publish), so it published stale and queued yet another build.
- The generation-33 wait is already recorded as an occurrence on issue-00b133ae7e13 (20:10:36Z). Recurrences should stay referenced there instead of a duplicate analysis.

Evidence gap (proof expectation only partly met)
- .dev/state.json points run 20261120_170259 at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but that file and its directory do not exist (Test-Path False). The expected dev-loop.wait.over_budget event therefore cannot be read; the dev-loop-journal portion of the proof expectation is unmet.
- Available proof: coord.issue.opened seq 2209, this issue record, and the single wake-b63e0968d7b7 request/queued/claimed/launched events.
- The missing run-level journal is the same class as issue-79e68f63fb01 (failure journal masks first cause); it should be fixed under that owner, not this wait issue.

Acceptance for this occurrence: met (wait_id, stage, reason, elapsed, generation, build_id, and the single reviewer wake are recorded). No code fix claimed. Fix candidate remains cross-build compile-worker arbitration (cap EXE + game-DLL + module workers at logical CPUs minus headroom, or serialize EXE compile against game-DLL compile) plus source-stability debounce before publish.
