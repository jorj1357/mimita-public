# issue-18050cc9b42e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-false-positive; duplicate-systemic)

### 2026-11-22T08:32:32.136Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (read-only; no source edited, no build/run started). Verdict: confirmed false-positive wait against a healthy build; duplicate occurrence of the systemic flat-budget defect.

Verified occurrence:
- wait_id game-dll-build:2026-11-22T08:03:41.642Z, kind game-dll-build, stage game_build, elapsed 60.671s vs budget 60.0s.
- Actual attempt game-dll-20261122T080341760815Z-26fbbf (generation 477), started 2026-11-22T08:03:41.761Z, completed 2026-11-22T08:05:48.006Z, success=true, total_seconds=126.231 (.dev/build-timings/game-dll-20261122T080341760815Z-26fbbf.json).
- Phase durations: changed_source_preflight 48.184s, npc.cpp compile 47.884s, link 26.198s, publish 0.571s. The 60s crossing happens during healthy preflight/compile, so the wait flagged progress, not a stall.

Defects still present in this occurrence (already tracked, not new):
1) Flat 60s budget: WAIT_ISSUE_SECONDS (devscripts/dev-loop.py:109-110, applied at 3490/3509/3526) is below a normal incremental game-DLL build. changed_source_preflight alone (48.184s) nearly exhausts it, and the build succeeded in 126.231s. This is one of ~98 open build-friction wait issues on the board.
2) Wrong build identity: this issue recorded generation=476 and build_id=2026-11-22T08:00:31.376619Z (the previously published EXE build) instead of the in-flight attempt generation 477 / game-dll-20261122T080341760815Z-26fbbf, because dev-loop.py:3532-3533 use self.latest_build and self.game_build_generation (set only on completion at dev-loop.py:2967).
3) Journal gap: .dev/state.json events_path (logs/2026-11-22/20261122_011031/dev-loop/events.jsonl) does not exist, so no dev-loop.wait.over_budget / reviewer-wake event can be inspected. Already tracked in issue-0498cbef4ef3.

Canonical tracker: issue-9a4503f6ed68 (with issue-7d3dcfd7433b). This occurrence adds evidence; the fix is systemic and not owned here. Do not close as fixed on the strength of this build. Recommended: stage/workload-aware budget (derive from p95 build duration, e.g. >=180s) and stall-based detection, dedup by kind+stage with occurrence counts, and thread the active game-build attempt id into wait evidence.

Next smallest check: after a budget/identity fix lands, re-run one npc.cpp game-DLL build and confirm the wait does not fire on a healthy 100-130s attempt and that any wait evidence names the in-flight attempt id/generation.
