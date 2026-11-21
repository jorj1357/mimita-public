# issue-c3ecfab72cba: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-21T22:06:30.542Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed by build-wait-reviewer in bounded wake activation wake-565c35c38094 (coordination session-9320415811c7, work session work-6f0e642bdc10). Read-only: no source edits, no builds started.

Issue completeness: readiness ready=true, missing=[]. wait_id, stage, reason, elapsed, generation, request_ids, paths, acceptance, and proof expectation are present. No clarification needed.

Reporter behavior (acceptance met):
- Exactly one issue (issue-c3ecfab72cba) and exactly one reviewer wake (wake-565c35c38094, created 2026-11-21T21:38:23.082Z) exist for wait_id game-dll-build:2026-11-21T21:37:21.900Z. No other wake request references this issue.
- Dedupe is by wait_id in devscripts/dev-loop.py:3408-3418 (reported_wait_ids) and devscripts/build_friction.py:131-169 (_wait_issue). This is a distinct wait occurrence, so a new issue is by design, not a duplicate defect.

Reproduction confirmed; the wait is real and was actively progressing, not a stall:
- Attempt game-dll-20261121T213722107270Z-52f7a2 started 2026-11-21T21:37:22.107Z and emitted build.completed at 21:39:13.814Z, success=true, no_op=true, generation 432, changed_paths=[], total_seconds=111.683. Evidence: .dev/build-timings/game-dll-20261121T213722107270Z-52f7a2.json and .jsonl.
- It was a no-op (0 compile.* events in the JSONL; scan.snapshot scanned=642 skipped=642 stale=0), yet still exceeded the 60 s budget.
- Measured stages for this attempt (from its JSONL): resident_state codegen 0.594 s + packet_registry codegen 3.552 s; manifest_snapshot 29.806 s; manifest_validation 0.082 s; source_snapshot 17.142 s; unmeasured gap 24.03 s (source_snapshot end 21:38:16.444Z to worker_input_validation start 21:38:40.475Z); worker setup/identity 0.262 s; object_cache_scan 32.481 s; no_op 0.039 s; output handling 0.217 s.
- The wait fired at 60.305 s while manifest/source snapshot work was still in flight; the same attempt then ran about another 51 s to completion.

Root cause and ownership:
- Dominant cost is build-lifecycle overhead (manifest snapshot, source snapshot, an unmeasured pre-worker gap, and the object-cache/dependency scan), not compiler work and not a stalled queue. This belongs to canonical lifecycle issue-136d4553ff92 (owner build-coordinator), not to the wait policy. Keep the 60 s budget.
- Evidence-quality defects in this attempt summary, also relevant to issue-136d4553ff92:
  1. No-op attempts inherit a stale worker_metrics block from a prior real build. This attempt reports compiled=12, compile_seconds=32.645, object_scan_seconds=98.848 while its own JSONL has zero compile events and phase object_cache_scan=32.48 s. Attempt 449ec0 shows the same inherited values with its own scan phase of 1.033 s. A reviewer reading worker_metrics would misattribute work that did not happen.
  2. The wait reason stores a truncated GAME_STAGE JSON fragment (evidence.reason begins te\":\"completed\"), so the issue does not lead with a readable current phase; wait.build_id is the EXE build id (2026-11-21T02:26:50.306117Z), not the game-DLL attempt id. Add build_attempt_id and a clean current-phase string to the wait record.

Occurrence/noise note: The same lifecycle overhead keeps producing fresh over-budget waits on successive attempts (e.g. wait_ids game-dll-build:2026-11-21T21:50:15.395Z and game-dll-build:2026-11-21T21:56:52.660Z were recorded after this one). Because dedupe is per wait_id, each new attempt creates another issue. Reducing lifecycle overhead is what stops the flood; do not raise the threshold and do not hand-create duplicate issues.

Verdict: valid, complete, and correctly reported build-friction occurrence. No duplicate issue created. No behavior or performance fix claimed; this is an evidence review only.
