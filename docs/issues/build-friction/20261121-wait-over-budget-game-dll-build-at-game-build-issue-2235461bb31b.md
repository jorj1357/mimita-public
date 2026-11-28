# issue-2235461bb31b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-preworker-wait-attribution-gap)

### 2026-11-21T21:56:08.232Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned game-DLL attempt, the dev-loop wait policy, and the reviewer wake record. Conclusion: confirmed real over-budget wait, not a hung build. The waiting attempt was a healthy no-op DLL build; the 60s budget was exceeded in pre-worker staging. The auto-created issue also mis-attributes the wait (generic stage, wrong build_id), which is a known, still-open evidence gap.

WAIT OCCURRENCE (verified)
- wait_id game-dll-build:2026-11-21T21:33:20.879Z; started 21:33:20.879Z; elapsed 61.049s; budget 60.0s; reported stage "game_build"; generation null; build_id 2026-11-21T02:26:50.306117Z (EXE artifact, not the DLL attempt).
- The 61s check landed inside manifest_snapshot: .dev/build-timings/game-dll-20261121T213320985628Z-1a2692.jsonl shows manifest_snapshot started 21:33:53.743Z (elapsed 32.749s) and completed 21:34:23.284Z (62.290s). The truncated reason in the issue is that same started row.
- Wake record: wake-9b5f268395f8 queued exactly once at 21:34:23.386Z; codex adapter launched 21:34:23.774Z then exited; retried on opencode at 21:38:23.796Z; claimed/launched 21:41:25.564Z/21:41:25.603Z. Acceptance "one reviewer wake per occurrence" is met, but queue-to-review was about 7 minutes because of the adapter fallback.

PINNED BUILD ATTEMPT
- attempt game-dll-20261121T213320985628Z-1a2692, request 2026-11-21T21:33:20.596Z-29fe5ab4; started 21:33:20.994Z, completed 21:35:17.415Z, exit 0, total 116.420s, no_op=true, generation 432 (unchanged), revision 34dcd763673b.
- Pre-worker staging: resident_state codegen 1.110s; packet_registry codegen 30.888s; manifest_snapshot 29.110s; manifest_validation 0.053s; source_snapshot 35.666s. The worker then scanned 642 objects, skipped 642, compiled 0, linked 0, no_op 0.003s (object_cache_scan 0.036s; worker input validation 0.012s).
- So about 97s of pre-worker snapshot/codegen work produced a no-op; the wait was not caused by compile/link and there was no stall or retry.

CONTEXT
- Adjacent no-op attempts 52f7a2 (21:37:21) and 449ec0 (21:40:24) deliberately exercised the scan-memoization trials tracked in issue-136d4553ff92 (report docs/reports/20261121_214300-warm-dll-cache-scan-optimization-results.md). They are not unexplained redundant builds.

FIRST DIVERGENCE / EVIDENCE GAP (still open)
- docs/specs/dev-loop/change-to-evidence-latency.md requires the auto-created wait issue to carry the exact build/run IDs and to append later observations. This issue cannot name the waiting attempt from its own fields: stage is the flat "game_build" instead of the real manifest_snapshot; build_id is the latest EXE artifact; generation is null because dev-loop.py:3396-3406 reads self.game_build_generation, which is only assigned after success.
- No terminal result was appended to this issue (occurrences=[]), because dev-loop.py:3408-3418 suppresses repeats of the same wait_id in reported_wait_ids, even though build_friction.py:164-169 supports issue_occurrence.
- Per-attempt summary .json still copies stale worker_metrics: .dev/build-timings/game-dll-20261121T213320985628Z-1a2692.json reports worker_metrics.total_seconds=170.04, compiled=12, compile_seconds=32.645, object_scan_seconds=98.848, link_seconds=10.942 while no_op=true, compile_sum_ms=0, changed_paths=[], and top-level worker_total_seconds=0.426. The same block appears in 52f7a2 and 449ec0. This recurrence of the stale-summary defect is already recorded in issue-136d4553ff92 (2026-11-21 append entries), so it needs no new issue.

