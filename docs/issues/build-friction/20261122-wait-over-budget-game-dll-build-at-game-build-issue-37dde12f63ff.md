# issue-37dde12f63ff: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-policy-noise)

### 2026-11-22T08:00:48.752Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Read-only review of wait game-dll-build:2026-11-22T07:27:50.398Z.

Acceptance PASS: the issue carries wait_id, stage=game_build, reason, elapsed=60.04s vs budget 60.0s, request_ids, and evidence. Wake wake-cc77e9236913 was queued exactly once (created 2026-11-22T07:28:52.057Z, launched 07:45:44.464Z).

Root cause (established, not a new fault): the flat 60s wait budget is below normal game-DLL build duration. The in-flight attempt game-dll-20261122T072750493926Z-7ac0b3 (generation 472, changed src/npc/npc.cpp) succeeded in 191.924s, completed 07:31:02.431Z. Phases: changed_source_preflight 85.144s (npc.cpp compile 84.847s), link 66.853s (gnu_linker 66.792s), object-cache scan 6.103s, publish 0.758s. The compile preflight AND the link each exceed 60s alone, so the wait was normal build progress, not a stall.

Identity defect persists: evidence records generation 471 and build_id 2026-11-22T02:37:06.972018Z (the prior published artifact), while the actual in-flight attempt was generation 472. devscripts/dev-loop.py:2967 sets game_build_generation only on completion, so check_wait_budget at line 3533 reads the stale previous value.

Journal gap persists: .dev/state.json events_path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl does not exist, so there is no on-disk dev-loop.wait.over_budget record for this wait_id.

Policy noise: 114 open build-friction issues, 87 of them game-dll-build at game_build. build_friction.py:131-169 keys uniqueness on wait_id, so each occurrence mints a new issue and wake. This is one more instance of the same signature.

Owner pointers: budget/aggregation issue-9a4503f6ed68, stage accounting issue-136d4553ff92, missing dev-loop journal issue-0498cbef4ef3. No code fix claimed (review-only).
