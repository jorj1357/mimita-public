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

## Scheduled independent review

### 2026-11-24T01:46:08.527Z — resource-profiler

A bounded independent review was queued as wake wake-af5cc0494785 after 491 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed)

### 2026-11-24T02:17:18.853Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper, wake-af5cc0494785). No source edits, no file claims, no build, no close.

Scope and state
- Issue readiness ready=true (score 8/8, missing=[]) at 2026-11-24T01:47Z; state in_review; owner empty; occurrences []; one prior review (id 21, build-wait-reviewer, 2026-11-20T20:14:32Z).
- Exact wait unchanged: exe-build:33:2026-11-20T19:50:40.561Z, stage setup, reason "preparing build", elapsed 60.253s vs 60.0s, generation 33, source_generation 34, build_id 2026-11-20T19:45:42.207284Z.

Observed facts (re-verified this sweep)
- Issue record exists: coord.issue.opened seq 2209 at 2026-11-20T19:51:40.840Z.
- Exactly one reviewer wake exists for this wait: wake-b63e0968d7b7 wake.queued 2026-11-20T19:51:42.178Z; codex claimed/launched 19:51:43.108/19:51:43.289Z; wake.retry to opencode 19:56:26.529Z; opencode claimed/launched 19:56:30.182/19:56:30.221Z; wake.failed "adapter process exited" 20:19:29.195Z (.dev/agent-wake/events.jsonl lines 320,324,325,331,335,336,351). That is one wake request with a fallback attempt, not duplicate wakes.
- Proof expectation part 1 (dev-loop wait event) is still unmet for THIS occurrence: .dev/state.json's old run 20261120_170259 points at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but Test-Path is false and the whole logs/2026-11-20 directory is absent. The expected dev-loop.wait.over_budget row for exe-build:33 is unreadable from any surviving journal. Available proof: this issue record, the wake journal above, and the occurrence appended to issue-00b133ae7e13 (coord seq 2253, 2026-11-20T20:10:36.899Z).

New evidence since the prior review
- The same wait class recurred on the live loop as a DIFFERENT wait_id and correctly got its own issue, not a duplicate: exe-build:49:2026-11-24T01:41:57.628Z, stage "compiling", elapsed 61.661s, generation/source_generation 49, build_id 2026-11-23T19:22:15.804284Z -> issue-c4b82c2ed632 (coord.issue.opened seq 12936, 01:43:00.896Z). Both dev-loop.wait.reviewer-wake and dev-loop.wait.over_budget were written to a readable journal (logs/2026-11-24/20261124_005533/dev-loop/events.jsonl lines 52-53), so the recording path works whenever the run journal exists.
- Reviewer-wake identity was CORRECT for that new wait: wake-70987c3bb01d queued 01:43:04.135Z, codex claimed/launched 01:43:10.131/01:43:10.487Z, wake.retry to opencode 01:46:08.868Z (.dev/agent-wake/events.jsonl lines 2451-2453,2457), and the reporter event embedded wake-70987c3bb01d. This is a negative control: prior "stale wake embedded" cases happen when build_friction.wake_reviewer's `agent-wake.py run --once` drains an older pending wake (build_friction.py:242-252) and reports that drain's result; with no older pending wake, identity matches.

Inference and uncertainty
- Acceptance for the 2026-11-20 occurrence is met: wait_id, stage, reason, elapsed, build/run IDs, generation, source_generation, and exactly one reviewer wake are recorded. This review adds no code-fix claim.
- The prior root finding (two 12-job builds contending for 12 logical CPUs plus source_changed_during_build=true) was not re-measured here because the 2026-11-20 build journals are gone; treat it as prior-review inference, not a fresh measurement.
- Whether the 60s WAIT_ISSUE_SECONDS (dev-loop.py:114-115) is below achievable incremental EXE latency on this host under load is still supported by contemporaneous over-budget waits (exe-build gen 49 at 61.661s today; 182/333 game-dll builds >60s per issue-84c584957e65), but the gen-49 build's own elapsed time was not captured in this sweep.

Contrary evidence
- The issue's stated risk ("a noisy wait policy could create duplicate friction issues") did not materialize for a distinct wait_id: the gen-49 wait produced one new issue (issue-c4b82c2ed632) with no occurrence appended here and no duplicate for the same wait_id.
- The missing journal half is a retention defect, not proof that the wait detector failed: the detector demonstrably runs and journals over-budget waits on the current loop.

Next discriminating check
- Before any fix or close: run one controlled over-budget wait on the current loop with a readable run journal and confirm, from that same journal, that (a) exactly one dev-loop.wait.over_budget row exists for the wait_id, (b) exactly one wake.queued exists whose issue references that wait_id, and (c) the dev-loop.wait.reviewer-wake event embeds that same wake id (catches the misattribution). Cross-link the result to issue-84c584957e65 (late dispatch/misattribution) and issue-79e68f63fb01 (run-journal retention). Keep this issue in_review until a journal-backed occurrence exists or a reasoned proof exemption is recorded. Cold-handoff: no owner, next step is the controlled-journal check, unknowns are the gen-33 build timing and whether the 60s budget is intentionally set below observed latency.
