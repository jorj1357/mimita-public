# issue-fef8cd0c4b71: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-21T20:11:01.704Z — build-wait-reviewer

related_build: game-dll-20261121T195311434653Z-0a1309

## Review (confirmed-build-wait)

### 2026-11-21T20:11:12.063Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in a bounded wake activation (wake-0047facffc17). Read-only: no source edits, no builds, no leases, no fix claimed.

Verdict: confirmed-build-wait — the wait is real but expected build latency; it is not a stuck stage. The build completed successfully.

Issue completeness: `issue readiness` ready=true, missing=[], score 8/8. wait_id, stage, reason, elapsed (61.042s), budget (60s), generation, build_id, request_ids, reproduction, expected behavior, acceptance, paths, risk, and proof expectation are present. Acceptance met: one issue for this wait_id and exactly one reviewer wake queued (wake-0047facffc17 at 19:54:13.157Z; the first codex child went stale and was recovered once as opencode at 19:57:08.720Z; no duplicate wake and no duplicate issue for this wait_id).

Mapping: wait game-dll-build:2026-11-21T19:53:11.319Z = build attempt game-dll-20261121T195311434653Z-0a1309 (generation 430, revision b16fc891, changed src/actor/player-ai.cpp). Summary: success=true, total 316.709s, completed 19:58:28.180Z.

Where the first 60s went (.dev/build-timings/game-dll-20261121T195311434653Z-0a1309.jsonl):
- 19:53:11.319–19:53:54.9 (~43.4s) pre-worker staging: codegen checks, manifest snapshot, and source_snapshot copy into .dev/build-snapshots/... (final copy stage alone 17.981s).
- 19:53:54.9–19:54:00.9 (~6.6s) gap before worker_input_validation.
- 19:54:00.9 worker starts (49.5s after the wait started); changed_source_preflight then compiles the single changed TU src/actor/player-ai.cpp (71.158s).
- The 60s budget was crossed at 19:54:12 while preflight was still running; the issue was filed at 19:54:12.473Z. The build had not yet reached object_cache_scan, parallel compile, link, or publish.

Full wall accounting (build.started 19:53:11.4 → build.completed 19:58:28.2, 316.709s):
- pre-worker staging ~49.5s
- changed_source_preflight 71.16s (player-ai.cpp compiled serially before the scan)
- object_cache_scan 83.17s (full 642-unit scan while only 1 TU changed)
- parallel compile of 12 remaining TUs 17.46s (phase compile)
- link 42.71s
- coherence_validation 0.91s
- publish phase 50.10s
The fixed 60s budget is far below the ~317s minimum for this path, so the issue will fire on essentially every non-cached DLL build.

Measurement gap (actionable): the publish phase measured 50.096s in the timeline, but worker_metrics.publish_seconds records only 1.258s, which covers only rename + sha256 in devscripts/game_build_worker.py:1945-1967. The remaining ~49s is spent after publish_seconds is snapshotted, in the post-hash publish subphases (collect_external_dependencies at :2027, write_cache_marker at :2033, current/generation metadata writes at :2017-2024) before progress.finish at :2042. Similarly worker_metrics.total_seconds (266.501) excludes the 50.2s publish phase versus build.completed 316.709, and devscripts/dev_loop/game_build.py:847-855 copies publish_seconds as if it were the full publish cost. The summary therefore under-reports publish latency ~39x and hides about one sixth of wall time.

Recurrence: the next attempt game-dll-20261121T200541886892Z-5b40e3 (generation 431) started 20:05:41.887Z and had already spent 77.7s in packet_registry_codegen plus 19.0s manifest/source snapshot (102.5s total) before its worker started at 20:07:24.438Z; it produced a separate wait issue (issue-81d40f5f20b3) at 20:06:42. This corpus now has many near-identical "Wait over budget: game-dll-build at game_build" issues (issue-81d40f5f20b3, issue-905f128c10a1, issue-194c538d7cbc, issue-dc6792df3815, issue-ad873eff97f3, issue-4c12dde1d7ba, issue-ca900e60ed4b, issue-9215d710021d), which is the duplicate-issue defect already flagged in issue-bb273ac1386c.

