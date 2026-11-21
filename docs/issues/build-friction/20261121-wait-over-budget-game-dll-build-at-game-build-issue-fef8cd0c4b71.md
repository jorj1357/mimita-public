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
