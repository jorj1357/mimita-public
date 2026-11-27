# issue-f36679f4ba95: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-wait)

### 2026-11-27T04:10:56.417Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (wake-f30a76fe5a01, session session-2fc5e50efc7c). Read-only: inspected the issue, the dev-loop journal, and the build timing summary; no source edits, no builds, no leases.

Occurrence is VALID (not a false positive):
- wait_id game-dll-build:2026-11-27T03:46:49.521Z; kind game-dll-build; stage game_build; budget 60.0s; elapsed 61.22s; trigger request 2026-11-27T03:46:49.303Z-5f223a6c:mode3:game.
- dev-loop.wait.over_budget fired once at 2026-11-27T03:48:13.916Z and dev-loop.wait.reviewer-wake fired once at 2026-11-27T03:48:13.915Z (run 20261127_032430; journal logs/2026-11-27/20261127_032430/dev-loop/events.jsonl lines 91-92). Exactly one issue (this one) and one wake (wake-f30a76fe5a01; request .dev/agent-wake/requests/wake-f30a76fe5a01.json).
- The waited-on attempt game-dll-20261127T034649716100Z-80c771 started 03:46:49.716Z, completed 03:48:43.676Z, total 113.953s, success=false, failed_phase=link. It was progressing while the wait fired, so this is a real over-budget build, not a dead wait.

First divergence for the wait = changed-source compile, not object_cache_scan:
- phase_durations_ms: changed_source_preflight 16344, compile 76672, object_cache_scan 2500, link 6047.
- 16 TUs compiled although only src/input/gamepad-input.cpp changed (changed_paths). worker_metrics: compiled 16, skipped 630, compile_sum 464684ms, jobs 10, max TU 47.368s (src/engine/engine-tick-camera.cpp), min 0.580s.
- The budget was exceeded during compile: compile wall alone is 76.7s > 60s, and preflight (16.3s) + slowest TU (47.4s) = 63.7s, so the wait was unavoidable once this TU set was chosen. This differs from issue-ad873 (object_cache_scan) and issue-dc6792 (packet codegen).
- Why 16 TUs from a one-file change (cache_invalidations): 2x "object file is empty" (engine-tick-camera.cpp, multiplayer-physical-entities.cpp), 10x "dependency newer than object: src/vehicles/vehicle-types.h", 1x "missing or invalid dependency file" (vehicle-system.cpp). 15 of the 16 recompiles were not caused by the changed source; they are object-cache / header-mtime invalidations.

Build outcome (separate from the wait; same known regression):
- Link failed in 5.985s with undefined symbols whose definitions exist in the tree: effectiveGameplayMouseButton (src/input/input-poll.cpp:170), MimitaNet::CommunityMatchClient::{selectFighterWeapon:115, fighterWeaponChoiceVisible:101, fighterWeaponChoiceCommitted:109, fighterWeaponId:89} (src/network/community-match-client.cpp), consumeScheduledTerminalMoveForNetwork:126, peekTerminalInteract:206, buildInputFrame:396 (input-poll.cpp), MapEditor::renderWorld.
- The next three attempts failed the same way on different symbols: 034911 (12.59s, compiled 1), 034952 (12.81s, compiled 1), 035109 (33.84s, compiled 2, last-attempt gen 763). This matches docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md ("each attempt fails on a different symbol"): this wait is downstream of that object-cache/link-list regression, not a new independent cause.

Acceptance check (verbatim): "The issue carries wait_id, stage, reason, elapsed, build/run IDs, and evidence; a reviewer wake is queued exactly once for this wait occurrence."
- wait_id yes; stage yes (game_build); elapsed yes (61.22); evidence yes; reviewer wake exactly once yes.
- reason NO: evidence.reason is a mid-JSON tail ("59485.0,\"build_attempt_id\":..."), not a readable stage summary. Known recurring defect: issue-6e08d89b55e3.
- build/run IDs NO: evidence.build_id=2026-11-27T01:43:27.658897Z does not match attempt game-dll-20261127T034649716100Z-80c771 (started 03:46:49.716Z); generation=null; no run_id. Same stale-identity symptom already noted on issue-6e08d89b55e3 (2026-11-26).
- So acceptance passes on record/wake uniqueness but fails on readable reason and correct build identity.

Resource context (observed, not causal):
- .resource.json: 86 samples @1s over 113.937s, mean host CPU 96.796%, peak 100%, minimum available memory 477,691,904 B (~455.6 MiB); sampler thread 25.3s CPU; runtime_alignment unavailable (fresh server/client journal not matched). Sibling attempts 034952/035024/035109 surface gap_key build-resource-sampling:* with unavailable matched_prebuild_resource_baseline / host_commit_charge_and_limit / journal_aligned_live_mode_workload_and_frame_tick_metrics. Host pressure is correlated but not proven causal; recorded as unknown, not zero. Gap keys appended once to issue-136d4553ff92.

Recommendations (report-only; no code changed or claimed):
1. Keep the 60s budget; this wait is valid.
2. The dominant avoidable cost here is 15 unnecessary recompiles from empty/stale objects and the vehicle-types.h mtime dependency - the same cache-health family as REG-2026-11-27. Complete the documented quarantine + full rebuild (human-authorized) before further wait triage, and switch invalidation to content hash (regression Prevention #3).
3. Fix the wait snapshot to store a readable stage + elapsed and the in-flight attempt id/generation (tracked by issue-6e08d89b55e3) so this acceptance item is actually met.
4. Attribute over-budget waits to the dominant substage and consolidate repeats under the existing tracker instead of one high-priority issue per wait_id.

Verdict: confirmed-build-wait. Evidence review only; no runtime proof claimed.

## Context

### 2026-11-27T04:12:34.372Z — build-wait-reviewer

related_build: game-dll-20261127T034649716100Z-80c771
related_run: 20261127_032430