Recommendation (for the owner; not changed here):
1. Treat this as expected build latency, not a hang; link to the end-to-end DLL accounting issue issue-136d4553ff92 and the wait-measurement issue issue-7d3dcfd7433b.
2. Either derive the game-dll-build wait budget from rolling build duration, or exempt/annotate while the pinned attempt is actively emitting phase/TU events (devscripts/dev-loop.py check_wait_budget, lines 3346-3407).
3. Count the whole publish phase, not only rename+hash, in publish_seconds and the summary (devscripts/game_build_worker.py:1945-2042; devscripts/dev_loop/game_build.py:847-855).
4. Attach repeat wait occurrences to one canonical issue instead of opening a new issue per wait_id.

Confidence: high for the wait-to-attempt mapping and the first-60s breakdown (exact journal events); high that this is expected build latency, not a wedge (build completed ok); medium for splitting the ~49s post-hash publish between collect_external_dependencies and write_cache_marker, because per-subphase timings were not retained and progress.json has since advanced to the next attempt.

## Scheduled independent review

### 2026-11-27T17:30:22.154Z — map-authored-parity

A bounded independent review was queued as wake wake-5703a0a88be6 after 847 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (confirmed-expected-build-latency; recurrence+wake-delivery defects persist; per-occurrence acceptance met)

### 2026-11-27T17:55:43.639Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review — issue-review-sweeper (wake-5703a0a88be6), 2026-11-27T17:3xZ. Read-only: no source edit, no build/test, no lease, no close, no fix/proof claim.

VERDICT: confirmed-expected-build-latency; recurrence and wake-delivery defects persist; acceptance for this specific wait occurrence is met.

READINESS / PROOF
- issue readiness: ready=true, score 8/8, missing=[], state=in_review, owner=<none>, tasks=0.
- No issue-level proof row in `proof list` for this issue. For a tooling wait the expected proof is the wait event + issue record + reviewer wake (all artifacts checked below), not a runtime game event. Prior review id 56 (build-wait-reviewer, 2026-11-21T20:11:12Z) already mapped this wait; this review adds fresh 2026-11-27 evidence and does not restate it.

