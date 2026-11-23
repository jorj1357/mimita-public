# issue-cd97259fdc20: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-false-positive-wait-policy)

### 2026-11-23T05:56:49.942Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait game-dll-build:2026-11-22T23:52:22.109Z (stage game_build, elapsed 61.088s vs 60.0s budget). Verdict: expected budget trigger, not a build stall. The in-flight game-DLL attempt compiled, linked, and published successfully. The 60s crossing is caused by a single heavy translation unit, so this is real (known) build friction surfaced by a flat wait policy, not a defect in this occurrence.

EVIDENCE (this exact occurrence; run 20261122_231306)
- logs/2026-11-22/20261122_231306/dev-loop/events.jsonl line 155: dev-loop.wait.over_budget, wall_time 2026-11-22T23:53:35.729Z, fields.wait.wait_id = game-dll-build:2026-11-22T23:52:22.109Z, created=true, issue=issue-cd97259fdc20, elapsed_seconds=61.088, budget_seconds=60.0, request_ids=["auto-27"].
- Same file line 154: dev-loop.wait.reviewer-wake, wall_time 2026-11-22T23:53:35.728Z, wait_id matches, issue matches, wake.queued=true, adapter=codex.
- In-flight attempt is game-dll-20261122T235222465818Z-9bfbf7 (visible only in the free-text reason). .dev/build-timings/game-dll-20261122T235222465818Z-9bfbf7.jsonl: build.started 23:52:22.465Z; build.completed 23:56:49.678Z; exit_code=0; generation 546; elapsed 267.197s.
- .dev/build-timings/.../current.json for the same attempt: compiled 2, skipped 641, compile_seconds 189.74, link_seconds 59.656, total_seconds 264.172, status ok.
- Stage breakdown from the same .jsonl: manifest_snapshot 1.80s, manifest_validation 0.20s, source_snapshot 1.31s, codegen ~2.5s, changed_source_preflight 151.157s (the single changed TU src/network/server-gamemode.cpp, translation_unit.started 23:52:34.587Z -> completed 23:55:05.744Z), object_cache_scan 1.60s, compile of module-identity-provider.cpp 38.576s, link 59.71s, publish 0.49s.
- At the 60s crossing (~23:53:22Z) server-gamemode.cpp was ~48s into a 151s compile and progressing; the attempt had already passed source snapshot and codegen. No stall, no failure, no retry of the build.

MECHANISM / ROOT CAUSE
- devscripts/dev-loop.py defines one flat WAIT_ISSUE_SECONDS=60 for every kind/stage (dev-loop.py:109-110) and check_wait_budget() (dev-loop.py:3486-3577) reports the coarse stage "game_build" with no sub-stage and no progress test.
- Any game-DLL build that touches a heavy TU necessarily exceeds 60s on this host: server-gamemode.cpp alone is ~151s, module-identity-provider.cpp ~38s, and link ~60s. The trigger is therefore expected for this class of change, not evidence of a problem in this wait.
- The systemic latency target is owned by issue-136d4553ff92 (edit-to-live <=10s); the wait-policy design is tracked under parent issue-e044a931c54b.

SECONDARY DATA DEFECTS (same occurrence)
- evidence.build_id=2026-11-22T21:32:48.195707Z is the last published EXE artifact, not the in-flight game-DLL attempt; the real attempt id only appears inside the free-text reason.
- evidence.generation=545 but the attempt produced generation 546 (generation is assigned at completion, so the wait payload lags).
- reason is the tail of a raw progress line (_record_game_build_output), unstructured, and stage collapses to game_build, so the payload does not name the expensive phase.

DUPLICATION / POLICY NOTE
- docs/issues/build-friction currently holds 130 "*-wait-over-budget-game-dll-build-at-game-build-*" issues; 83 were created on 2026-11-22 alone. Each distinct wait_id correctly gets its own issue (no duplicate for this wait_id; occurrences=[]), but the recurring condition is one systemic problem. Do not open a new fix issue. Recommend closing/merging this occurrence into parent issue-e044a931c54b and tracking the latency work under issue-136d4553ff92. A per-stage/progress-aware budget or a single recurring friction issue would cut the noise; that is a policy change and needs a human decision, so no code was edited.

WAKE CHECK (acceptance)
- Exactly one wake.queued for this wait occurrence: wake-44675b502200, .dev/agent-wake/events.jsonl line 2027, wall_time 2026-11-22T23:53:25.322Z, agent build-wait-reviewer, issue-cd97259fdc20.
- The later wake.retry / wake.claimed entries (lines 2050, 2067) are stale-launch recovery of the SAME wake, not a second queued wake. Only one reviewer-wake event exists for this wait_id (1 of the 2 journal matches; the other is the over_budget record).
- Issue readiness: ready=true, missing=[]. The issue carries wait_id, stage, reason, elapsed, budget, request_ids, build_id, and evidence. Acceptance "a reviewer wake is queued exactly once for this wait occurrence" is satisfied.

PROOF / SCOPE
- This validates the over-budget wait path (journal events + issue record + single reviewer wake). It does not claim the systemic latency or any behavior fix.
- No source edited; no file lease claimed.
