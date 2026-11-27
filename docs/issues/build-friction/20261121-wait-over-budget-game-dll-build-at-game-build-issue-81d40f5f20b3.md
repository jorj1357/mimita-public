# issue-81d40f5f20b3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-21T20:46:33.280Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence against the pinned build attempt and the wait policy. Conclusion: this is not a hung build; it is normal game-DLL build duration exceeding a flat 60s wait budget. No source edit is needed for this occurrence.

WAIT OCCURRENCE (verified)
- wait_id: game-dll-build:2026-11-21T20:05:41.774Z
- started_utc: 2026-11-21T20:05:41.774Z; elapsed_seconds: 60.527; budget_seconds: 60.0; stage: game_build
- Captured reason is a normal stage-start line, not a stall: codegen:packet_registry_codegen.py state=started at 20:05:43.800Z, elapsed_ms=1913.782 (build only 1.9s in).
- Issue evidence generation:430 is the last-published generation when the wait was reported, not the attempt target. The attempt builds generation 431.
- Reviewer wake was queued exactly once: wake-28e8124a18a3 (.dev/agent-wake/requests/wake-28e8124a18a3.json), status launched. Acceptance clause satisfied.

BUILD ATTEMPT (pinned)
- attempt: game-dll-20261121T200541886892Z-5b40e3
- started 20:05:41.887Z, completed 20:11:22.692Z, success=true, total_seconds=340.795, worker_total_seconds=209.94 (.dev/build-timings/game-dll-20261121T200541886892Z-5b40e3.json).
- Stage split: changed_source_preflight 56.478s; object_scan 89.771s; compile 76.745s for 12 units; link 41.866s; publish 0.915s.
- The codegen stage active when the wait fired took 77.749s total (20:05:43.800Z to 20:07:01.550Z); one mandatory stage alone exceeds the 60s budget.
- Generation 431 compiled and published; it was later superseded by generation 432 (.dev/build-timings/game-dll-20261121T201417234689Z-2d9df4.json, ready.json).

FIRST DIVERGENCE (owner and line)
- devscripts/dev-loop.py:108-109 sets one global WAIT_ISSUE_SECONDS=60 for every wait kind.
- devscripts/dev-loop.py:3391-3407 fires the game-dll-build issue from wall-clock age of game_build_started_utc alone, using game_build_detail as the reason; it does not compare elapsed time to an expected stage duration and does not require stage progress to have stopped.
- devscripts/build_friction.py:143-202 creates a fresh issue per wait_id, so every attempt longer than 60s yields a new near-duplicate issue. That is why the board holds many identical wait-budget records for this stage.
- packet_registry_codegen.py runs on every attempt and its recent durations vary widely (3.48s at 19:53:15Z, 3.64s at 20:14:21Z, 77.75s here, 83.18s at 02:30:26Z, 224.53s at 03:50:46Z), so a fixed 60s threshold cannot distinguish slow-but-healthy from stuck.

ASSESSMENT / NEXT
- This occurrence is build-friction noise, not a hang. The underlying measurement/latency work is already owned by issue-136d4553ff92 (owner: build-coordinator).
- Proposed fix for that owner or a separately claimed task (needs human approval): make the wait stage-aware and progress-aware - derive the budget from the current stage's recent duration, or fire only after no stage-progress event for N seconds, and coalesce per build attempt. Do not implement here.
- No files edited, no build or test run requested. Evidence confidence: high (pinned attempt summary plus stage timeline plus wake request).

## Context

### 2026-11-21T20:46:41.053Z — build-wait-reviewer

related_build: game-dll-20261121T200541886892Z-5b40e3

## Scheduled independent review

### 2026-11-27T18:47:41.152Z — build-wait-reviewer

A bounded independent review was queued as wake wake-083e14d90c6f after 856 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-recurring-wait-policy-noise-with-real-latency-deficit)

### 2026-11-27T19:15:20.387Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-083e14d90c6f), read-only, 2026-11-27T19:0x-19:20Z. No source edit, no build/run request, no close, no fix claim.

