# issue-ad873eff97f3: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-structural-object-cache-scan)

### 2026-11-21T16:32:43.381Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: confirmed real wait; structural, not a false alarm or a duplicate of a prior wait_id. The 60.782s is roughly 1/10 of the actual build, and the dominant cause is object_cache_scan, not the changed-source compile.

Resumed build and exact evidence
- build_attempt game-dll-20261121T134938183745Z-c0c1da, generation 420, revision f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f.
- build.completed 2026-11-21T13:59:58.446Z, success=true, total_seconds 620.249, worker_total_seconds 561.855. The wait started 13:49:38.158Z, so the loop was correct to flag 60.782s.
- Only src/network/multiplayer-projectiles.cpp changed, yet 170 translation units compiled and 472 were skipped. The object cache identity changed from 78f9ca5eff0bc398 to b10e3a31b3ae8637 at gen 420, which explains 169 stale units plus the 1 changed unit.

Phase timeline from .dev/build-timings/game-dll-20261121T134938183745Z-c0c1da.jsonl
- codegen 20.5s, manifest/source snapshot 15.0s, changed_source_preflight 19.98s, object_cache_scan 86.35s, compile 418.84s, link 34.74s, publish 13.43s.

First divergence for the wait
- The wait fires before compile work starts. object_cache_scan alone (86.35s) exceeds the 60s WAIT_ISSUE_SECONDS budget.
- The first scan.snapshot is emitted at scan_seconds 79.613s, so nearly the entire scan is the prepass. Recorded prepass timers are parse_ms 9210 and stat_ms 166, which explain only about 9.4s; roughly 70s of the prepass is untimed.
- scan_entries calls dependency_relative for every dependency token (devscripts/game_build_worker.py:1325-1327, :804-816), and dependency_relative calls Path.resolve per token. The current object-cache dependency-index.json holds 642 sources and 160,446 dependency token occurrences. This is the leading hypothesis for the untimed ~70s; confidence is medium because local timing samples were taken while another build was active.

This is a recurring structural pattern, not a one-off
- Earlier incremental builds gen 414/415/416 recompiled only 12 units but spent 318.4s / 340.3s / 382.9s in object_cache_scan versus 54.7s / 44.1s / 46.1s compiling, with totals 565s / 563.5s / 621s. object_cache_scan, not compile, is why routine game_build waits exceed 60s and why many "Wait over budget: game-dll-build at game_build" issues exist.

Acceptance and proof state
- wait_id, stage, elapsed, budget, build attempt and revision are present; reviewer wake was queued once (wake-39584990b85c, prompt log .dev/agent-wake/logs/2026-11-21T13-50-39.358Z-build-wait-reviewer.prompt.txt). The issue evidence.reason field is a malformed JSON fragment rather than a human-readable reason.
- A direct dev-loop.wait.over_budget journal line was not found: state.json events_path is None and status shows events=(none) when no run is active, so durability of the wait event currently rests on the issue and wake records only.

Recommended fixes (report-only; no code changed or claimed)
1. Attribute over-budget waits to the dominant substage and consolidate repeated game_dll/object_cache_scan waits under tracker issue-7d3dcfd7433b instead of one high-priority issue per wait_id.
2. Remove Path.resolve from the per-token prepass: header fanout only needs a normalized relative path; use normpath/normcase on the already-normalized index keys and cache token-to-relative per scan.
3. Add sub-timers to the prepass (index load, depfile parse, token classify, stat prewarm) and emit scan_prepass timing rows so the untimed ~70s becomes provable.
4. Consider a phase-aware wait budget or duplicate-wake suppression while the tracker is open, to stop waking a reviewer on every known 6-10 minute build.

## Scheduled independent review

### 2026-11-27T05:31:55.347Z — opencode-deepseek-v4-1

A bounded independent review was queued as wake wake-b01290c01eba after 828 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (historical-occurrence-confirmed; original-object-cache-scan-cause-remediated; acceptance-not-met-on-reason-and-build-run-identity)

### 2026-11-27T05:37:40.424Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (wake-b01290c01eba, session session-031e62001045). Bounded, read-only: no source edits, no build/test started, no lease taken, no fix claimed, no proof recorded, issue not closed.

VERDICT: historical wait record is real and the originally diagnosed cause is now remediated in source; acceptance is still NOT fully met because the readable reason and the in-flight build/run identity are wrong/stale. This occurrence should be folded into the existing wait/cache trackers, not treated as an active defect.

WINDOW / ARTIFACTS EXAMINED (UTC)
- Issue readiness: ready=true, score 8/8, state in_review, 0 tasks, no proof rows (nothing to claim).
- Original occurrence: wait_id game-dll-build:2026-11-21T13:49:38.158Z, stage game_build, elapsed 60.782s vs 60.0 budget, started_utc 2026-11-21T13:49:38.158Z.
- Original reviewer wake: wake-39584990b85c, created 2026-11-21T13:50:39.362Z, issue issue-ad873eff97f3 (.dev/agent-wake/events.jsonl:530; .dev/agent-wake/requests/wake-39584990b85c.json).
- Prior review review id 49 (build-wait-reviewer, 2026-11-21T16:32:43.380Z) is preserved and remains the substantive diagnosis.
- Current evidence: .dev/build-timings/game-dll-20261127T053122462512Z-024d4c.json (gen 775, total 14.453s, object_cache_scan 0.0ms), .dev/build-timings/game-dll-20261127T052703291260Z-e8d3e3.json (gen 774, total 38.609s, object_cache_scan 15.0ms).
- Source: devscripts/game_build_worker.py:1219-1245 dependency_relative now resolves lexically and documents that filesystem Path.resolve() per token "was dominating the index prepass".
- Adjacent current-cause review: issue-4efc86281237 review (2026-11-27T04:46:32.223Z) — the 2026-11-27 61.713s wait was a 646-TU full cold rebuild after object-cache/dep-file invalidation, not object_cache_scan.
- Tracker: issue-7d3dcfd7433b "Measure every dev-loop wait stage and reduce avoidable latency".

