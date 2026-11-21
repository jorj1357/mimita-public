# issue-ca900e60ed4b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-duplicate-wait-policy)

### 2026-11-21T16:19:47.614Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-21T15:52:23.943Z (build_attempt game-dll-20261121T155224262249Z-d0908c, generation 420, source_generation 1, request auto-1).

Evidence
- Issue evidence: budget_seconds=60.0, elapsed_seconds=60.05, stage=game_build, started_utc=2026-11-21T15:52:23.943Z.
- .dev/build-timings/game-dll-20261121T155224262249Z-d0908c.json: started 15:52:24.289Z, total_seconds=122.986, success=false, failed_phase=changed_source_preflight.
- Timeline .jsonl: at the 60s budget the build was inside manifest_snapshot (15:52:33.9 -> 15:53:05.3, 31.3s) transitioning to source_snapshot (15:53:05.5 -> 15:53:33.4, 27.9s). The reason field's '[GAME_STAGE] source_snapshot started ... elapsed_ms 41194' is active progression, not a stall. Pre-compile fixed overhead (codegen ~9s + manifest_snapshot 31.3s + source_snapshot 27.9s) already exceeds the 60s budget before a single TU compiles.
- Build then failed at 122.99s on a real (now-removed) source error: src/actor/player-ai.cpp:1010 `'maybeRequestFallback' was not declared in this scope`. Current source has no `maybeRequestFallback` references (rg across src/, zero matches), so that compile failure is stale/transient and not the wait cause.
- Wake: .dev/agent-wake/requests/wake-e96c02d2d614.json created 2026-11-21T15:53:24.986Z, issue=issue-ca900e60ed4b, status=launched. Exactly one reviewer wake for this wait_id; dedupe by wait_id works.

Finding (first divergence)
The wait policy is the fault, not the build. devscripts/dev-loop.py:108-109 sets one global WAIT_ISSUE_SECONDS=60; check_wait_budget() at devscripts/dev-loop.py:3391-3407 flags game-dll-build only when total elapsed since game_build_started_utc >= that budget, with no per-stage budget and no progress/stall test. Every non-trivial game-DLL build whose pre-compile stages exceed 60s trips it. This is the same root cause already documented on issue-450cfc80a0b9 (verdict reviewed-needs-policy-fix) and belongs to parent issue-e044a931c54b (owner build-runtime-subagents) plus measuring issue-7d3dcfd7433b. This occurrence is a duplicate false positive; do not open a new fix here.

Field-fidelity gap
The evidence bundle's build_id is the latest EXE build '2026-11-21T02:26:50.306117Z' and generation=null, while the actual DLL attempt id (game-dll-...-d0908c, generation 420) appears only inside the free-text reason. Acceptance requires build/run IDs, so recommend adding a structured build_attempt_id (and the DLL generation) to the wait payload at dev-loop.py:3391-3407.

Recommendation for owner (report only; reviewer edited nothing)
1. Replace the flat 60s total-stage budget with stage-aware budgets AND progress detection (flag only when the stage detail/timeline has not advanced for N seconds).
2. Deduplicate by (kind, stage, active build attempt/generation) and append occurrences to the parent instead of one issue per wait-start timestamp.
3. Include structured build_attempt_id/generation in the wait evidence.

Verdict: reviewed-duplicate-wait-policy. Valid occurrence, false-positive wait (build actively progressing), same root cause as issue-450cfc80a0b9; no new code change required for this issue.
