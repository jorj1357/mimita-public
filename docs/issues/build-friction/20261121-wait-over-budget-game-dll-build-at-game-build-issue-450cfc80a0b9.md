# issue-450cfc80a0b9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-needs-policy-fix)

### 2026-11-21T15:41:09.947Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-21T03:46:54.696Z (build_attempt game-dll-20261121T034654937752Z-3c82d2, generation 417).

Evidence
- Issue evidence record: budget_seconds=60.0, elapsed_seconds=61.776, stage=game_build, source_generation=5, request_ids=[auto-5].
- .dev/build-timings/game-dll-20261121T034654937752Z-3c82d2.json: started 03:46:54.938Z, completed 04:25:24.660Z, total_seconds=2309.492, success=false, failed_phase=changed_source_preflight, worker_error="changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1488.004s". Phase timings: object_scan 337.838s, changed_source_preflight 342.853s, compile 414.893s, link 100.34s, publish 69.91s.
- devscripts/dev-loop.py:103-106 sets one global WAIT_ISSUE_SECONDS=60 for every stage. devscripts/dev-loop.py:3433-3449 flags game-dll-build once total elapsed in stage game_build >= that budget; no progress/stall test.
- At issuance (03:47:56Z, 61.776s) the build reason was [GAME_STAGE] codegen:packet_registry_codegen.py started (elapsed_ms 6535), i.e. the build was actively progressing.

Finding (first divergence)
The wait policy, not the build, is the fault. A 60s total-stage budget is applied to game_build, a stage that legitimately runs 10-40 minutes (this attempt: 2309.492s). Every real game-DLL build therefore trips it, producing 36+ open "Wait over budget: game-dll-build at game_build" issues (issue-450cfc80a0b9, issue-ad873eff97f3, issue-6415397775a2, issue-f9ea4f30ad18, issue-dad513d47108, issue-121f867b430a, issue-9215d710021d, issue-e96486c8cc3e, issue-f995213a15b0, plus older in_review duplicates). This occurrence is a false positive: at 61.776s the build was 6.5s into packet registry codegen, not stalled. The issue also duplicates the intent of parent issue-e044a931c54b (assigned build-runtime-subagents) and measuring issue-7d3dcfd7433b (triaged); creating another identical wait issue is noise.

Real friction in the same attempt (separate owner, already re-resolved)
- The attempt failed compile at src/network/multiplayer-projectiles.cpp:2419 (std::nullptr_t to float at spawnExplosionFx arg 7), then blocked the queue for ~38 min. That source is now fixed at line 2419 (1.0f arg) and generation 420 builds successfully (current.json total_seconds=600.49, status=ok). No open action for this location.
- Remaining, separate, still-relevant slow phases for the wait parent: object_scan 337.8s, changed_source_preflight 342.9s (worker reports 1488s), compile 414.9s, link 100.3s.

Missing fields
None. issue readiness is ready=true, missing=[], score=8/8; reproduction, expected behavior, acceptance, paths, proof expectation, and evidence are all present.

Recommendation for the owner (report only; reviewer made no code edit)
1. Replace the single 60s total-stage budget in dev-loop.py:check_wait_budget() with stage-aware budgets AND progress detection (flag only when the stage detail/timeline has not advanced for N seconds), so an actively progressing long build does not page a reviewer.
2. Deduplicate by (kind, stage, active build attempt/generation) instead of one issue per wait start timestamp, and append occurrences under the existing parent rather than spawning new issues.
3. Route the phase-timing findings to issue-e044a931c54b / issue-7d3dcfd7433b; do not leave 36 parallel identical wait issues open.

Verdict: reviewed / root cause is wait-policy false positive; requires a dev-loop wait-budget fix owned separately (not this reviewer). No source or leased file edited.
