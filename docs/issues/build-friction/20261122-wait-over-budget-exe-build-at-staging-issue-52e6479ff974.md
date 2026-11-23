# issue-52e6479ff974: Wait over budget: exe-build at staging

## Human report (verbatim, immutable)

> exe-build wait at staging exceeded the wait budget


## Context

### 2026-11-22T19:31:39.069Z — build-wait-reviewer

related_build: 2026-11-22T17:39:49.763492Z
related_run: 20261122_144609_01

## Review (confirmed-known-root-cause)

### 2026-11-22T19:34:16.958Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait exe-build:5:2026-11-22T17:37:18.164Z (EXE generation 5, stage staging, 60.98s) against run 20261122_144609_01.

Findings:
- Wait is real and expected: generation-5 EXE build total 155.35s. "staging" is a normal post-link phase in build.py (line 1400/1421-1424 stage_runtime_dlls) that can exceed the generic 60s budget. Journal logs/2026-11-22/20261122_144609_01/dev-loop/events.jsonl:1601 (AI launch accepted 17:37:11 waiting on source build) and :1606 (build completed 17:40:02, artifact 2026-11-22T17:39:49.763492Z).
- Issue creation succeeded: issue-52e6479ff974 created 17:38:19.705Z with wait_id/stage/reason/elapsed/build_id/generation/source_generation.
- Reviewer wake was queued exactly once: .dev/agent-wake/events.jsonl wake.queued 17:38:25.768Z (wake-f0479e4f7c10).
- First divergence: dev-loop logged dev-loop.wait.issue-failed instead of dev-loop.wait.over_budget at events.jsonl:1602 (17:38:55.981Z) because build_friction.wake_reviewer (devscripts/build_friction.py:237-246) blocks on `agent-wake.py run --once` with timeout=30. An interactive review session outlives 30s, so subprocess.TimeoutExpired is caught at devscripts/dev-loop.py:3573 and reported as failure even though the issue and wake both succeeded. Same defect already recorded in issue-9aeec1fbae9d; recommended fix there is to queue/add the wake and return, log over_budget/reviewer-wake before any adapter launch, and report adapter launch separately.
- Secondary: wait.reason captured the stale compile-log line "[SKIP] src\host\host-persistence-fallback.cpp" instead of the staging phase detail, and wait.build_id is the last successful artifact (2026-11-22T08:00:31.376619Z), not the in-progress generation-5 attempt; run_id is absent from evidence.
- Delivery latency: wake.claimed by codex at 18:02:51, retried opencode 18:06:37, launched 18:25:25. The codex model_not_found and undrained fallback are already tracked by issue-84c584957e65; not a duplicate of this wait.

Verdict: not a duplicate wait occurrence. Acceptance is functionally met (one issue, one queued wake) but the dev-loop journal falsely reports failure and omits over_budget. No source edit made; the code fix belongs to issue-9aeec1fbae9d, separate from this occurrence.
