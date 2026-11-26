# issue-be98253c272f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed: real wait, budget misfit, unreadable reason)

### 2026-11-26T05:52:13.558Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Review of wait game-dll-build:2026-11-26T04:10:38.647Z (dev-loop run 20261126_040711)

VERDICT: Wait is real and the build succeeded; the 60s flat budget is marginally too tight for normal game-DLL builds, and the recorded reason is an unreadable log-line fragment. Recurring noise, not a stall.

OBSERVED (direct):
- Build game-dll-20261126T041038797106Z-596df3 succeeded: generation 684, status ok. Journal logs/2026-11-26/20261126_040711/dev-loop/events.jsonl: dev-loop.game-build.result total_seconds=63.109, compile_seconds=24.703, link_seconds=11.344, worker_total_seconds=38.25; generation ready at 04:11:48.553Z with duration_seconds=64.703.
- Wait recorded exactly once at 04:11:47.730Z: dev-loop.wait.over_budget elapsed=60.384s vs budget_seconds=60.0 (0.384s / 0.6% over); one reviewer wake queued (dev-loop.wait.reviewer-wake, adapter=codex).
- Mechanism: check_wait_budget (devscripts/dev-loop.py:3839) with WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:114); record_wait_incident + wake_reviewer only on created (dev-loop.py:3907-3920). Keyed by wait_id, so each occurrence creates its own issue+wake by design.
- Time was normal work, not a stall: manifest_snapshot 10.515s, source_snapshot 0.985s, packet_registry codegen 2.25s, src/gamemode/arena-ball.cpp compile 22.437s, module-identity-provider compile 2.166s.
- The wait reason is NOT human-readable: evidence.reason is `":51078.0,"build_attempt_id":"game-dll-20261126T041038797106Z-596df3",...` i.e. the last 240 chars of the [GAME_FILE] worker line. Source: _record_game_build_output sets game_build_detail = line.rstrip()[-240:] (dev-loop.py:3112, 3142); check_wait_budget uses game_build_detail as reason (dev-loop.py:3892).

OBSERVED (aggregate, same logs):
- 9 game-dll-build over-budget waits today (all game_build): 60.182, 60.215, 60.384, 60.464, 60.564, 60.738, 61.2, 61.733, 62.51s -> issues 75e49f67b48c, 8b46d995a7a0, be98253c272f(this), 5137be4d2f57, ac23ab33d184, fa0da68ca2b3, 55479477f3a7, 347500dfb032, 4a0450674f5f. One of these (gen683) had a 137s build but was snapshot at 60.182s.
- Recent succeeded builds split across the line: gen676 24.06s, gen680 24.17s, gen682 21.97s (under 60s) vs gen684 63.11s, gen685 84.09s, gen683 136.98s (over).
- 210 same-title "Wait over budget: game-dll-build at game_build" build-friction issues exist (191 in_review, 18 new). The issue's own stated risk, "A noisy wait policy could create duplicate friction issues," has materialized.

INFERENCE:
- A flat 60s budget sits inside the normal distribution of single-file incremental game-DLL builds (codegen ~2-10s + ~20-25s TU compile + ~5-11s link + ~10s manifest snapshot), so it fires on ordinary work far more often than on genuine stalls.
- The reason fragment makes triage hard: the acceptance "carries wait_id, stage, reason..." is met only literally, not semantically.

UNKNOWN:
- Whether intended policy is a single fixed 60s for all wait kinds or per-kind/per-stage budgets (no spec located in this review).
- Whether duplicate issue count vs wake count should be capped by a rolling window.

RECOMMENDED SMALLEST NEXT CHECKS (no code changed in this review):
1. Decide budget policy for game-dll-build using measured durations (current p95 clearly > 60s): per-kind/stage budget, or rolling-window suppression of repeat over-budget notices. Owner: dev-loop/build-friction owner; needs human decision on shared authority.
2. Make the wait reason a readable structured string (build attempt id, stage, current phase/changed file) instead of the raw line tail (dev-loop.py:3112/3142/3892).
3. Consolidate the 210 same-title issues into one live tracker linked to issue-7d3dcfd7433b ("Measure every dev-loop wait stage and reduce avoidable latency") and issue-e044a931c54b ("Waiting is the enemy").

CONFIDENCE: high for observed timings/events; medium for the budget-policy inference.
