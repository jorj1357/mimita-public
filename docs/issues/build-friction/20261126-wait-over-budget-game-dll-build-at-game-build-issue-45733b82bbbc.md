# issue-45733b82bbbc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-true-positive)

### 2026-11-26T17:28:23.154Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (activation wake-359d9fad0264). Read-only review: no source edits, no build, no runtime claim.

VERDICT: Confirmed true positive. The 60s wait budget was exceeded by a real whole-game DLL compile; the attempt then completed successfully and published generation 702. The wait record and the single reviewer wake are correct. No duplicate issue was created.

WAIT UNDER REVIEW
- wait_id: game-dll-build:2026-11-26T17:07:53.638Z
- kind/stage: game-dll-build / game_build
- elapsed at record: 60.263s (budget 60.0s)
- request: 2026-11-26T17:07:53.410Z-5492d681:mode3:game (Mode 3 batch, gen 7, requested_by opencode-deepseek-v4-1)
- dev-loop run: 20261126_170323

TIMELINE (UTC)
- 17:07:53.410 batch sealed; 17:07:53.638 game build start; 17:07:53.868 attempt started
- 17:08:54.002 issue created (elapsed 60.263s)
- 17:09:06.828 dev-loop.wait.over_budget (created=true)
- 17:09:37.172 attempt completed success; generation 702 published
Total build wall time 103.313s: a true over-budget build, not a stalled worker.

ACTUAL BUILD IDENTITY vs RECORDED (identity gap)
- Actual in-flight attempt: game-dll-20261126T170753867026Z-7428d5, generation 702, source_generation 7 (dev-loop.game-generation.ready @17:09:37.607Z).
- Recorded in the wait evidence: build_id 2026-11-26T16:59:26.767416Z (stale latest-successful EXE build) and generation null.
- Root: dev-loop.py check_wait_budget game-DLL arm (~3896-3899) uses self.latest_build and self.game_build_generation, so while the attempt is in flight the recorded build_id is the previous success and generation is null. Tracked by issue-7d3dcfd7433b. The reason field is a raw truncated compiler-progress JSON tail (dev-loop.py ~3892); tracked by issue-6e08d89b55e3.

WHY IT WAS SLOW (direct evidence from .dev/build-timings/game-dll-20261126T170753867026Z-7428d5.json)
- Changed header src/render/post-fx.h plus 4 sources; header fanout invalidated 21 TUs (cache_invalidations), 624 skipped, 644 sources, jobs=10.
- Phase compile 79.359s; compile_max 44.446s. Slowest TUs: main-systems.cpp 44.446s, engine-tick-render.cpp 41.574s, engine-tick.cpp 39.746s, engine-tick-ui.cpp 38.839s, engine-tick-setup.cpp 38.300s, replay-export-json.cpp 35.865s. Link 7.078s.
- Resource sample: mean host CPU 96.412% (peak 100), minimum available memory 68,337,664 bytes (~65 MiB).
- Interpretation: one header edit produced a ~79s compile wave, so a 60s budget is exceeded by construction for this workload. This is expected behavior of the current budget, not a mechanism failure. Exact per-TU vs host-saturation attribution is not isolated (see gap below).

ACCEPTANCE CHECK
- wait_id: pass; stage: pass; elapsed: pass (60.263); evidence: pass.
- build/run IDs: partial. Request_id and dev-loop run are present, but recorded build_id is stale and generation is null (issue-7d3dcfd7433b).
- reason readable: fail. Raw JSON tail, not a stage summary (issue-6e08d89b55e3).
- reviewer wake queued exactly once: pass. One wake.queued for wake-359d9fad0264 at 17:08:56.669Z; codex claimed/launched 17:09:06, exited; fallback opencode claimed/launched 17:21:55 (wake.retry). One queue entry, retry only.

RECURRENCE
Four over-budget game-DLL waits in ~30 min, all compile-bound: issue-404bd5b44b6e (16:49:17.979, 60.023s), this issue (17:07:53.638, 60.263s), issue-24863e1f595e (17:13:10.456, 60.731s), issue-a02e5a6588db (17:20:46.773, 61.280s). Distinct wait_ids, so one issue each is correct (not duplicates). Pattern is budget-vs-workload, tracked by umbrella issue-136d4553ff92 (whole-DLL stage accounting) and issue-9a4503f6ed68 (wait policy/proof).

NEXT STEP / OWNER
- No code fix performed (review scope; no human approval to edit). Smallest next checks, ordered: (1) decide whether the 60s budget should differ for header-fanout builds or derive from changed-TU count; (2) reduce post-fx.h fanout if header hygiene permits; (3) fix wait identity so the recorded build_id/generation name the in-flight attempt. Owners: issue-136d4553ff92 (build-coordinator) for stage/latency/budget, issue-7d3dcfd7433b for wait identity, issue-6e08d89b55e3 for reason text.

UNKNOWNS
- Exact split between per-TU compile cost and host CPU/memory saturation (compile_sum 625.238s over a 79.359s wall with 10 jobs implies heavy parallelism).
- Whether client-side host contention contributed cannot be isolated without a matched prebuild baseline (reported as a resource gap on issue-136d4553ff92).
