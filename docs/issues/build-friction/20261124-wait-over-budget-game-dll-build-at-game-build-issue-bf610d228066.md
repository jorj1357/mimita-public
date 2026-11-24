# issue-bf610d228066: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (pass)

### 2026-11-24T04:14:11.953Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer build-wait-reviewer, independent bounded review of this occurrence. Read-only: no source edits, no build request, no file claims, no fix or close claimed.

OBSERVED (this occurrence)
- Dev-loop journal logs/2026-11-24/20261124_022440_01/dev-loop/events.jsonl:
  - dev-loop.wait.over_budget wall_time 2026-11-24T03:35:41.817Z: wait_id game-dll-build:2026-11-24T03:34:39.674Z, kind game-dll-build, stage game_build, elapsed 60.185s vs budget 60.0s, source_generation 21, build_id 2026-11-24T03:30:52.372066Z, request_ids [2026-11-24T03:34:39.332Z-773be157:mode3:game], issue issue-bf610d228066, created true.
  - dev-loop.wait.reviewer-wake wall_time 2026-11-24T03:35:41.816Z: same wait_id and issue, wake queued true, adapter codex.
- .dev/agent-wake/events.jsonl: exactly one wake.queued for this issue, wake-1ad2bc01995e at 2026-11-24T03:35:40.306Z. Search of .dev found no second wake for this wait_id.
- Dedupe: build_friction._wait_issue keys on evidence.wait_id; one issue row exists for this wait_id. Sibling wait_ids (e.g. game-dll-build:2026-11-24T03:30:57.098Z -> issue-456061c38a8c) are distinct occurrences, not duplicates.
- Attempt correlation via request_ids (needed because evidence.build_id is a revision id): .dev/build-timings/game-dll-20261124T033440193460Z-3a5393.json request_ids contains 2026-11-24T03:34:39.332Z-773be157:mode3:game; generation 579, success true, total_seconds 534.174, compile_seconds 506.113, compiled 158, skipped 485, started 2026-11-24T03:34:40.193Z, completed 2026-11-24T03:43:34.367Z.
- Cause: legitimate long recompile, not a stuck wait. cache_invalidations cite newer header dependencies (src/gamemode/gamemode.h, src/gui/ui-system.h, src/render/post-fx.h, src/entities/aimbody-config.h); the only direct edit was src/ragdoll/ragdoll-mode.cpp. Header fanout plus a 158-TU compile drove 534s.

ACCEPTANCE ASSESSMENT
- Met for this occurrence: unique issue, exactly one reviewer wake, and the dev-loop.wait.over_budget record all exist; the issue carries wait_id, stage, elapsed, budget, request_ids, source_generation, and evidence.
- Not fully met: 'build/run IDs' is imprecise here. evidence.build_id is the last published EXE/revision id (2026-11-24T03:30:52.372066Z), generation is null, there is no run_id, and the true game-DLL attempt id (game-dll-20261124T033440193460Z-3a5393) is reachable only via request_ids -> build-timings json. The same attribution defect is recorded in issue-f66e6e82656b review 2026-11-24T03:45:19Z; owner dev-loop.py / hot-reload-latency-root.
- evidence.reason is a truncated -ftime-report tail, not a sub-stage; consistent with that same defect.

CONCLUSION
- No defect in the wait/wake path for this occurrence; do not reopen the mechanism. The attribution/policy fix is owned elsewhere (issue-f66e6e82656b; latency owner issue-136d4553ff92). I did not edit code, request a build, or close anything.
- Next discriminating check (owner/human): on the next >60s game-DLL build, assert the emitted record's structured generation/build_attempt_id (or sub-stage reason) matches the attempt in .dev/build-timings, and measure completion-time distribution before any budget change.