ASSESSMENT / NEXT
- This is wait-policy noise over a real performance problem, not a build failure. A flat 60s budget is below the ~97-116s that pre-worker snapshot/codegen currently takes for an unchanged tree, so such attempts will file an issue and wake a reviewer each time. 41 build-friction issues with this exact title already exist.
- No fix belongs in this issue and none was made: no source edited, no build/test requested. The owner of the performance and attribution fix is issue-136d4553ff92 (build-coordinator): emit the active attempt id and target generation in wait evidence, record the true pipeline stage, append the terminal result to the existing issue, and source every attempt summary metric from that attempt. The default-budget question remains open per docs/specs/dev-loop/agent-build-queue.md:314.
- Confidence: high (pinned attempt JSONL/JSON, adjacent attempts, wake events, ready.json). No runtime behavior involved.

## Scheduled independent review

### 2026-11-28T18:31:34.485Z — magic-wand-cpu-expansion

A bounded independent review was queued as wake wake-32598382a055 after 916 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-attribution-gap-persists-volume-264)

### 2026-11-28T18:39:26.947Z — issue-review-sweeper

Reviewer: issue-review-sweeper

INDEPENDENT REVIEW (2026-11-28) - issue-review-sweeper, wake-32598382a055.
Scope: re-verified this 2026-11-21 wait occurrence and its two recorded gaps against the CURRENT dev-loop source and the retained 2026-11-28 journals. Read-only: no source edited, no build/test requested, no proof claimed, issue not closed, no fix claimed.

ISSUE STATE / READINESS / PROOF
- issue readiness: ready=true, score 8/8, missing=[], tasks=0, state=in_review, blockers=[].
- Proof records on THIS issue: none (proof list --issue issue-2235461bb31b -> []). This is appropriate for a build-friction wait record; no gameplay/runtime behavior is involved, so there is nothing to runtime-prove here. The workspace-wide proof status (proven 5 / stale 44) is unrelated to this issue.

OBSERVED FACTS (direct, current)
1. Attribution gap still present in current source. devscripts/dev-loop.py:3904-3920 builds the game-dll wait record with stage=game_build (flat, not the real pipeline stage), build_id=self.latest_build (last published EXE artifact), generation=self.game_build_generation, and wait_id=f'game-dll-build:{self.game_build_started_utc}' (no attempt id). self.game_build_generation is assigned only after success (dev-loop.py:3277 / prior reviews cite :3259), so during a wait it is the previous generation or None.
2. Current 2026-11-28 journals reproduce the exact misattribution. All 7 dev-loop.wait.over_budget rows retained under logs/2026-11-28/**/dev-loop/events.jsonl report stage=game_build and the SAME build_id 2026-11-27T17:45:19.700118Z (a stale EXE artifact): 07:57:37Z gen813 issue-d7dc1872d630; 08:42:37Z gen816 issue-e8e596d81554; 09:03:31Z gen817 issue-226ec498afe3; 17:31:56Z gen820 issue-747933df08a6; 17:35:26Z gen820 issue-6ed1b2897c5b; 17:41:56Z gen820 issue-004c549df353; 18:12:24Z gen825 issue-0e64599cd133.
3. The generation field is not merely null now; it is the stale previous value. The 18:12:24Z row reports generation=825 while attempt game-dll-20261128T181102041294Z-a9cdb2 (worker_started 18:11:09.337Z) published generation 826 at 18:13:07.066Z (build/game/current.json; a9cdb2 changed src/network/server-gamemode.cpp, compiled 30, total 125.093s). So a cold-handoff reviewer still cannot name the in-progress target from the wait record.
4. Terminal result still not appended. dev-loop.py:3922-3931 dedupes by wait_id in self.reported_wait_ids before/around record_wait_incident, so build_friction.record_wait_incident is not re-called for the same id and the occurrence branch (build_friction.py:131-140, 164-169) never records a completion. This issue still has occurrences=[].
5. Backlog has grown sharply since the first review. Exact-title 'Wait over budget: game-dll-build at game_build' issue docs: 41 on 2026-11-21 (prior review) -> 264 on 2026-11-28 (first-line title match). Total build-friction issues 321: 283 in_review, 31 new, 6 assigned, 1 deferred, 0 done/closed. Of the 264 exact-title: 244 in_review, 19 new, 1 assigned, 0 done/closed. Daily exact-title creation: 20261120=21, 1121=26, 1122=83, 1123=14, 1124=37, 1125=19, 1126=26, 1127=28, 1128=10 so far. Cross-thread counts were 227 on 2026-11-24 and 249 on 2026-11-27, so the growth is continuous.
6. Healthy builds routinely exceed the flat 60s budget. Latest a9cdb2 total_seconds=125.093 (compiled 30); 3007f5 total_seconds=134.187 (compiled 47). The 60s trigger therefore fires on ordinary successful builds, not only on stalls.

