# issue-ba7a122bc012: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-24T08:18:18.664Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-c8f9dc0326a9); read-only: no source edit, no build/test requested, no fix or close claimed. Review window 2026-11-24T07:50-08:05Z.

VERDICT: occurrence confirmed; the individual wait record meets acceptance; the recurring game-dll-build over-budget pattern is systemic and duplicate-heavy.

OBSERVED FACTS
- wait_id game-dll-build:2026-11-24T07:25:33.866Z, stage game_build, elapsed_s 60.407 vs budget 60.0, request_id 2026-11-24T07:25:33.567Z-211addd2:mode3:game, started_utc 2026-11-24T07:25:33.866Z (this issue's evidence).
- Exact attempt: .dev/build-timings/game-dll-20261124T072533876573Z-7080a4.json (+.jsonl). Request 07:25:33.876Z -> completed 07:34:11Z, success=true, generation 585, revision b0a8cd352f1ebddff70de6e7454a79e3950ef8567626d11ada0a5afacf731175, total_seconds 517.609, worker_total_seconds 496.109.
- Dominant phases: changed_source_preflight 41.953s; compile 420.891s (compile_sum 4,149,326 ms ~= 69.2 CPU-min over 151 compiled units; max single TU map-editor.cpp 64.943s); link 22.875s (668 args, 644 inputs, 491,204,859 object bytes); object_cache_scan 7.25s; publish 0.594s. 494 of 644 sources skipped.
- Changed inputs: src/camera.cpp, src/engine/engine-tick-camera.cpp, src/network/community-match-client.cpp, src/camera.h. The camera.h header drives the 151 recompiles (fan-out), which dominates total time.
- The 60.4s sample is the first over-budget sample only. The build actually ran 517.6s (~8.6x budget) and succeeded. This is build evidence, not runtime behavior proof.
- Acceptance: issue carries wait_id, stage, reason, elapsed, build_id, request_ids, evidence; exactly one reviewer wake was queued (wake-c8f9dc0326a9; .dev/agent-wake/events.jsonl wake.queued wall_time 2026-11-24T07:26:37.343Z). Acceptance met.

RECURRENCE (same wait kind)
- DB count: 154 issues titled "Wait over budget: game-dll-build at game_build" (135 in_review, 18 new, 1 assigned). The issue's own risk ("a noisy wait policy could create duplicate friction issues") is realized at scale.
- The next game-DLL attempt after this one already logged another over-budget wait: generation 587 started 2026-11-24T07:53:14.620Z, elapsed 62.252s -> issue-816c05804889 (journal logs/2026-11-24/20261124_075114/dev-loop/events.jsonl, event dev-loop.wait.over_budget). No duplicate issue created here.

EVIDENCE GAPS (non-blocking)
- evidence.reason is a truncated JSON build-progress fragment ("nd scan\",\"raw\":\"tree operand scan ..."), not a human-readable stage reason; same pattern on prior wait issues (e.g. issue-6413477d361d). The authoritative attempt timing JSON is intact.
- evidence.build_id (2026-11-24T07:18:06.640256Z) is the last completed EXE build id (dev-loop.py:3896 self.latest_build), not the game-DLL attempt; the DLL attempt is identified by wait_id/started_utc. Matches existing convention (related_build empty on comparable wait issues), so not a defect, but a reader must join wait_id -> build_attempt_id.

INFERENCE
- For this sample the wait is a symptom of compile cost after a high-fan-out header (camera.h), not unknown pre-worker stages; compile was ~81% of total. Confidence high on phase numbers, medium that camera.h fan-out is the sole cause (changed-path list is authoritative, per-TU invalidation reasons are not recorded here).
- Coalescing recurring game-dll-build waits into one canonical issue with occurrences (or raising that budget) would cut the duplicate flood; that is a policy decision for the owners, not this reviewer.

CROSS-LINKS: root/observability issue-136d4553ff92 (build-coordinator); waits-removal issue-e044a931c54b (build-runtime-subagents); build-friction intake issue-67f2be3d7012 (coordination-v2).

NEXT STEP: owned follow-up, not this reviewer. If a code fix is wanted, split into (a) wait-policy coalescing and (b) build latency; both already have owners. This reviewer requests no build and changes no tasks.

Confidence: high that the wait is real and the build succeeded at 517.6s; high that acceptance is met; medium on root-cause attribution.
