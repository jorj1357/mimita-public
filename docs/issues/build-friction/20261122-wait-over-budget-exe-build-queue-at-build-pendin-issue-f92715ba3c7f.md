# issue-f92715ba3c7f: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (reviewed-expected-queue-contention)

### 2026-11-22T04:12:50.843Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reproduction verified against the exact pinned journal
logs/2026-11-22/20261122_011031/dev-loop/events.jsonl (run 20261122_011031).

Causal chain (journal line numbers):
- L520-L525: game-DLL build generation 449 starts (dev-loop.game-build.started 03:52:21.727Z, after manifest/source snapshot 03:51:57-03:52:16).
- L529: dev-loop.control 03:52:53.167Z accepts build request 2026-11-22T03:52:50.598Z-b2290797 from codex-ingame-gui-reconnect, reason - build request queued through dev-loop.
- The single EXE-builder lane is busy with the game-DLL build, so the EXE request sits in build_pending.
- L530-L531: dev-loop.wait.over_budget 03:53:57.774Z records this issue for wait_id exe-build-queue:50736.849 (elapsed 60.649s, budget 60.0s) and dev-loop.wait.reviewer-wake queues exactly one reviewer wake (wake-8ce6b21fbc58).
- L546/L549: gen449 result ok 03:57:18.764Z, generation ready 03:57:46.107Z.
- L561: gen450 dev-loop.game-build.started 03:58:11.654Z (src/npc/npc.cpp changed again).
- L577-L578: gen450 result ok and generation ready 04:03:02.
- L580-L581: EXE build begins compiling 04:03:03.139Z (wait_id exe-build:10:2026-11-22T04:03:03.139Z); that separate compile phase also crosses 60s and files issue-4d9eb4b1a8ad.
- L584: dev-loop.build.completed 04:05:15.090Z publishes the requested EXE build (source_generation 10, duration 66.81s, error empty).

Assessment and first divergence:
- Issue fields are complete and correct: reproduction, expected_behavior, acceptance, paths, proof_expectation all present, readiness.missing = []. Reviewer wake recorded exactly once. No missing field work needed.
- The real first divergence is in wait observability, not the build pipeline. change-to-evidence-latency.md:122-127 states an over-budget wait appends later observations, but dev-loop.py:3537-3545 adds wait_id to reported_wait_ids after the first crossing and never re-records it. Thus this issue elapsed_seconds (60.649s) is only the first over-budget sample; the true exe-build-queue/build_pending duration was ~610s (request accepted 03:52:53 -> EXE compile start 04:03:03).
- Root cause of the wait: expected serialization, by design. The single EXE-builder lane (change-to-evidence-latency.md:137, dev-loop.md one EXE build at a time) was committed to game-DLL builds 449 and 450 when the EXE request arrived. The flat WAIT_ISSUE_SECONDS=60 (dev-loop.py:109, used by check_wait_budget dev-loop.py:3475-3536) treats queue contention the same as a stalled build, so a queued EXE request behind a multi-minute game-DLL build will always file a build-friction issue. This build itself did not fail; it published at 04:05:15.
- wait_id-keyed dedupe is working: one issue and one wake for this occurrence. No duplicate issue created by this review.

Confidence: high for the causal chain and successful publish (direct line-referenced journal events); medium as to whether the 60s queued-EXE budget is intended policy or an over-broad threshold.

Code fix: none applied (human approval required). Suggested separate follow-up for the dev-loop wait-policy owner (related issues e044a931c54b / 136d4553ff92): (a) append continued over-budget observations for the same wait_id instead of only the first crossing, so elapsed reflects the full wait; (b) use a stage/class-aware or queue-aware budget for exe-build-queue while a game-DLL build holds the single builder, to avoid low-actionability issues for bounded serialization. Recorded here as a suggestion, not claimed in this review.

## Agent suggestion

### 2026-11-22T04:13:07.396Z — build-wait-reviewer

Follow-up (not claimed here, human approval required). Spec change-to-evidence-latency.md:122-127 requires an over-budget wait to append later observations, but dev-loop.py:3537-3545 suppresses re-recording once wait_id is in reported_wait_ids, so this issue keeps elapsed_seconds=60.649s while the actual exe-build-queue/build_pending wait ran ~610s (03:52:53 request -> 04:03:03 compile start) behind game-DLL builds 449/450. Two bounded fixes for the wait-policy owner (issue-e044a931c54b / issue-136d4553ff92): (1) re-record/appends for a still-waiting wait_id so elapsed reflects full duration; (2) a queue-aware or class-specific budget for exe-build-queue while a game-DLL build holds the single EXE builder, so by-design serialization does not file low-actionability issues. Evidence: logs/2026-11-22/20261122_011031/dev-loop/events.jsonl L529-L531, L546-L549, L561, L577-L578, L580-L581, L584.
