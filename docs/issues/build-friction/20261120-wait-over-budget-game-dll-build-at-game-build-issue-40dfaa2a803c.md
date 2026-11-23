# issue-40dfaa2a803c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-active-build)

### 2026-11-20T17:22:49.327Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-4c7fdaef7a55), 2026-11-20.

VERDICT: confirmed over-budget wait; not a detection defect. The wait fired during a genuine, actively compiling game-DLL build, not an idle or wedged loop.

EVIDENCE
- Wait record: wait_id game-dll-build:2026-11-20T16:21:27.899Z, kind game-dll-build, stage game_build, elapsed 62.368s (budget 60s), generation 405, source_generation 20, request auto-20, build_id 2026-11-20T07:47:09.981634Z.
- Build attempt: .dev/build-timings/game-dll-20261120T162131173533Z-83bf96.json started_utc 2026-11-20T16:21:31.173Z, completed_utc 16:38:16.483Z, total_seconds 1005.136, success=false.
- The attempt was busy, not stalled: worker_metrics changed_source_preflight_seconds 429.618, object_scan_seconds 500.072, compile_seconds 1360.694, dependency_check_ms 121770.77, scan_parse_ms 360444.779, link_seconds 136.181, compiled 23 of 642 sources.
- It failed on a real compiler error, not a timeout: src/network/server-gamemode.cpp:3949-3951 'const struct GroundState' has no member named 'groundNormal' (worker_error 'changed-source preflight failed: src/network/server-gamemode.cpp in 312.998s'); slowest compile src/network/server-gamemode.cpp 930.07s.
- Issue fields complete: reproduction, expected behavior, acceptance, paths (devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py), risk, and proof expectation all present; readiness ready=true, missing=[].
- Reviewer wake cadence: exactly one wake queued for this wait, wake-4c7fdaef7a55 at 2026-11-20T16:22:32.947Z; codex adapter launched 16:22:34 and retried as opencode 16:39:30 / 17:02:23. One wake record with adapter fallbacks, no duplicate issue/wake.

DUPLICATE CHECK
- No other issue carries this wait_id; occurrences list is empty. Other game-dll-build/game_build friction issues (b0ba8698acd7, 9a9807fa0128, bb273ac1386c, e84cfa28c7f7, 25b14f436a3e, etc.) use different wait_ids, so this is not a duplicate and needs no occurrence append.

ACTION
- Informational for the wait-issue mechanism: behavior matches acceptance (issue + exactly one reviewer wake). The real cost is build latency, owned by issue-e044a931c54b (avoidable waits) and issue-7d3dcfd7433b (per-stage wait measurement). Recommend the latency owner use the worker_metrics above (object_scan/dependency_check/preflight dominate) as the next reduction target rather than treating this wait as a dev-loop bug.
- No source edits, no file leases, no build requested by this review.

## Scheduled independent review

### 2026-11-23T13:20:04.290Z — mode3-latency-goal

A bounded independent review was queued as wake wake-3093c566f04f after 405 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (occurrence-confirmed; mechanism-flood-and-wait-evidence-identity-defects-open)

### 2026-11-23T13:27:37.322Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-3093c566f04f), read-only, 2026-11-23T13:20-13:35Z. No source edited, no build requested, no run started, no fix claimed, no issue closed.