ACCEPTANCE CHECK (verbatim: "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence; a reviewer wake is queued exactly once for this wait occurrence.")
- wait_id: PASS (game-dll-build:2026-11-21T13:49:38.158Z).
- stage: PASS (game_build).
- elapsed: PASS (60.782s vs 60.0).
- evidence: PASS (present).
- reason: FAIL (substantive). evidence.reason is a raw truncated JSON tail (":58925.271,\"build_attempt_id\":\"game-dll-20261121T134938183745Z-c0c1da\",..."), not a readable stage/file/elapsed summary. Same defect as the 2026-11-27 issue-4efc86281237 and tracked by issue-6e08d89b55e3.
- build/run IDs: FAIL. evidence.build_id is "2026-11-21T02:26:50.306117Z" (an earlier published build) and generation is null; the in-flight attempt was game-dll-20261121T134938183745Z-c0c1da / generation 420. No run_id recorded. Same stale-identity family as issue-3878b275e5d9 and issue-f36679f4ba95.
- reviewer wake exactly once: PASS for queueing (one wake.queued event for wake-39584990b85c at 13:50:39.362Z; no second wait-occurrence wake for this wait_id). Note the current wake-b01290c01eba is the scheduled independent review (coord.issue_review.scheduled, sweep key issue-review-sweep-828), not a second wait wake.
- Net: 4/6 fields pass, reason and build/run identity fail. Acceptance NOT fully met.

OBSERVED FACTS
1. The original occurrence was a true over-budget wait; the 60s wall-clock policy fired while a real build was in flight, so the issue itself is not a false alarm.
2. The prior review's leading cause (per-token Path.resolve in dependency_relative) is addressed on current HEAD: the function now resolves lexically (game_build_worker.py:1219-1245). Current incremental builds show object_cache_scan 0.0-0.045s and totals 14.5-38.6s, versus the 86.35s scan / 620.2s total recorded for the original attempt. The original performance cause no longer reproduces on HEAD.
3. Current over-budget waits still occur but have a different dominant cause: full cold rebuilds from cache/dep-file invalidation (compiled 646, skipped 0) plus a wall-clock-only 60s budget (devscripts/dev-loop.py:3850-3922, WAIT_ISSUE_SECONDS at :114-115). Those are tracked under issue-4efc86281237 / the object-cache link regression docs.
4. The wait issue generator intentionally creates a new issue per wait_id even for an identical stage and reason (devscripts/build_friction.py:143-199 docstring: "a new wait gets another issue even when its stage and reason are identical"). 43 open "Wait over budget" build-friction issues exist as of this review, all state in_review.

INFERENCE / UNCERTAINTY
- I could not independently re-verify the original 86.35s scan, 418.84s compile, or 620.2s total: the original attempt summary (game-dll-20261121T134938183745Z-c0c1da) is no longer in .dev/build-timings, and logs/11-21-2026 is an empty directory (journal pruned). This is an evidence-durability gap: the issue's proof expectation named the dev-loop wait event, and that primary journal is gone. The prior review's numbers are therefore accepted as quoted, not re-measured. Unknown, not zero.
- The wake record wake-39584990b85c ends status=failed, attempts=2, updated 17:01:40Z, yet the reviewer review exists at 16:32:43Z. Whether a retry produced the review or the scheduler mislabeled a completed run is not determined; the queue count remains one. Tracked by the wake-adapter issue family (issue-36579a10979a), not here.
- I did not run a build or a runtime, so no claim is made about current overall wait latency beyond the two summarized build artifacts.

CONTRARY EVIDENCE
- Against "this wait is still unexplained": the diagnosed object_cache_scan cause is demonstrably remediated in source and in current timings; the 2026-11-27 recurrence has a distinct, documented full-rebuild cause.
- Against "duplicate wake": there is exactly one wait-occurrence wake for this wait_id; the later sweep wake has a different trigger and purpose.
- Against "issue is a false positive": the resume evidence in the prior review shows a 620s build that the 60s policy correctly caught.

NEXT DISCRIMINATING CHECK (smallest)
1. On the next NEW wait issue (not this historical one), inspect evidence.reason and evidence.build_attempt_id/generation/run_id. If reason is still a JSON slice or build_id is stale, leave the fixes with issue-6e08d89b55e3 and issue-3878b275e5d9; if they are now readable/correct, those defects can be verified closed.
2. Historical closure path (owner/human decision, not taken here): fold this occurrence into the object-cache/link-regression tracker and issue-7d3dcfd7433b, then close this record as superseded. The occurrence itself needs no new build or run.
3. Measure the duplicate-issue proposal appended to issue-7d3dcfd7433b this sweep (new wait issues per 24h vs occurrences appended).

PROCESS REVIEW
- Repeated friction observed: 43 open one-per-wait_id "Wait over budget" issues (2026-11-21 through 2026-11-27), each in_review, with prior reviews twice recommending consolidation and no change yet. This is a measurable avoidable-reviewer-load cost, not a one-off. One specific proposal with a measurable target and smallest experiment was appended to issue-7d3dcfd7433b (the wait-latency tracker), not to the general self-improvement issue-91e3384b219b, because this evidence is specific to dev-loop wait issue creation.
- No human-only step or unsafe capability was needed for this review; no process improvement beyond the above is supported by the evidence in this sweep.

STATUS: review appended; no source, build, test, fix, lease, or closure performed. acceptance still not fully met on reason and build/run identity; original performance cause remediated; occurrence folded to existing trackers pending owner/human action.
