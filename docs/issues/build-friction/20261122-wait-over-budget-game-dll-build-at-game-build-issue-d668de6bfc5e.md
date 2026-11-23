# issue-d668de6bfc5e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T19:11:48.475Z — build-wait-reviewer

related_build: game-dll-20261122T180535335873Z-773b0b
related_run: 20261122_174002

## Review (reviewed-with-findings)

### 2026-11-22T19:17:51.139Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of wait occurrence game-dll-build:2026-11-22T18:05:34.493Z (elapsed 60.302s, budget 60.0s).

VERDICT: Acceptance criteria are met for this occurrence. The wait was productive compile work, not a stall. The record has one attribution defect, and the recurring over-budget policy is now producing duplicate issues at high volume.

EVIDENCE (direct observation)
1. Journal logs/2026-11-22/20261122_174002/dev-loop/events.jsonl: dev-loop.wait.over_budget at wall_time 2026-11-22T18:06:37.952Z for issue-d668de6bfc5e, wait_id game-dll-build:2026-11-22T18:05:34.493Z, source_generation=7, request_ids=["auto-7"], reason revision=007813969191c69908bf8c8bcc054a067e50e7fb90607ab3524d7ebf91ff6e6e.
2. Actual game-DLL attempt was game-dll-20261122T180535335873Z-773b0b (.dev/build-timings/game-dll-20261122T180535335873Z-773b0b.json): started 2026-11-22T18:05:35.335Z, completed 2026-11-22T18:08:52.214Z, generation 508, success=true, total_seconds=196.865. Changed path src/network/server-npcs.cpp (build note 17d49b9187b4, agent horde-behavior-evidence) compiled in 100.531s; changed_source_preflight 100.608s, link.lld_linker 58.423s. Build was actively progressing and succeeded.
3. Reviewer wake was queued once for this wait_id: .dev/agent-wake/events.jsonl wake-fe718d06a423 wake.queued 2026-11-22T18:06:35.961Z; codex wake.claimed 18:06:37.632Z / wake.launched 18:06:37.819Z (pid 39476). The codex adapter exited; wake.retry 18:25:25.086Z, then opencode wake.claimed 18:35:56.541Z / wake.launched 18:35:56.591Z (pid 35352), which is this review session. No duplicate wake was queued for this wait_id.

FIRST DIVERGENCE
The wait exceeded budget because one changed-source translation unit (src/network/server-npcs.cpp) takes ~100s to compile, plus a ~58s LLD link, so total wall time (~197s) is inherently above the fixed 60s wait budget. The build was not idle or hung.

EVIDENCE DEFECT (does not block this occurrence)
a. evidence.build_id "2026-11-22T17:39:49.763492Z" and evidence.generation 507 are the previously published build/generation, not the build that caused this wait (game-dll-20261122T180535335873Z-773b0b, generation 508). The active attempt id is absent from the evidence; related_build/related_run have now been corrected to the real attempt and run.

LATE DISPATCH (already owned elsewhere)
The reviewer reached this wait ~29 minutes after issue creation (18:06:37 -> 18:35:56) because the codex primary adapter exited and the opencode fallback waited for the next drain. This matches the existing issue-84c584957e65; not re-filed here.

RECURRENCE / DUPLICATE NOISE (needs a policy decision, no code change made)
This is 1 of 118 "Wait over budget: game-dll-build at game_build" issues; 9 were created after 17:00 today, including issue-f6c050fa46bc, issue-f31c330d18e0, issue-e7b2635a5d05, issue-9202020e4618. Per-wait_id uniqueness plus a 60s budget below the normal single-TU + link cost means nearly every changed-source game-DLL build files another issue and wakes another reviewer. Suggested direction (same as the prior review of issue-e45921a75b6e): group by kind+stage into one canonical issue with occurrences, or condition the budget on compile progress / expected per-TU cost instead of first-start age. Ownership and budget decision belong to build-friction; left unowned.

NO CODE FIX CLAIMED. No repository source or config edited; coordination records only.