OBSERVED FACTS
1. Readiness is complete: `issue readiness issue-40dfaa2a803c` -> ready=true, missing=[], score 8/8, state in_review. The record carries reproduction, expected behavior, acceptance, paths (devscripts/dev-loop.py, devscripts/build_friction.py, devscripts/agent-wake.py), risk, and proof expectation. No behavior task/proof row is attached; this is an informational build-friction occurrence, and the prior verdict records it as a genuine active build (not a detector bug).
2. The occurrence is genuine and correctly attributed at occurrence level: .dev/build-timings/game-dll-20261120T162131173533Z-83bf96.json has generation=405, started_utc 2026-11-20T16:21:31.173Z (3.27s after the wait clock 16:21:27.899Z), completed_utc 16:38:16.483Z, total_seconds=1005.136. The wait fired while a build was actively compiling, not while idle or wedged.
3. Per-occurrence wake cadence is exactly one. .dev/agent-wake/events.jsonl holds one pre-existing wake.queued for this issue (wake-4c7fdaef7a55, agent build-wait-reviewer, 2026-11-20T16:22:32.947Z) plus this sweep's scheduled wake (wake-3093c566f04f, 2026-11-23T13:20:04.256Z). There is no duplicate wake for this wait occurrence.
4. WAIT-EVIDENCE IDENTITY DEFECT is present in this record: evidence.build_id=2026-11-20T07:47:09.981634Z is the last EXE build, not the in-flight game-DLL attempt id game-dll-20261120T162131173533Z-83bf96. evidence.generation=405 is correct for this occurrence. Acceptance requires build/run IDs, so the build ID field names the wrong entity even though generation matches.
5. The same identity defect is live on the newest same-family occurrence: issue-cd97259fdc20 (created 2026-11-22T23:53:23.260Z) records generation=545 and build_id=2026-11-22T21:32:48.195707Z, while the in-flight attempt was generation 546, build_attempt_id game-dll-20261122T235222465818Z-9bfbf7, started 2026-11-22T23:52:22.465Z, total_seconds=267.197. So later occurrences carry both a stale generation and a stale build_id.
6. DUPLICATE-FLOOD RISK HAS MATERIALIZED. The coordinator now holds 130 open issues titled "Wait over budget: game-dll-build at game_build" (115 in_review, 14 new, 1 assigned); 152 open "Wait over budget:*" issues; 165 build-friction issues total. Creation is per-wait_id by design: devscripts/build_friction.py:143-148 states "a new wait gets another issue even when its stage and reason are identical", and record_wait_incident mints a new issue when `_wait_issue` finds no exact wait_id (lines 164-180). The prior independent review of issue-9a4503f6ed68 measured 127 of these at 2026-11-22T22:28Z; the count is now 130.
7. Recent build latency improved: the 16 newest game-DLL attempts (generations 547-559) completed in 13.9-58.6s, all under the 60s wait budget, and the newest over-budget game_build wait issue is 2026-11-22T23:53 (gen 546, 267.2s). As of 2026-11-23T13:20Z this exact wait signature has not re-fired for ~13.5h.

INFERENCE / UNCERTAINTY
- This occurrence-level issue is informational and correctly filed; the specific wait is not duplicated. The live defects are mechanism-level: (a) per-wait_id issue minting that has produced a 130-issue flood, and (b) wait-evidence identity (stale generation and/or EXE build_id instead of the in-flight game-DLL attempt). Both are already owned elsewhere: issue-9a4503f6ed68 (owner wake-proof-wait) for the wait hook/grouping and issue-7d3dcfd7433b for wait measurement; I am not claiming them as new work.
- I did not re-derive the 1005.136s failure cause; the earlier groundNormal compile error is consistent with the recorded preflight/compile metrics but I did not re-open the compile log.
- The recent all-under-budget builds are not evidence that the detector is fixed: a cold or header-fanout build can exceed 60s again, and generations 549 (58.6s) and 550 (52.3s) sit close to the budget.

CONTRARY EVIDENCE
- Against the issue's literal acceptance ("a reviewer wake is queued exactly once for this wait occurrence"), this occurrence passes: one issue, one reviewer wake, durable journal events. The duplication is aggregate across occurrences, not a duplicate for this wait_id.
- The identity defect did not affect generation for this particular occurrence (405 matched); it is demonstrated on later occurrences, so it is a current mechanism defect rather than a defect in this specific record.
- The wait was a healthy active build that later failed on a real compiler error, so waiting here was not avoidable dev-loop overhead in the sense of an idle loop.

NEXT DISCRIMINATING CHECK
1. Identity: for the next over-budget game_build wait, compare its issue evidence generation/build_id to the matching .dev/build-timings attempt generation/build_attempt_id (as done above for issue-cd97259fdc20 vs generation 546). A mismatch on a fresh pair confirms the identity defect.
2. De-noising: after a signature-grouping change, count net-new "Wait over budget: game-dll-build at game_build" issues across the next 5 over-budget game_build occurrences. Target: 0 net-new duplicate-title issues while each occurrence still appends exactly one occurrence row and queues exactly one reviewer wake, and each carries its own wait_id and attempt evidence.

VERDICT: occurrence-confirmed; mechanism-level flood and wait-evidence identity defects remain open under issue-9a4503f6ed68 / issue-7d3dcfd7433b.
