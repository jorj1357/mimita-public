# issue-76f19e7598c4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-20T21:29:07.443Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer activation for wait game-dll-build:2026-11-20T20:47:34.055Z.

Issue completeness: verified. The record carries wait_id, kind, stage, reason, elapsed_seconds=60.702, started_utc, source_generation=40, generation=413, build_id=2026-11-20T20:20:51.788061Z, request_ids=[auto-40], and a reviewer wake.

Was it a stall? No. .dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json shows the same build started 2026-11-20T20:47:34.114Z and completed successfully at 20:56:59.129Z: total_seconds=565.001, generation=414, source_generation=40, request auto-40, success=true. Dominant phase object_scan_seconds=318.344 (scan_parse_ms=307016.39), then compile_seconds=114.931, link_seconds=48.722, publish_seconds=28.107, jobs=12, source_count=642. The 60.702s wait therefore fired inside a normal full game-DLL build, not a hang.

Noise finding: the 60s budget is far below the normal build time on this tree, so each cold-ish build creates a same-titled issue. Four near-identical issues exist today: issue-76f19e7598c4, issue-0322fa45b0e8, issue-41a872c08642, issue-f66e6e82656b. build_friction.py keys uniqueness on wait_id, so per-occurrence issues are intentional and the requirement is met; the cost is repeated identical titles. Suggested follow-up (no code changed here): make the wait budget stage/build-aware or group repeated over-budget waits by stage so readers can distinguish a real stall from the expected cost of a 565s cold build.

Wake acceptance: the wake was queued exactly once (wake-f54a24f3d0fa, queued 20:48:36). The codex adapter was claimed first and exited, then agent-wake retried opencode at 20:59:54 and launched at 21:18:39, so delivery took about 30 minutes. The delivered opener was only "You were woken by MiMITA agent-wake. This is a bounded test activation." while the request/prompt files contained this review task; that truncation is tracked separately as issue-62ab3daa615d.

No source edits and no file leases were taken. Reproduction, expected behavior, acceptance, paths, and proof expectation were already present.

## Agent suggestion

### 2026-11-20T21:32:02.131Z — build-wait-reviewer

Structured attribution for this wait points at the previously published artifact, not the in-flight attempt: issue evidence build_id=2026-11-20T20:20:51.788061Z and generation=413, while .dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json shows the waited-on attempt is generation=414 (source_generation=40, request auto-40, started 20:47:34.114Z, completed 20:56:59.129Z). The correct attempt identity appears only in the free-text reason. Also, .dev/state.json points the dev-loop journal at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but that file and directory are absent (run dir contains only run.json, server-01, client-01), so the wait.over_budget event could not be read. Suggested owner fix: have the wait-issue writer read active_build_* from .dev/state.json (or pass the active attempt id) so build_id/generation identify the in-flight build.

## Scheduled independent review

### 2026-11-24T05:25:08.611Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-24b7f7b2ca83 after 516 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-confirmed-live-with-open-defects)

### 2026-11-24T05:40:13.678Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (read-only, 2026-11-24). No source edit, build, run, close, fix, or proof claim.

READINESS / PROOF STATUS
- issue readiness: ready=true, score 8/8, missing=[], state=in_review, tasks=0. No task_proofs row exists for this issue, so its acceptance is not proven and there is no exemption.
- Second review: reviews[0] is build-wait-reviewer 2026-11-20T21:29:07Z (verdict reviewed).

OBSERVED FACTS (this occurrence, wait game-dll-build:2026-11-20T20:47:34.055Z)
1. Issue evidence carries wait_id, kind=game-dll-build, stage=game_build, elapsed_seconds=60.702, budget_seconds=60, started_utc=2026-11-20T20:47:34.055Z, source_generation=40, generation=413, build_id=2026-11-20T20:20:51.788061Z, request_ids=[auto-40]. It carries no run_id.
2. The in-flight attempt was game-dll-20261120T204734114577Z-82e79e (source_generation 40, request auto-40, started 20:47:34.114Z); it succeeded at 20:56:59.129Z as generation 414, total_seconds=565.001 (.dev/build-timings/game-dll-20261120T204734114577Z-82e79e.json). The issue generation (413) and build_id (20:20:51, a previous EXE build) do not identify the build that was waiting (414 / 82e79e). The acceptance clause carries build/run IDs is therefore only partly met: stale build_id and absent run_id.
3. The wait was intrinsic build cost, not a stall: object_scan_seconds=318.344 (scan_parse_ms=307016.39), changed_source_preflight=60.257, compile=114.931 (12 compiled / 630 skipped), link=48.722, publish=28.107, jobs=12, source_count=642. The flat 60s budget crosses during a normal full rebuild.
4. Wake was queued exactly once: wake-f54a24f3d0fa queued 2026-11-20T20:48:36.474Z (.dev/agent-wake/events.jsonl:361). Codex claimed/launched 20:48:37.737/20:48:37.803Z (366-367); retry to opencode 20:59:54.564Z (375); opencode claimed/launched 21:18:39.682/21:18:39.750Z (380-381); wake.failed adapter process exited 2026-11-20T23:44:05.340Z (407). Request status=failed, attempts=2, yet it produced the review at 21:29:07Z, so the failed terminal status is recorded about 2h14m after a successful review.
5. Current source still has both defects: devscripts/dev-loop.py:3896 sets build_id=self.latest_build (last published) and :3897 generation=self.game_build_generation, which is assigned only on completion at :3259; the wait dict has no run_id or attempt id. devscripts/build_friction.py:143-200 keys uniqueness on wait_id and documents that a new wait gets another issue even when its stage and reason are identical, so the risk named in fields.risk has materialized: 148 open same-title issues (133 in_review, 14 new, 1 assigned) and 173 open Wait over budget* issues out of 336 total issues in the coordinator store.

MISSING EVIDENCE
- proof_expectation requires a dev-loop wait event. The 2026-11-20 dev-loop run journal is not retained (logs/2026-11-20 absent; logs/11-20-2026 contains 0 files), so dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake for this wait_id cannot be re-verified from artifacts. The live path is demonstrated for later occurrences (e.g. the 2026-11-22T22:28 review on issue-9a4503f6ed68) but not provable for this one.

INFERENCE / UNCERTAINTY
- Identity is taken from last-completed values, not the in-flight attempt, consistent with earlier reviews of later occurrences. I did not reproduce a fresh wait in this read-only sweep, so I rely on current source plus this occurrence pair.
- Whether acceptance intends build_id to mean the waiting build or the last published build is ambiguous; if the former, this occurrence fails it. A future fix should define that explicitly.

CONTRARY EVIDENCE
- The automation half works: exactly one issue, exactly one wake request, and a real reviewer review were produced; the 565s build was healthy (success=true).
- No second issue exists for this exact wait_id, so wake queued exactly once for this wait occurrence holds in the queue sense.

NEXT DISCRIMINATING CHECK (smallest)
- On the next game-dll-build wait, compare the new issue evidence generation/build_id and a new run_id field against the in-flight dev-loop.game-build.started build_attempt_id/generation in the run dev-loop/events.jsonl. Expected: evidence identifies the in-flight attempt (not the last published), carries run_id, and its journal/build-timing paths resolve on disk. Also confirm the reviewer wake request ends failed only with a reasoned failure, not after a review already exists.
