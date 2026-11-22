# issue-6d795125c36f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (not-wedged-policy-miscalibrated)

### 2026-11-22T06:24:27.851Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Wait occurrence reviewed from pinned markers/journals (no source edits, no build, no run).

Identity: wait_id game-dll-build:2026-11-22T05:52:23.994Z, kind game-dll-build, stage game_build, elapsed 60.848s vs 60.0s budget, recorded 2026-11-22T05:53:24.882Z, request_ids [auto-3], source_generation 3.

In-flight attempt: game-dll-20261122T055224056464Z-80d87f (started 2026-11-22T05:52:24.057Z, 63ms after the wait start), generation 462, revision 16292a5, changed path src/npc/npc-navigator.cpp, build note 408af55ca1dc. It SUCCEEDED: completed 05:54:05.767Z, total 101.696s, worker 73.37s. Not wedged and not a hang.

Timeline at the 60.848s deadline (about 05:53:24.9): changed_source_preflight was running (05:52:53.265Z to 05:53:28.877Z, 35.6s incl. npc-navigator.cpp 35.34s); object_cache_scan, compile (2.51s), and link (gnu_linker 24.50s) followed before publish. Progress was continuous.

First divergence: the flat 60s WAIT_ISSUE_SECONDS (devscripts/dev-loop.py:109) is below normal whole-game DLL cost; check_wait_budget (devscripts/dev-loop.py:3520-3536) ages from game_build_started_utc, so any healthy build over 60s files an issue. Corpus .dev/build-timings/game-dll-*.json (n=199): p50 131.9s, p75 359.9s, p90 991.7s, p95 1914.5s; 127/199 (63.8 percent) exceed 60s. A 60s issue threshold guarantees one issue per build.

Attribution defect (persists): recorded evidence carries generation 459 and build_id 2026-11-22T02:37:06.972018Z (the previously published EXE/artifact), while the in-flight attempt was generation 462. self.game_build_generation lags the active attempt at trip time. source_generation 3 is correct. Set related_build to game-dll-20261122T055224056464Z-80d87f.

Recurrence: brain holds 101 build-friction issues; 75 non-closed issues share the exact title Wait over budget: game-dll-build at game_build (58 in_review, 17 new). record_wait_incident (devscripts/build_friction.py:131-169) dedupes only by exact wait_id, and wait_id embeds the start timestamp, so every >60s build creates a new issue.

Spec alignment: docs/specs/dev-loop/change-to-evidence-latency.md:122-127 mandates one issue plus one reviewer wake per exact wait occurrence and names wait_id as the dedupe key, so per-occurrence issue creation is currently spec-conforming; the noise follows from the uncalibrated 60s budget and lack of a signature-level canonical issue.

Wake delivery: wake-bc97cbbd190e created 05:53:26.641Z, claimed/launched 06:07:25.095Z/06:07:25.131Z (adapter opencode, attempts 2), about 14 min late. Detection and queueing were correct; delivery lagged.

Verdict: not-wedged-policy-miscalibrated. No code fix claimed here. Recommended separate work: measured per-stage budgets from .dev/build-timings, signature-based canonical accumulation for repeated same-stage waits, and fixing game_build_generation attribution to the active attempt.

## Context

### 2026-11-22T06:24:48.200Z — build-wait-reviewer

related_build: game-dll-20261122T055224056464Z-80d87f
evidence: {'text': 'Attribution: in-flight attempt generation 462 succeeded in 101.696s (worker 73.37s); evidence recorded generation 459/build_id 2026-11-22T02:37:06.972018Z was stale. Timing corpus n=199 p50=131.9s; 63.8% over 60s. 75 non-closed duplicate-title wait issues.'}
