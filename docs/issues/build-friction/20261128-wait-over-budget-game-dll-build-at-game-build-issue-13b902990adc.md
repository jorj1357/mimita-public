# issue-13b902990adc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-genuine-build; evidence-partial-missing-wait-journal-events)

### 2026-11-28T18:36:49.445Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-1dbab16fcd3b). Read-only: no source edit, no compiler/build/worker/dev-loop/test/runtime started by this agent, no fix or closure claimed. This is an observability-friction record, not a gameplay/behavior claim.

## Acceptance / readiness
- Issue readiness ready=true, missing=[], state=new before this review. wait_id, stage, reason, elapsed, build/run IDs, paths, and proof expectation are present.
- wait_id game-dll-build:2026-11-28T18:00:34.216Z; stage game_build; elapsed 63.662s vs budget 60.0s; source_generation 81; request_ids [build-request-12597f8bba32].
- Exactly one issue was created (issue-13b902990adc 18:01:38.716Z) and one reviewer wake was queued (wake-1dbab16fcd3b 18:01:42.600Z, +8.4s after the 60s trigger). The exactly-once queue held: one wake.queued for this wake id.

## What the wait actually was (genuine work, not a stall)
Pinned attempt game-dll-20261128T180034349425Z-3007f5 (.dev/build-timings/game-dll-20261128T180034349425Z-3007f5.json):
- success=true, generation 825, source_generation 81, revision c5e48a0..., total_seconds 134.187, completed 18:02:48.537Z. Changed path src/npc/team-brain.cpp (header src/npc/team-brain.h fanout 28).
- phase_durations_ms: changed_source_preflight 10797, compile 96234, link 4219, object_cache_scan 7031, publish 625.
- At the 60s trigger (~18:01:34) the build was in the compile phase (compile window ~18:00:59 -> 18:02:36); compile dominates 96.2s of 134.2s. Host was near-saturated (resource.jsonl host cpu 100% from 18:01:05 on; free memory fell to ~0.5-2.4 GB). The 63.7s wait is full-DLL compile work, not a hang.

## FIRST DIVERGENCE (proof expectation met 2/3)
Proof expectation is "Dev-loop wait event plus the issue record and reviewer wake record." The issue record and the queued wake exist, but BOTH required dev-loop journal events are missing for this wait:
- logs/2026-11-28/20261128_171225/dev-loop/events.jsonl line 822: dev-loop.wait.issue-failed wall 18:02:12.828Z, error "Command '[... agent-wake.py run --once]' timed out after 30 seconds", reason "could not record over-budget wait".
- No dev-loop.wait.over_budget and no dev-loop.wait.reviewer-wake row carries this wait_id (the reviewer-wake/over_budget at lines 879/880 belong to the NEXT wait game-dll-build:2026-11-28T18:11:01.936Z / issue-0e64599cd133).
- Cause is a confirmed live recurrence of the wake_reviewer inline-timeout defect tracked on issue-9a4503f6ed68 (occurrence COORD_20769): build_friction.wake_reviewer (build_friction.py:244-246) runs agent-wake.py run --once synchronously with timeout=30 and no try/except; dev-loop.check_wait_budget (dev-loop.py:3925-3950) wraps record_wait_incident + wake_reviewer in a single try and already added wait_id to reported_wait_ids (dev-loop.py:3929) before wake_reviewer. TimeoutExpired jumps to the except, so over_budget and reviewer-wake are skipped and never retried. The issue and queue survive, but the journal half of the proof for this occurrence is permanently lost.
- First missing/incorrect step: event emission is coupled to inline adapter-launch success instead of the queue/record result.

## Late reviewer dispatch (issue-84c584957e65)
From .dev/agent-wake/events.jsonl: wake.queued 18:01:42.600Z; codex claimed/launched 18:08:28.049/18:08:28.131Z (pid 30516, +405.5s after queue); wake.retry "previous adapter process exited; trying fallback" 18:12:23.833Z; opencode claimed/launched 18:31:34.693/18:31:34.718Z (pid 26348). Queue-to-final-launch 1792.1s (~29m52s), two wake.launched for one wait_id. The build completed at 18:02:48.5Z, so the reviewer launched ~1726s (28m46s) after the build it was reviewing had already finished. Same late-dispatch/duplicate-launch pattern already tracked (occurrence appended there).

## Evidence identity gaps (cross-linked, no duplicate issues)
- build_id 2026-11-27T17:45:19.700118Z and generation 820 are the prior latest_build, not the in-flight attempt game-dll-20261128T180034349425Z-3007f5 (real generation 825). The in-flight identity appears only inside the truncated reason. Tracked on issue-7d3dcfd7433b.
- reason is a mid-JSON tail "...51594.0,\"build_attempt_id\":...,\"progress_write_ms\":...,\"record_overhead_ms\":..." from dev-loop.py:3130/3160 detail=line.rstrip()[-240:], not a readable stage summary. Tracked on issue-6e08d89b55e3.
- No run_id in the wait record. NOTE: this run's events_path is valid (run 20261128_171225), so the journal-half loss here is the inline-timeout path, NOT the stale-path/ENOENT loss tracked on issue-6e2d7b91e7d9.

## Suggested smallest fix (report-only; unowned; belongs to issue-9a4503f6ed68 owner)
Make wake_reviewer's inline run --once best-effort (catch subprocess.TimeoutExpired and return {queued:true, launch_error:...}), and/or emit dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake from the record/queue result BEFORE any adapter launch attempt, recording launch status as a separate field. This is a dev-loop.py/build_friction.py change under shared build authority; not claimed by this reviewer.

## Confidence
High that the wait was genuine compile work and that the two journal events are missing (direct lines quoted). High on the dispatch timings (wake journal). Confirmed: one issue + one wake queued exactly once.
