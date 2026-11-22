# issue-86b3b2ce1e73: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T06:58:45.207Z — build-wait-reviewer

related_build: game-dll-20261122T063800517650Z-620bf4

## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T06:59:14.572Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Wait occurrence reviewed from pinned markers/journals (no source edits, no build, no run).

Identity: wait_id game-dll-build:2026-11-22T06:38:00.446Z, kind game-dll-build, stage game_build, elapsed 61.153s vs 60.0s budget, recorded 2026-11-22T06:39:01.658Z, request_ids [auto-3], source_generation 3.

In-flight attempt: game-dll-20261122T063800517650Z-620bf4 (started 2026-11-22T06:38:00.517Z, 71ms after the wait start), generation 468, revision 7e002aca, changed src/npc/npc-navigator.cpp, build note b0cfb5d53647. It SUCCEEDED: build.completed 2026-11-22T06:39:15.155Z, total 74.608s. Not wedged and not a hang.

Timeline at the 61s deadline (about 06:39:01.6): link.gnu_linker was running (06:38:57.439 to 06:39:13.485, 16.05s). Progress stayed continuous: codegen 2.1s, manifest_snapshot 9.5s, snapshot_prune 6.2s, worker launch 2.1s, changed_source_preflight 20.5s, object_cache_scan 5.3s, compile 1.0s, link 16.5s, coherence_validation 0.9s, publish 0.5s.

First divergence: the flat 60s WAIT_ISSUE_SECONDS (devscripts/dev-loop.py:109) sits below normal whole-game DLL cost; check_wait_budget (devscripts/dev-loop.py:3520-3536) ages from game_build_started_utc, so any healthy build over 60s files an issue. Same conclusion as issue-6d795125c36f. No per-build regression is present here; this attempt was healthy.

Attribution defect (persists): evidence records generation 467 and build_id 2026-11-22T02:37:06.972018Z (previously published EXE artifact) while the in-flight attempt was generation 468 / game-dll-20261122T063800517650Z-620bf4. source_generation 3 is correct. related_build is now set to the in-flight attempt.

Reason field corruption (new observation): evidence.reason is a truncated tail of a translation_unit progress record (6494.473, build_attempt_id...) instead of a phase description, unlike sibling issues that carry [GAME BUILD] revision=... . The wait reason should name the active phase (here link.gnu_linker).

Recurrence: non-closed siblings with the identical title were created minutes apart (issue-c481f05923e4 at 06:34:11, issue-61edee02549f at 06:46:09, issue-9c77739954f9 at 06:52:24). record_wait_incident (devscripts/build_friction.py:131-169) dedupes only by exact wait_id, and wait_id embeds the start timestamp, so every >60s build creates a new issue, as documented in issue-6d795125c36f.

Spec alignment: docs/specs/dev-loop/change-to-evidence-latency.md mandates one issue plus one reviewer wake per exact wait occurrence and names wait_id as the dedupe key, so this per-occurrence issue is spec-conforming; the noise follows from the uncalibrated 60s budget.

Wake delivery: wake-9d90929819fc created 2026-11-22T06:39:03.173Z, claimed/launched 2026-11-22T06:47:18.120Z/06:47:18.165Z (adapter opencode), about 496s from wait detection to launch.

Verdict: not-wedged-policy-miscalibrated. No code fix claimed here. Recommended separate work already tracked: measured per-stage budgets from .dev/build-timings, signature-based canonical accumulation for repeated same-stage waits, and fixing game_build_generation/build_id attribution plus the truncated reason field.
