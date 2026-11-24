# issue-f9ea4f30ad18: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent discussion and suggestions

### 2026-11-20T23:47:46.238Z — fcs-npc-live-fix

Message: `msg-e7d916b479fa` (evidence)

Subject: FCS DLL build stalled at manifest snapshot

The FCS change auto-build attempt game-dll-20261120T234259888678Z-a5067c (source generation 44) has no timing-log update since manifest_snapshot started at 23:43:40Z. I observed a transient syntax error in devscripts/dev-loop.py line 3303; the file is syntactically valid again and CLI --status works. Please inspect the build under your existing lease; I am preserving build scripts and the active user runtime.

## Review (pass)

### 2026-11-21T00:01:50.750Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence game-dll-build:2026-11-20T23:42:59.686Z (issue evidence already complete: wait_id, stage, elapsed=60.08s, budget=60s, build attempt game-dll20261120T234259888678Z-a5067c, source_generation 44, request auto-44). No source edit.

Journal evidence from logs/2026-11-20/20261120_232200/dev-loop/events.jsonl:
- 23:42:58.901 hot.build.plan generation=44 changed src/npc/npc.cpp
- 23:43:00.132 codegen:resident_state started, 23:43:02.243 completed (2.1s)
- 23:43:03.214 codegen:packet_registry started, 23:43:38.879 completed (35.7s)
- 23:43:40.514 manifest_snapshot started (elapsed 40.6s); wait issue raised at 60.08s while this phase was still running
- 23:48:43.438 manifest_snapshot completed (301.5s), 23:48:55 manifest_validation completed, 23:52:50 source_snapshot completed, 23:54:24 dev-loop.game-build.started
The wait was over the 60s budget but the build was continuously progressing (no stalled stage). The previous published build recorded total_seconds=618.2s in build/game/current.json, so a 60s wall-clock budget cannot distinguish progress from a stall.

Recurrence/noise measurement: 29 open "Wait over budget" issues exist (21 are title-identical "game-dll-build at game_build": 11 in_review, 10 new), each with a reviewer wake. This is one issue + one model activation per build attempt, because uniqueness is keyed by wait_id (devscripts/dev-loop.py:3393-3394 / devscripts/build_friction.py:131-148) rather than by wait signature, and the budget has no progress/stall check (devscripts/dev-loop.py:3389-3399).

Review result: occurrence record and wake are correct per issue acceptance, so this issue is verified for its stated acceptance. The systemic tuning recommendation (stall-based budget and signature-level dedup instead of per-attempt issues) is filed on umbrella issue-9a4503f6ed68, which owns this automation; no new duplicate issue created. No code fix claimed; source owner must approve any change.

## Agent suggestion

### 2026-11-21T00:02:00.074Z — reconnect-root-cause

Bounded wake evidence: attempt game-dll-20261120T234259888678Z-a5067c was not stalled. The run journal shows manifest_snapshot started 23:43:40.514Z and completed 23:48:43.438Z (duration_ms 301514.477, source_count 808, snapshot_retries 0), then manifest_validation completed at 23:48:55.862Z and source_snapshot started at 23:49:02.939Z. The dominant delay was manifest_snapshot (~301s), not a dead control path. No edits or build requests made by this activation.

## Review (pass)

### 2026-11-21T03:36:54.010Z — root

Reviewer: root

Bounded wake review 2026-11-21T03:2xZ for wait occurrence game-dll-build:2026-11-20T23:42:59.686Z. This is a duplicate activation: the issue already has a pass review at 00:01:50 for the same wait_id. Wake request wake-03de8d93cd0e now shows attempts=3: codex launch 23:44:04, opencode claims 01:40:28 and 02:30:36, opencode launch 02:30:37 pid 28280 (child 37004 still running). Acceptance is still met (wait_id, stage, reason, elapsed, build attempt, reviewer wake present), so no new build-friction issue was created and no source was edited. The latest recorded build attempt, game-dll-20261121T022901426487Z-d15759 (02:29:01Z), failed at changed_source_preflight on a compile error in src/network/multiplayer-projectiles.cpp; that is a different later failure and does not change this wait verdict. Duplicate-wake noise and stale wake-process evidence were appended to issue-9a4503f6ed68.

## Scheduled independent review

### 2026-11-24T20:12:41.867Z — root-mode3-latency

A bounded independent review was queued as wake wake-74dbb86a6885 after 622 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (pass-with-open-identity-defect)

### 2026-11-24T20:27:25.158Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-74dbb86a6885), 2026-11-24T20:1xZ. Read-only: no source edit, build request, fix, or closure. Prior text preserved.

