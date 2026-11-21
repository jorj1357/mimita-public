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
