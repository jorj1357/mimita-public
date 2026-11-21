# issue-44d82d8626ad: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-21T22:24:00.016Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: confirmed — the wait is a real, expected-duration build, not a stalled or wedged build.

Reproduction confirmed at the pinned occurrence:
- wait_id game-dll-build:2026-11-21T21:56:52.660Z fired at elapsed 60.294s while attempt game-dll-20261121T215653845698Z-0e52c1 was still in codegen (reason: codegen:packet_registry_codegen.py started, elapsed_ms 10617.5).
- That attempt: started 21:56:53.845Z, completed 22:03:11.439Z, total_seconds=377.503, success=true, no_op=true.
- Timeline .dev/build-timings/game-dll-20261121T215653845698Z-0e52c1.jsonl: resident_state_codegen 8.389s, packet_registry_codegen 102.887s, manifest_snapshot 153.203s, manifest_validation 1.937s, source_snapshot 68.079s, then worker no_op phase 5.826ms. The 60s budget expires inside packet_registry_codegen, long before any build can finish.

Normal latency, not anomaly:
- Recent game-DLL attempts all exceed 60s: d501fb worker_total_seconds=359.611 (compile 76.94s/12 files, link 61.718s), 0e52c1 total_seconds=377.503, 729a42 build.failed at 321.428s.

Root causes:
1. Single fixed budget. WAIT_ISSUE_SECONDS is 60s (devscripts/dev-loop.py:108-109) and is applied to the game-dll-build wait (devscripts/dev-loop.py:3391-3407). Every normal build is over budget. This is the issue's stated risk "a noisy wait policy could create duplicate friction issues": there are currently 45 open build-friction issues titled "Wait over budget: game-dll-build at game_build" (28 in_review, 17 new).
2. No-op short-circuit runs after the pre-worker pipeline. devscripts/dev_loop/game_build.py:617-692 runs both codegen scripts, manifest_snapshot, and source_snapshot before the worker; no_op is only derived at lines 779/809. 0e52c1 (request auto-1) was a duplicate after d501fb published generation 433 at 21:56:51Z, yet it spent 377s redoing snapshots to conclude no_op.
3. Coarse stage label. stage is hardcoded "game_build" (devscripts/dev-loop.py:3398); the real stage only appears in reason, so all 45 issues share one title. state.json also shows request churn: auto-1..auto-17 with most "superseded", auto-2 failed, source_generation 17.

Acceptance check: wait_id, stage, reason, elapsed, generation, request_ids, budget_seconds, and build_attempt_id (in reason) are present, and exactly one reviewer wake was queued for this wait (this session). Recording acceptance is met; the policy is firing on expected latency.

Recommended fix (not applied; review-only, claim separately):
1. Use a per-kind/stage budget derived from measured baseline (game-dll-build p95 is ~370-400s) or a progress-based rule that flags only when no new stage event arrives for N seconds.
2. Detect an already-published/no-op revision before codegen/manifest/source snapshot so a duplicate request returns in milliseconds.
3. Set stage from the active GAME_STAGE (e.g. codegen:packet_registry_codegen.py) so occurrences are distinguishable.
4. Deduplicate queued auto-N requests for the same revision to cut superseded/no-op churn.

Confidence: high. Evidence is the pinned attempt summary + timeline and .dev/state.json request log.
