# issue-0322fa45b0e8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-21T00:06:02.028Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence game-dll-build:2026-11-20T20:58:50.904Z (issue evidence complete: elapsed=60.257s, budget=60s, source_generation 41, request auto-41). No source edit.

Exact attempt evidence (.dev/build-timings/game-dll-20261120T205850979297Z-7cdcb2.json):
- success=true, generation=415, source_generation=41, started_utc=20:58:50.979Z, completed_utc=21:08:14.521Z, total_seconds=563.53
- worker_metrics: object_scan_seconds=340.257 (dominant), compile_seconds=96.924, link_seconds=37.094, publish_seconds=27.554
- request auto-41 state is "success" in .dev/state.json
The over-budget check fired at 60.257s while the healthy build was inside its normal 340s object-scan phase. Nothing was stalled; this is a false-positive friction alarm, not a build hang.

Wake timing evidence: request wake-15cdbf8cf0e0 was created 2026-11-20T20:59:52Z and launched 2026-11-20T23:44:04Z (MIMITA_WAKE_ID=wake-15cdbf8cf0e0). The reviewed build had already succeeded at 21:08:14Z, so the reviewer activation arrived ~2h44m after the build finished and ~36m after it started; it could not influence or observe the wait. Wake prompt delivery also matched issue-62ab3daa615d (only the first line reached the session), so the target issue had to be resolved from MIMITA_WAKE_ID and the .dev request file.

Review result: issue acceptance is met for this occurrence (wait_id, stage, reason, elapsed, generation, request id, evidence, one reviewer wake). Systemic recommendation (stall-based budget, signature dedup, and stale-wake expiry) is filed on umbrella issue-9a4503f6ed68; no duplicate issue created, no source fix claimed.

## Scheduled independent review

### 2026-11-24T12:43:53.814Z — resource-profiler

A bounded independent review was queued as wake wake-d4324fdcd3cf after 555 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-open)

### 2026-11-24T12:56:25.760Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-d4324fdcd3cf), read-only. No source edit, build, close, or fix claimed.

OBSERVED FACTS
1. Readiness/state: issue-0322fa45b0e8 is ready (score 8/8, no missing fields), state=in_review, tasks=0, reviewers=[build-wait-reviewer]; prior review id 29 (verdict pass) is intact.
2. Original occurrence artifacts: .dev/build-timings/game-dll-20261120T205850979297Z-7cdcb2.json records success=true, generation=415, source_generation=41, started 2026-11-20T20:58:50.979Z, completed 21:08:14.521Z, total_seconds=563.53 (object_scan 340.257, compile 96.924, link 37.094, publish 27.554). The sole reviewer wake for this wait, wake-15cdbf8cf0e0, was created 20:59:52.893Z, claimed/launched 2026-11-20T23:44:04.061Z/109Z, attempts=2, status=failed ('adapter process exited'), updated 2026-11-21T00:13:29.066Z. Only that request file in .dev/agent-wake/requests references this issue. So one wake was queued for this wait_id; the review landed ~2h44m after the wait started and after the build succeeded.
3. Identity defect reproduced on this occurrence: the wait evidence carries build_id=2026-11-20T20:20:51.788061Z and generation=414, but the in-flight attempt was game-dll-20261120T205850979297Z-7cdcb2 / generation 415 (source_generation 41). The 'build_id' field therefore names the previous published build, not the waited-on attempt.
4. Recurrence persists (new since the 2026-11-21 review): the current build-friction listing returns 100 issues (90 in_review, 8 new, 2 assigned), of which 91 are titled 'Wait over budget: game-dll-build at game_build'. Same-day examples: issue-8576955c4fb5 (61.311s), issue-c7d9edb5e68b (60.464s), issue-9a301a157e7c (61.713s), issue-3472692a57c2 (60.733s). The budget is still the flat 60s (dev-loop.py:113-115); dedupe is per wait_id only (build_friction.py:131-169), so each attempt mints another issue and wake.
5. Fresh pair for the same defects: wait game-dll-build:2026-11-24T12:22:08.237Z (issue-3472692a57c2) also carries generation=593 / build_id=2026-11-24T07:48:35.684015Z while its attempt game-dll-20261124T122208532874Z-63061a completed as generation 594. That build was a real success at total_seconds=97.566 (compile 61.449; server-projectiles.cpp 60.521; link 22.383). Its resource capture reports mean_host_cpu_percent=99.853, peak 100.0, memory_used_percent=94, minimum_available_memory=312,864,768 B, with matched_prebuild_resource_baseline unavailable. The wake wake-e471a391868c was created 12:23:09.565Z; codex claimed/launched 12:23:21.774/844Z; scheduler retry 12:38:27.642Z; opencode claimed/launched 12:43:54.446/499Z; attempts=2, status=launched. That is two wake.launched events for one wait_id and ~1244.9s (~20m45s) queue-to-final-launch.
6. Control: this sweep's own wake (wake-d4324fdcd3cf) was queued 12:43:53.783Z and launched 12:43:54.418Z, so the happy path remains sub-second when opencode is the adapter.

INFERENCE / UNCERTAINTY
- The original occurrence satisfies the per-wait record contract (one issue, wait_id/stage/reason/elapsed, one queued wake) only weakly: the required build/run identifier is present as a field but points at the wrong build. This review therefore does not treat 'build/run IDs' as fully satisfied.
- The recurrence is policy + identity + per-attempt dedupe, not an inert hook. The hook fired and produced issues/wakes; the defects are the flat budget, stale identity, and per-attempt dedupe.
- The 12:22 alarm is a genuine >60s successful build, not a hang. Whether 97.566s was intrinsic or contention-inflated is UNKNOWN: the host was saturated (CPU ~100%, memory 94%) but the matched pre-build baseline is unavailable, so I do not claim host pressure caused the duration.
- I did not verify whether any same-wait_id duplicate issue exists beyond the wake duplicate; the observed duplicate is two launches of one wake, not two issues.

CONTRARY EVIDENCE
- Exactly one reviewer wake request exists for the original wait_id and exactly one review (id 29) was appended for it, so the per-occurrence wake-queue contract held for this occurrence.
- The latest waited-on build succeeded normally; the over-budget check is not describing a stall.
- The wake path is fast when it reaches opencode directly (this sweep: ~0.6s), so the 20m45s delay is adapter-recovery specific, not a generally slow queue.

NEXT DISCRIMINATING CHECK
- On the next over-budget game-dll wait, join the wait issue's evidence build_id/generation to that run's dev-loop.game-build.started build_attempt_id and result generation; a continued one-generation mismatch confirms the identity defect until the umbrella fix lands.
- After the adapter-preference/work-landed fix, measure queue-to-final-launch from .dev/agent-wake/requests/<wake>.json (created_utc -> launched_utc, attempts) on the next 5 over-budget waits; target <=300s and attempts=1, with 0 duplicate wake.launched per wait_id.
- To separate signal from noise, time one matched idle-host incremental build of the same one-TU change and compare against the 97.566s / 100%-CPU observation; if the idle build is also >60s, the 60s budget is mis-calibrated for incremental builds rather than merely contended.

PROCESS REVIEW
- No net-new process proposal is warranted from this sweep. The repeated late/duplicate dispatch is already owned by issue-84c584957e65 with a measurable acceptance (<=5 min, exactly one launch) and proposed adapter-preference/work-landed fixes; the recurring-budget/dedupe policy is already owned by umbrella issue-9a4503f6ed68. This review adds the 2026-11-24T12:22 occurrence to issue-84c584957e65 instead of duplicating a proposal.