OBSERVED FACTS (direct)
1. Wait record: wait_id game-dll-build:2026-11-21T19:53:11.319Z; evidence budget_seconds=60, elapsed=61.042s, stage=game_build, build_id=2026-11-21T02:26:50.306117Z; related_build game-dll-20261121T195311434653Z-0a1309.
2. Exactly one reviewer wake is recorded for this occurrence: wake-0047facffc17 (created 2026-11-21T19:54:13.157Z, issue=this). Its recorded status is `failed` ("adapter process exited", attempts=2), yet it produced build-wait-reviewer review id 56 at 20:11:12Z. So wake status is not a reliable proxy for whether a review was produced.
3. Budget policy unchanged: WAIT_ISSUE_SECONDS still defaults to a flat 60s (devscripts/dev-loop.py:114, env MIMITA_WAIT_ISSUE_SECONDS), applied uniformly to exe-build, exe-build-queue, and game-dll-build (dev-loop.py:3840-3901). check_wait_budget creates one issue + wake per wait_id.
4. Latest relevant build, generation 799, attempt game-dll-20261127T170504574022Z-3ad5f1: build.started 2026-11-27T17:05:04.689Z → build.completed 17:16:44.396Z, elapsed 699.444s, exit 0, published. build/game/progress.json: compile_s=628.355, link_s=43.276, object_cache_scan phase 2.614s, publish phase 606.888ms. current.json: publish_seconds=0.498, total_seconds=698.375. A healthy single build is ~11.7x the 60s budget; today's wait issue issue-4ddfb305b25b was created 17:06:06Z (~62s into this attempt).
5. Corpus (fresh, `issue list --kind build-friction --limit 1000`): 306 build-friction issues, 282 "Wait over budget"; 249 are exactly "Wait over budget: game-dll-build at game_build"; states 258 in_review, 23 new, 1 assigned. Created-by-day: 11-20=29, 11-21=27, 11-22=96, 11-23=15, 11-24=45, 11-25=19, 11-26=26, 11-27=25. Per-occurrence uniqueness is keyed on the timestamped wait_id (devscripts/build_friction.py:131-200), so each distinct wait mints a new issue. That is the accepted design in issue-9a4503f6ed68, not a regression against that spec.
6. Wake delivery (fresh): 301 request files with agent build-wait-reviewer; 296 recorded status=failed (175 "adapter process exited", 120 "adapter process exited; reconciled by scheduler", 1 WinError 5), 3 launched, 2 queued. Failed status is NOT proof of no-review: wake-0047facffc17 (this issue) produced review 56, while failed-status wakes for issue-f995213a15b0 and issue-af958c3e10eb produced no build-wait-reviewer review and were instead swept days later by issue-review-sweeper (2026-11-26T15:52Z "wake queued once but delivery failed; unowned backlog flood"; 2026-11-24T00:54Z "reviewer declined as test activation; no-review-until-sweep").
7. Publish accounting (contrary to the prior review's largest omission): publish_seconds is still snapshotted immediately after publish.rename_and_hash (devscripts/game_build_worker.py:2811), excluding publish.current_metadata/generation_metadata/external_dependency_manifest/cache_marker (lines 2869-2897). In gen 799 those four subphases total ~58ms vs publish phase 606.9ms, so publish_s under-reports by ~0.11s (~18%). The prior review's ~49s publish gap does NOT reproduce here: external_dependency_manifest is 10.2ms because it now reuses scan results (lines 2878-2887), so that specific slow subphase is mitigated; the structural omission remains but is currently small.

INFERENCE / UNCERTAINTY
- High confidence the wait is expected build latency, not a wedge (build exit 0, gen 799), and high confidence the wait will keep firing on essentially every non-cached DLL build (699s >> 60s).
- High confidence (code + artifacts) that per-occurrence creation plus the flat 60s budget cause the 249-issue identical-title backlog. Medium confidence whether that aggregate volume is economically unacceptable — that is a design decision owned by issue-9a4503f6ed68 / issue-7d3dcfd7433b.
- High confidence that wake status "failed" is unreliable (mixed review outcomes for identical failure strings). Unknown without sampling how many of the 296 failed wakes actually produced reviews; I did not inspect all 249 issues' review history.
- I did not force a fresh >60s build; this review relies on the 2026-11-27 attempt already in the journal. No gameplay/runtime proof is applicable.

CONTRARY EVIDENCE
- Acceptance for THIS occurrence is met: one issue carries wait_id/stage/reason/elapsed/build id and exactly one reviewer wake request exists (wake-0047facffc17).
- Builds complete successfully; the wait machinery is not wedging builds.
- The prior review's dominant publish-measurement gap (~49s) is largely mitigated by scan-result reuse (external_dependency_manifest ~10ms); only a ~0.11s structural under-count remains.
- The trigger is workload-dependent and has been dormant between heavy builds (a 2026-11-23 sweep saw the last 12 DLL builds at 15.1-58.6s); today's 699s build re-opened it.

NEXT DISCRIMINATING CHECK (smallest, for the owner)
1. For the newest wait issue issue-4ddfb305b25b (created 2026-11-27T17:06:06Z for the attempt starting 17:05:04), confirm whether its build-wait-reviewer wake produced a review; then sample 10 failed-status wakes across days and cross-check issue reviews to quantify the true delivery rate. Correct the status signal (review-produced vs adapter-exit) before adding any retry logic.
2. Decide the volume policy on issue-9a4503f6ed68: either keep per-occurrence issues by design and accept ~25/day on heavy-build days, or gate the budget on rolling build duration (e.g. max(60, 1.2xp95 of the last N successful DLL builds)) and/or attach repeated same-kind+stage occurrences to one canonical issue. This is a design change, not a review-time fix.

STATUS: read-only; no source edit, no build/test, no lease, no fix, no proof, no close.
