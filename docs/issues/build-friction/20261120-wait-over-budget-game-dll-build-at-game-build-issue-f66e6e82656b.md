# issue-f66e6e82656b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-20T20:51:35.001Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence in a bounded test activation. No source edits, no file claims, no fix claimed.

Wait identity
- wait_id game-dll-build:2026-11-20T20:09:36.981Z; kind game-dll-build; stage game_build; elapsed 60.125s vs 60.0s budget; request auto-35; source_generation 38.
- Exactly one issue exists for this wait_id (brain.db issues query), so wait_id dedupe held. Issue opened 2026-11-20T20:10:37.206Z (coord seq 2254).
- Exactly one reviewer wake wake-76b6596d1467 was queued 2026-11-20T20:10:39.012Z for this issue and launched 20:33:56Z (attempt 2, child_pid 23228). That matches the issue acceptance.

What the wait was
- The in-flight attempt is game-dll-20261120T200937027322Z-8daf4c (generation 412, source_generation 38), started 20:09:37.027Z and completed successfully 20:18:25.865Z after total_seconds 528.823. It was not a hang.
- Timeline from .dev/build-timings/game-dll-20261120T200937027322Z-8daf4c.json/.jsonl: packet_registry codegen 20:09:38-20:10:09 (31s), manifest_snapshot 19s, source_snapshot 20:10:28-20:10:43 (14s), physics TU compile 64.3s, object/dependency scan 259.9s (scan_parse 253.4s), remaining compiles to 20:16:56, link 61.2s, publish 25.5s.
- The 60s check fired during the fixed prelude (changed_source_preflight 64.5s plus object scan 259.9s), before and independent of the arcade physics edit. The 60s game_build budget is below this host's measured non-trivial game-DLL baseline, so a similar issue is expected on most game-DLL builds.

Attribution defect (evidence-backed)
- The issue's structured build_id=2026-11-20T19:55:20.256758Z and generation=411 are the previously published artifact (attempt e1b74d), not the in-flight attempt. The correct identity (attempt 8daf4c, generation 412, stage source_snapshot) appears only inside the free-text reason.
- Consequence: build_id and generation on the issue point at the wrong build and cannot be used for correlation; only wait_id is reliable.
- Fix candidate (not claimed): while game_build is running, populate structured build_attempt_id/generation/stage from .dev/state.json active_build_* instead of the latest published artifact.

Evidence gap
- .dev/state.json points the active dev-loop journal at logs/2026-11-20/20261120_170259/dev-loop/events.jsonl, but neither that file nor its directory exists (Test-Path False). The dev-loop.wait.over_budget event cannot be read, so the dev-loop-journal portion of the proof expectation is unmet. Same class as issue-79e68f63fb01 / issue-4e3ef676ba20; do not duplicate that fix here.

Recommendation
- Calibrate per-stage game_build budgets against measured data (preflight plus object scan alone exceed 60s) or measure compile/link only; emit sub-stage wait attribution for codegen/manifest/source_snapshot/scan/compile/link/publish. Cross-reference wait-policy owner issue-e044a931c54b.
- The many open records titled "Wait over budget: game-dll-build at game_build" carry distinct wait_ids, so they are policy-conformant, but the volume confirms the budget is miscalibrated rather than each build being anomalous.

Acceptance for this occurrence: met except the dev-loop journal evidence, which is unavailable for reasons owned elsewhere. Final policy decision remains with a human reviewer.

## Scheduled independent review

### 2026-11-24T03:31:36.604Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-c00e8acaad4e after 500 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed)

### 2026-11-24T03:45:19.523Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review #2 - issue-review-sweeper, 2026-11-24. No source edits, no build, no file claims, no fix or close claimed. Prior build-wait-reviewer text above is preserved unchanged.

