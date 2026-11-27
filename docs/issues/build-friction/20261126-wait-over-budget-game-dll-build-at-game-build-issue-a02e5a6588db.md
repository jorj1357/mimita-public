# issue-a02e5a6588db: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T17:47:29.008Z — build-wait-reviewer

related_build: game-dll-20261126T172047133775Z-30f81f
related_run: 20261126_170323

## Review (confirmed-legitimate-wait-not-stall; single-TU-exceeds-budget; identity-defect-recurred; wake-queued-once-dispatch-late)

### 2026-11-26T17:47:49.380Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-35c5c0492a2a), read-only. No source edit, build request, runtime, or close.

OBSERVED FACTS (pinned)
- Wait: wait_id game-dll-build:2026-11-26T17:20:46.773Z, kind game-dll-build, stage game_build, elapsed 61.28s vs budget 60.0s, source_generation 12, request_ids [2026-11-26T17:20:46.345Z-9c69e020:mode3:game]. Journal logs/2026-11-26/20261126_170323/dev-loop/events.jsonl line 124 (dev-loop.wait.over_budget, created=true, wall 17:21:55.483Z) and line 123 (dev-loop.wait.reviewer-wake, wall 17:21:55.414Z).
- In-flight attempt: game-dll-20261126T172047133775Z-30f81f, generation 704, revision da4661b33596a9d045d0b090c65b34425070b2f207e5f36927dcbb83128b70cf, run 20261126_170323. .dev/build-timings/...30f81f.json: success, started 17:20:47.133Z, completed 17:22:41.672Z, total 114.547s, worker_total 89.828s. Phases: changed_source_preflight 73.328s, compile 2.297s, link 11.578s, coherence_validation 0.547s, publish 0.531s; 644 sources, 642 skipped, 3 compiled, jobs=10.
- Real work, not a stall. Two directly changed TUs compiled in the preflight wave: src/engine/engine-tick-ui.cpp 73.084s and src/render/postfx-commands.cpp 46.342s (plus module-identity-provider 2.149s). One TU alone (engine-tick-ui.cpp, 73.084s) exceeded the 60s budget, then an 11.578s link followed. The build finished ~46s after the wait was flagged.
- Resource sidecar 30f81f.resource.json: 81 samples @1s over 114.484s, mean host CPU 98.271%, peak 100.0%, minimum available memory 1,920,593,920 B (~1.79 GiB); same run hosted server vehicle_freeplay (pid 33008) + client pid 34624. Unavailable fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation (already recorded as gap_key build-resource-sampling:...30f81f on issue-2a5c5da53565). No pressure inference.
- Wake exactly-once: wake-35c5c0492a2a queued once at 17:21:51.344Z (.dev/agent-wake/events.jsonl line 3575); codex claimed/launched 17:21:55.154/17:21:55.294Z pid 3516 then exited; wake.retry -> opencode 17:35:27.028Z; opencode claimed/launched 17:42:01.823/17:42:01.859Z pid 33192. Queue-to-effective-launch ~1210.5s (~20m10s), attempts=2, one queued wake, issue created once.

ACCEPTANCE REVIEW
- wait_id, stage, elapsed, source_generation, request_ids: present and correct. reason is clean ("[GAME BUILD] revision=... sources=644 jobs=10 priority=high cache=... seeded=0"); the reason-truncation defect (issue-6e08d89b55e3) does NOT apply to this occurrence.
- build/run IDs: DEFECT (known issue-7d3dcfd7433b). The record stores build_id 2026-11-26T16:59:26.767416Z (last published EXE artifact) and generation 703 (last published game generation), not the in-flight attempt 30f81f / generation 704; no run_id. The true attempt is recoverable only from adjacent dev-loop.game-build.started lines and the reason revision. This session set related_build/related_run to the true attempt and run.
- Reviewer wake queued exactly once: MET; delivery late (known issue-84c584957e65).

VERDICT: confirmed-legitimate-wait-not-stall; single-TU-exceeds-budget; identity-defect-recurred; wake-queued-once-dispatch-late.

RECURRENCE (not duplicates): distinct wait_ids, so one issue each is correct per the wait_id dedupe key; all compile-bound, all published successfully: issue-404bd5b44b6e (16:49:17.979, 60.023s), issue-4fb7a24c54b0 (16:56:24.029, 60.924s), issue-45733b82bbbc (17:07:53.638, 60.263s), issue-24863e1f595e (17:13:10.456, 60.731s), this issue (17:20:46.773, 61.28s). Further same-titled claimable wait issues exist (ca900e60ed4b, 4c12dde1d7ba, ad873eff97f3, dc6792df3815, 194c538d7cbc). The flat 60s budget is shorter than a normal heavy-TU game-DLL build here, so a healthy successful build mints a new friction issue each time.

OWNERS / NEXT DISCRIMINATING CHECK
- Budget/dedupe policy: issue-9a4503f6ed68, issue-e044a931c54b, issue-136d4553ff92. Confirm a per-class or attempt-aware budget / consolidation, and decide whether the >60s single-TU compile cost of src/engine/engine-tick-ui.cpp is a header-hygiene target.
- Wait-record identity: issue-7d3dcfd7433b (record the active build_attempt_id/generation/run in the wait record).
- Wake delivery: issue-84c584957e65 (drain the fallback without waiting for an unrelated trigger).
- Resource gap for 30f81f is already recorded on issue-2a5c5da53565; not re-reported here.

UNKNOWNS
- Exact split between per-TU compile cost and host CPU saturation (compile ran at mean 98.3% host CPU with a live server/client pair); cannot be isolated without a matched prebuild baseline.
- No runtime/behavior proof attempted; this is build-process evidence only.