OBSERVED FACTS (verified)
1. Fresh occurrence, same signature. issue-003b412c0836 was created 2026-11-27T18:42:03.907Z for wait_id game-dll-build:2026-11-27T18:41:02.539Z, elapsed_seconds=60.845, budget_seconds=60.0, stage=game_build. The dev-loop runtime journal logs/2026-11-27/20261127_174537/dev-loop/events.jsonl contains both dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake for that wait_id (issue=issue-003b412c0836, created=true, wake queued=true, adapter=codex). The captured reason is a normal build-start line ("[GAME BUILD] revision=fc7d555... sources=646 jobs=10 priority=high ..."), not a stall.
2. The pinned attempt is healthy but slow. game-dll-20261127T184102743581Z-55292b published generation 805, success=true, total_seconds=427.963, worker_total_seconds=412.554 (.dev/build-timings/game-dll-20261127T184102743581Z-55292b.json; build/game/current.json). Stage split: changed_source_preflight 286.041s; compile 290.658s for 2 compiled / 645 skipped; link 119.636s; publish 0.544s. At the 60.8s alarm the build was inside normal preflight.
3. Policy is still one flat budget. devscripts/dev-loop.py:114-115 WAIT_ISSUE_SECONDS = max(10, env MIMITA_WAIT_ISSUE_SECONDS or 60). The game-dll alarm at dev-loop.py:3885-3901 still fires on wall-clock age of game_build_started_utc alone; it does not compare elapsed to an expected/recent stage duration and does not require stage progress to have stopped.
4. Coalescing is per-exact-wait only. devscripts/build_friction.py:131-140 and 164-169 dedupe on evidence.wait_id; dev-loop.py:3890 derives wait_id from the per-attempt started_utc. One attempt therefore still mints one issue. The brain currently holds roughly 90 open issues titled "Wait over budget: game-dll-build at game_build" created 2026-11-24..27 (e.g. 18:42, 18:33, 18:22, 18:17, 17:57, 17:47 on 2026-11-27). That is the same aggregate the 2026-11-22 review of issue-9a4503f6ed68 recorded (127 then), so this is a persisting, not new, condition.
5. Not "free noise": the same gen-805 build changed only src/npc/recast-navigation.cpp (1 direct changed source, 645 skipped) yet took 427.963s. The new docs/specs/dev-loop/build-performance-budget.md (2026-11-27, currently untracked) sets "One changed leaf .cpp in one hot module" at p50 10s / p90 20s / hard ceiling 30s, so this attempt is ~14x its hard ceiling. The 2026-11-21 pinned attempt was 340.795s; today's is 427.963s. The duration grew; it did not recover.
6. Acceptance clause met for this occurrence. Exactly one issue and one reviewer wake were produced for wait_id 18:41:02.539Z. However the reviewer-wake event's captured launch_result text names issue-796a7430913c / wait game-dll-build:2026-11-27T18:21:05.061Z while its issue field is issue-003b412c0836. That is consistent with the synchronous agent-wake.py "run --once" (build_friction.py:244-246) draining an older pending request; I did not confirm whether the current prompt was also dispatched.

INFERENCE / UNCERTAINTY
- The alerting hook is not broken: detect -> one issue -> one wake -> durable journal works for one wait_id. The remaining defects are (a) a flat, unmeasured budget and (b) per-attempt issue/wake minting. Both were already named in the 2026-11-22 review of issue-9a4503f6ed68 and are unchanged in behavior.
- I sampled 2026-11-21 (original incident), 2026-11-22 (issue-9a4503f6ed68 review), and today's gen 805; all three were healthy builds. I did not measure the stall/noise ratio across all ~90 open issues, so I cannot say what fraction are genuine stalls.
- The wait evidence still records generation=804 while the in-flight attempt targets generation 805; build_id is a previous EXE build. Recorded generation is the last published generation, not the attempt identity. This is the identity defect already noted in issue-9a4503f6ed68 and is not re-proven here beyond this fresh pair.

CONTRARY EVIDENCE
- The occurrence is a genuine >60s wait and automation handled it end to end, so the feature is not inert.
- "Wait policy noise" alone understates the finding: the build duration itself is far over the new performance budget, so this occurrence is both a noisy alarm and a real latency signal. Do not close this issue as a no-op while the duration deficit is unaddressed.
- Exactly one wake was queued for this wait_id, so the "exactly once" wording holds; the mismatch in item 6 is about which request the immediate launch drained, not about duplicate creation for one wait_id.

NEXT DISCRIMINATING CHECK
1. On the next game-DLL attempt, compare the new wait issue's generation/build_id with that attempt's dev-loop.game-build.started build_attempt_id and terminal result generation. A mismatch re-confirms the identity defect with a fresh pair.
2. Read .dev/agent-wake/requests for the wake tied to issue-003b412c0836 and confirm whether it dispatched issue-003b412c0836 or replayed issue-796a7430913c.
3. After a stage-aware/progress-aware wait change lands, a healthy multi-minute build like gen 805 (or its successor) must yield 0 new wait issues, while a synthetic no-stage-progress stall must still yield exactly 1. Record both before/after counts and the pinned journals.