INFERENCE / UNCERTAINTY
- Inference (high confidence): the hook triggers on whole-attempt wall age with a 60s budget below normal game-DLL build time, and wait_id is keyed on attempt start time, so each >60s attempt mints a distinct issue plus a reviewer wake. Because the recorded fields cannot name the in-progress attempt, every reviewer must reconstruct attribution. The 264-issue, 0-closed backlog is the measurable consequence.
- Uncertainty: the original 2026-11-21 attempt summary (.dev/build-timings/game-dll-20261121T213320985628Z-1a2692.json) is pruned (not retained), so the prior review's cited 116.420s / 97s-preworker numbers are treated as unverified historical evidence, not re-confirmed here.
- Uncertainty: a flat budget is partly a policy choice; docs/specs/dev-loop/agent-build-queue.md keeps the default-budget question open.

CONTRARY EVIDENCE
- Not every long attempt is noise. eb3d52 (game-dll-20261128T174046257052Z) has failed_phase=attempt_timeout, total 1086.062s, during a deliberate object-cache quarantine cold rebuild (note: 'Object-cache quarantine ... cold build', expected_seconds 1000). That is a real, expected long operation, not an unexplained stall, but it still trips the same 60s hook.
- The stale worker_metrics defect flagged on 2026-11-21 is NOT reproduced in retained recent no-op attempts: 96489e (gen750), 2d2614 (668), e5b99b (651), 5eb933 (632) all show no_op=true with coherent compiled=0 / compile_sum_ms=0 / worker_metrics.total_seconds == worker_total_seconds. The original 1a2692 summary is pruned, so the 2026-11-21 claim can be neither confirmed nor refuted from current artifacts; treat it as historical, not current.
- The literal acceptance 'one reviewer wake per occurrence' still holds: each wait_id maps to one issue + one wake. The problem is distinct occurrences, not duplicate creation for one occurrence.

BUILD RESOURCE GAPS (for owner, already conceptually tracked, not re-filed)
- brain_check surfaced 3 new gap keys: build-resource-sampling:game-dll-20261128T181102041294Z-a9cdb2, ...180034349425Z-3007f5, ...174046257052Z-eb3d52. Each reports unavailable matched_prebuild_resource_baseline, host_commit_charge_and_limit, and journal_aligned_live_mode_workload_and_frame_tick_metrics. This is the same matched-pre-build-baseline gap already recorded in issue-136d4553ff92, so I did not open a duplicate suggestion; the owner should fold these keys into that existing telemetry work. Missing sample is unknown, not zero, and no host-pressure cause is inferred.

LINKED ARTIFACTS
- Wait journals: logs/2026-11-28/20261128_*/dev-loop/events.jsonl (7 rows above).
- Attempt summaries: .dev/build-timings/game-dll-20261128T181102041294Z-a9cdb2.json, ...180034349425Z-3007f5.json, ...174046257052Z-eb3d52.json.
- Published build: build/game/current.json generation=826, revision=0bb9684f..., status=ok.
- Source: devscripts/dev-loop.py:3904-3920, :3277, :3922-3931; devscripts/build_friction.py:131-140, 143-169, 244-246.

NEXT DISCRIMINATING CHECK
- Attribute one wait end-to-end without reconstruction, measurement-only and owned by issue-136d4553ff92: emit the active build attempt id (game-dll-<ts>-<hash>) and the target generation into the wait evidence from the already-parsed attempt stream, and append exactly one terminal occurrence (attempt id + status + final generation) when that attempt completes, leaving the 60s trigger unchanged. Success = for a single over-budget attempt the issue's own fields name the attempt_id and final generation, and exactly one completion occurrence is appended.
- Volume lever (separate, owner issue-9a4503f6ed68): compare new wait issues per day at the current budget vs the measured p95 of successful attempts (recent samples exceed 125-134s; older samples 180-700s), holding the tree/workload constant.

ASSESSMENT
- Confirmed: the wait is real and the recorded attribution gap still exists in current source; this issue is not fixed. No fix belongs in this issue and none was made.
- Owner for attribution + per-attempt metrics remains issue-136d4553ff92 (build-coordinator). Volume/noise policy belongs to the wait-hook owner issue-9a4503f6ed68.
- Confidence: high on source and current journals; medium on the historical stale-metrics claim due to pruned artifacts.