OBSERVED FACTS (this occurrence, as filed)
- wait_id game-dll-build:2026-11-20T20:09:36.981Z; elapsed 60.125s vs 60.0s budget; request auto-35; source_generation 38; stage game_build.
- Dedupe held: read-only query of .dev/agent-coord/brain.db found exactly one issue row whose evidence contains this wait_id (issue-f66e6e82656b). No duplicate row for this occurrence.
- Per-attempt artifact still exists: .dev/build-timings/game-dll-20261120T200937027322Z-8daf4c.json -> generation 412, success=true, total_seconds 528.823, changed_source_preflight_seconds 64.529, object_scan_seconds 259.898 (scan_parse_ms 253396), link_seconds 61.193, publish_seconds 25.456. Confirms the 60s check fired during the fixed prelude, independent of the gameplay edit.
- Attribution defect is still visible in the stored record: evidence.build_id=2026-11-20T19:55:20.256758Z and generation=411 (the previously published artifact), while the in-flight attempt was generation 412 (8daf4c). Only wait_id is reliable.
- Original journal remains unrecoverable: logs/2026-11-20/20261120_170259/dev-loop/events.jsonl does not exist (Test-Path False; logs/2026-11-20 has no run directories). The original dev-loop.wait.over_budget event cannot be read, so the journal portion of the proof expectation is unmet for THIS occurrence and cannot be recreated.

NEW EVIDENCE (contemporaneous, 2026-11-24)
- The over-budget event/wake path works now: logs/2026-11-24/20261124_022440_01/dev-loop/events.jsonl contains dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake with matching wait_id and issue id.
- Two new same-title issues were created during this review:
  - issue-456061c38a8c, wait_id game-dll-build:2026-11-24T03:30:57.098Z, elapsed 60.796s, state new, one wake wake-3e8165012607.
  - issue-bf610d228066, wait_id game-dll-build:2026-11-24T03:34:39.674Z, elapsed 60.185s, state new, one wake wake-1ad2bc01995e.
  So one-issue plus one-wake per occurrence still holds today.
- Attribution defect persists and is now worse:
  - Both new issues store generation=null and build_id=2026-11-24T03:30:52.372066Z, which equals .dev/state.json latest_successful_build_id (a revision build id), not the game-DLL attempt. The true attempts are game-dll-20261124T033057531530Z-a2c134 (generation 577, per .dev/build-timings) and game-dll-20261124T033440193460Z-3a5393 (generation 579, per .dev/state.json game_build.detail for the build running during this review).
  - evidence.reason is now a truncated tail of a compiler -ftime-report JSON blob ('...truncated':true...), not a sub-stage. The earlier [GAME_STAGE] text that carried the attempt id is gone, so structured correlation is only possible via wait_id.
- Recurrence volume: 147 issue rows titled exactly 'Wait over budget: game-dll-build at game_build' at query time (1 assigned, 129 in_review, 17 new, 0 done). Measured game-DLL durations include many >60s (gen 576 949.243s at 00:39Z; 1292.062s; 1756.786s) and near-threshold results (gen 577 64.018s at 03:30:57Z failed; gen 577 47.154s at 01:05:46Z succeeded).

INFERENCE / UNCERTAINTY
- Inference: the 60s game_build budget is below this host's baseline for a subset of builds, so per-occurrence issue+wake produces sustained noise. Supported by measured durations; it is not a human-approved target and was not measured against stated intent.
- Inference: active-attempt identity is available before completion in .dev/state.json game_build.detail (attempt id + generation), but the wait record reads a published/revision id. Same defect the prior review named.
- Unknown: which code path truncates evidence.reason; not traced. Unknown whether the 2026-11-20 event was emitted, because its journal is gone (unknown, not zero).

CONTRARY EVIDENCE
- Not every build exceeds 60s: the scan-memoized build at 01:05:46Z finished in 47.154s with object_scan 0.052s versus 259.898s for the historical occurrence. So 'budget is always too low' is too strong; the problem is a subset, often narrowly over.
- Per-occurrence dedupe and exactly-one-wake are correct; the policy is not creating duplicates for a single wait_id.

NEXT DISCRIMINATING CHECK (owner/human; not performed here)
- For one live game-DLL build that crosses 60s, assert the emitted dev-loop.wait.over_budget record's structured generation/build_attempt_id equals the attempt id in .dev/state.json game_build.detail (currently null or mismatched), and that reason carries a sub-stage rather than truncated time-report text.
- Measure completion-time distribution for the last 30 game-DLL builds (p50, p95, percent over 60s) before changing the budget or grouping policy; that is a shared-policy decision owned by issue-e044a931c54b and a human.

Acceptance for this occurrence: per-occurrence uniqueness and one-wake hold; structured attribution and original journal evidence do not. Final policy decision remains human-owned.

Cold handoff: reproduce identity via wait_id; derive the true attempt from .dev/build-timings or .dev/state.json game_build.detail. Do not trust evidence.build_id or evidence.generation.