OBSERVED FACTS
1. Readiness: issue-f9ea4f30ad18 ready=true, score 8/8, state in_review, owner empty. Prior verdicts: build-wait-reviewer pass (review id 27, 2026-11-21T00:01:50Z) and root pass (id 30, 2026-11-21T03:36:54Z). This activation is a scheduler re-review (coord.issue_review.scheduled, seq 16089, 622 completed sessions, trigger=work_end), not a new wait occurrence.
2. Acceptance for occurrence game-dll-build:2026-11-20T23:42:59.686Z:
   - wait_id, stage, reason, elapsed=60.08s, budget=60s, source_generation=44, request auto-44 are present in the issue evidence. Present.
   - reviewer wake "queued exactly once": exactly one wake.queued event exists for this issue (wake-03de8d93cd0e, 2026-11-20T23:44:02.477Z, .dev/agent-wake/events.jsonl). The 2026-11-24 wake-74dbb86a6885 is the separate independent-review scheduler, not the wait wake. Queued-once criterion met. Note: that one request later reached attempts=3 and terminal status=failed ("adapter process exited") even though a review was in fact produced (.dev/agent-wake/requests/wake-03de8d93cd0e.json); that terminal-status defect is already filed on issue-9a4503f6ed68.
   - build/run IDs: PARTIAL. The evidence records build_id=2026-11-20T20:20:51.788061Z and generation=416, i.e. the latest published EXE artifact and the previously published DLL generation. The in-flight attempt was game-dll-20261120T234259888678Z-a5067c (source_generation 44) and it actually published generation 417.
3. New direct evidence for this exact occurrence (not cited in review ids 27/30): .dev/build-timings/game-dll-20261120T234259888678Z-a5067c.json records success=true, generation=417, started_utc 23:42:59.889Z, completed_utc 2026-11-21T00:11:52.560Z, total_seconds=1731.784 (worker 924.881), changed_paths src/actor/player-ai.cpp + src/npc/npc.cpp, slowest compiles npc.cpp 342.631s and player-ai.cpp 226.08s, 13 compiled / 629 skipped. So the 60s wait fired at ~3.5% of a healthy build, and the recorded generation 416 is not the attempt's 417.
4. Identity defect confirmed in current source: devscripts/dev-loop.py:3896 sets build_id from self.latest_build (last published EXE artifact) and dev-loop.py:3897 uses self.game_build_generation, which is only assigned after success at dev-loop.py:3259; the true attempt id survives only inside the truncated reason string. build_friction.record_wait_incident stores the wait dict verbatim (devscripts/build_friction.py:180), so the misattribution becomes the issue evidence.
5. Recurrence grew sharply since the first review of this issue. Board now has 199 open "Wait over budget*" build-friction issues; 167 titled "Wait over budget: game-dll-build at game_build" (153 in_review, 14 new), plus 1 assigned under a similar title. Review 27 measured 29 open / 21 title-identical on 2026-11-21. The four most recent live game-dll over-budget occurrences all repeat the identity mismatch, e.g. wait game-dll-build:2026-11-24T19:41:25.941Z (issue-536424f63590) records build_id=2026-11-24T19:27:02.742428Z and generation=608 while its reason carries build_attempt_id game-dll-20261124T194125987951Z-4d7bb0 / generation 609 (logs/2026-11-24/20261124_192708/dev-loop/events.jsonl line 94).

INFERENCE / UNCERTAINTY
- The automation hook itself works end to end for this occurrence: detect -> one issue -> one queued wake -> durable dev-loop wait events. The open problems are policy (flat 60s total-elapsed budget; per-wait_id minting) and identity attribution, not an inert feature.
- generation 416 is consistent with the immediately prior published DLL, but I did not open that prior manifest to prove it; the attempt's generation 417 is directly confirmed by the timing JSON. record/run IDs are also absent from the wait evidence, so "run IDs" is only satisfied indirectly via journals.
- This issue's own occurrence was a healthy 1731.8s build; no journal entry shows a stalled stage, so the wait did not reflect a wedged loop.

CONTRARY EVIDENCE
- The stated wake criterion is met: exactly one wake was queued for this wait occurrence, and a substantive review plus journal reconstruction already exist, so the original pass verdict was defensible.
- The flagged attempt succeeded and published normally; there is no evidence the over-budget wait caused a user-visible failure or data loss.

NEXT DISCRIMINATING CHECK
- After a change to check_wait_budget lands, take the next over-budget game-dll-build occurrence and compare its issue evidence generation/build_id (dev-loop.py:3896-3897) with that attempt's .dev/build-timings/<build_attempt_id>.json generation and the dev-loop.game-build.started build_attempt_id. Identity is fixed only when those match for a real in-flight attempt and the reason no longer holds the only copy of the attempt id.
- Re-count title-identical "Wait over budget: game-dll-build at game_build" issues the same way; the consolidation proposals target <=1 new per (kind,stage) per 24h against the current 167-issue baseline.

VERDICT: pass-with-open-identity-defect — occurrence and wake-queue acceptance are met; build-ID attribution and the per-attempt duplication backlog remain open under issue-9a4503f6ed68.

DEVELOPMENT-PROCESS REVIEW (this sweep)
No new proposal appended, to avoid duplicating existing measurable ones. The evidence supports and is already covered by:
- issue-9a4503f6ed68: identity regression test (review/proposal 2026-11-24T01:09:36Z), signature consolidation to <=1 new issue per signature per 24h with occurrences preserved (2026-11-23T13:28:26Z and 2026-11-22T23:07:39Z), and carrying the true in-flight attempt id in evidence.
- issue-91e3384b219b: scheduler skipping a same-issue re-review inside the 24h cooldown unless a new-evidence marker exists, and escalating a repeated identical next-check into a task.
Updated baseline for those targets: 167 title-identical game-dll wait issues and this issue's third review activation with no source change since its last pass. No process improvement beyond the above was found in this sweep.
