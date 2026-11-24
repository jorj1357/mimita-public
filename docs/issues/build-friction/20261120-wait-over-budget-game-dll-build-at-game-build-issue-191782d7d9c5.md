# issue-191782d7d9c5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-20T20:36:57.060Z — build-wait-reviewer

related_build: game-dll-20261120T195523321490Z-e1b74d
related_run: 20261120_170259

## Review (confirmed)

### 2026-11-20T20:38:10.363Z — build-wait-reviewer

Reviewer: build-wait-reviewer

VERDICT: confirmed over-budget wait; the build was healthy, not stuck or queued.

Wait occurrence: wait_id game-dll-build:2026-11-20T19:55:23.269Z, stage game_build, elapsed 60.418s at detection.
Evidence: .dev/build-timings/game-dll-20261120T195523321490Z-e1b74d.json/.jsonl - build.started 19:55:23.323Z, build.completed 20:09:36.810Z, success=true, total 853.471s, generation 411, request auto-34, source_generation 34. Run: 20261120_170259. Wake: wake-a5dd648492be queued once; claimed/launched with codex at 19:56:30 (process exited), fallback claimed/launched with opencode at 20:19:29 (this activation). No duplicate wake request for this wait_id.

Measured stage breakdown for the same attempt:
- codegen + manifest_snapshot + source_snapshot: ~47s
- changed_source_preflight: 150.3s, dominated by cold compile of src/actor/player-ai.cpp (149.99s)
- object_cache_scan: 356.1s (42% of the build; scan_parse_ms 326.5s over 1501 unique dependencies)
- compile: 107.6s (13 TUs compiled, 629 skipped, jobs=12)
- link: 62.8s
- publish: 120.2s

Interpretation: the 60s budget fires ~55s into the build during codegen/manifest snapshot, before the dominant object-scan cost. A normal warm rebuild with one changed player-ai.cpp already spends ~150s on that TU alone, so a 60s crossing is expected for this workload rather than evidence of a wedge. The wait policy is reporting real elapsed time accurately.

Field completion: reproduction, expected behavior, acceptance, paths, and proof expectation were already present and accurate. Added related_build and related_run. Preserved the existing structured evidence so wait_id dedupe still works.

Recurrence: the same wait_id has no later occurrence, so no occurrence was appended. Distinct wait_ids at 08:31:55Z (issue-63dccbd8f8b6) and 08:45:25Z (issue-2008e743c710) are separate issues by build_friction.record_wait_incident design (wait_id is the uniqueness key); they are not byte duplicates, but they do create repeated same-title high-priority triage entries.

Minor accuracy note: evidence.generation was captured as 410 at wait detection while the attempt published generation 411. Not a blocker; worth normalizing if generation is used for correlation.

No source edits and no lane files claimed. Measurement and consolidation follow-up proposed on issue-7d3dcfd7433b.

## Agent suggestion

### 2026-11-20T20:39:11.238Z — build-wait-reviewer

Review limitation and evidence gap: dev-loop.py --status advertises events=C:\mimita-v9\logs\2026-11-20\20261120_170259\dev-loop\events.jsonl for run 20261120_170259, but that directory/file does not exist. The run dir contains only client-01, server-01, and run.json. The wake prompt asks the reviewer to inspect the dev-loop journal, but the advertised path is missing; I reconstructed the wait and stage timing from .dev/build-timings/game-dll-20261120T195523321490Z-e1b74d.json/.jsonl and .dev/agent-wake/events.jsonl instead. Suggest either writing the dev-loop journal to the advertised run path or making --status report the actual journal path. Not claimed or fixed here.

## Scheduled independent review

### 2026-11-24T02:57:56.152Z — hot-reload-latency-root

A bounded independent review was queued as wake wake-2d17204f557c after 496 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed)

### 2026-11-24T03:15:41.264Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (wake-2d17204f557c, 2026-11-24). Independent bounded review. Read-only: no source edit, build, lease, close, or proof claim.

VERDICT: confirmed. The recorded over-budget wait is real; the build was healthy rather than stalled or queued; the issue carries wait_id, stage, reason, elapsed, build/run IDs, and structured evidence; and exactly one reviewer wake was queued for this wait occurrence. One stage-attribution correction is recorded below.

OBSERVED FACTS (independently re-derived)
- Artifacts present: .dev/build-timings/game-dll-20261120T195523321490Z-e1b74d.json (7414 B) and .jsonl (443503 B); .dev/agent-wake/requests/wake-a5dd648492be.json.
- Build: started 2026-11-20T19:55:23.323Z, completed 20:09:36.810Z, success=true, total_seconds=853.471, generation 411, request auto-34, source_generation 34, jobs 12, compiled 13, skipped 629. changed_paths: src/actor/player-ai.cpp and src/debug/structured-log.cpp.
- Phase timings recomputed from the JSONL: object_cache_scan 356.1s (41.7% of total), changed_source_preflight 150.3s, compile 107.6s, link 62.8s, publish 120.2s. scan_parse_ms 326.5s over 1501 unique dependencies. These match the 2026-11-20 review.
- Wait: wait_id game-dll-build:2026-11-20T19:55:23.269Z, stage game_build, elapsed 60.418s, budget 60.0s. No queue delay is visible: build.started is 54 ms after the wait_id timestamp.
- Wake: exactly one wake.queued for wake-a5dd648492be at 19:56:29.235Z. codex claimed/launched 19:56:29.961/19:56:30.004Z; wake.retry to opencode 20:10:41.252Z; opencode claimed/launched 20:19:29.151/20:19:29.185Z; the review was appended at 20:38:10.363Z; the request was then finalized status=failed (adapter process exited) at 20:48:37.261Z with attempts=2. No second wake.queued exists for this wait_id.

CORRECTION / CONTRARY EVIDENCE
- The 2026-11-20 review says the 60s budget fires ~55s into the build during codegen/manifest snapshot. The timeline does not support that stage attribution. At 60.4s (~19:56:23.7Z) the active phase was changed_source_preflight (started 19:56:18.556Z), whose first TU src/actor/player-ai.cpp started 19:56:18.573Z with elapsed_ms=55249.9. codegen + manifest_snapshot + source_snapshot all completed by 19:56:10.839Z, ~12.8s before the crossing. The ~55s figure matches preflight start, not the budget crossing. The overall conclusion (expected workload cost, not a wedge) still holds.
- The declared proof expectation (dev-loop wait event plus the issue record and reviewer wake record) is only partially verifiable. logs/2026-11-20/20261120_170259 is absent and logs/11-20-2026 has 0 entries; a read-only rg pass over logs/ found no *170259* directory. The dev-loop wait event cannot be re-read from its journal; only its JSON copy in the issue evidence, the .dev/build-timings pair, and the wake request/journal survive. The 2026-11-20 review already flagged the advertised --status journal path as missing; it is still missing.

INFERENCE / UNCERTAINTY
- The cause of the missing run dir is unknown: retention pruning of the 2026-11-20 root is plausible (issue-3f776ce60511) but this is inference, not proof. Manual cleanup or another flow is not excluded.
- The final wake status=failed was written after a review had already landed. That is a wake-outcome bookkeeping symptom, not evidence the review was absent.

NEXT DISCRIMINATING CHECK (read-only)
- For raw proof: check for any retained copy of the 20261120_170259 dev-loop journal or the dev-loop.wait.over_budget event outside logs/ (archived coordinator/agent artifacts). If none exists, treat .dev/build-timings + the wake request as the surviving proof surface and narrow the issue proof_expectation to those retained artifacts instead of the pruned run path.
- For the wait policy: sample the next N over-budget waits and compare elapsed-at-detection with the active phase from .dev/build-timings JSONL. This sample shows a 60s crossing can legitimately occur in changed_source_preflight, not only during codegen.

No source, build, runtime behavior, or fix is claimed. Issue remains in_review.
