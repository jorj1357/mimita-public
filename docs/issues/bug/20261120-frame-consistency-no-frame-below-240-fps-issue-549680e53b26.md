# issue-549680e53b26: Frame consistency: no frame below 240 FPS

## Human report (verbatim, immutable)

> i want to get like 240fps 99.99% of the time ... no frames should be below that if we can help it ... super super consistent no jittering that is biggest issue no stuttering or jittering


## Task linked

### 2026-11-20T04:59:57.219Z — runtime-60hz

Task: ui-cost-trace (step 0)

## Task linked

### 2026-11-20T04:59:58.859Z — runtime-60hz

Task: frame-spike-trace (step 0)

## Context

### 2026-11-20T06:25:31.325Z — work-metrics

reproduction: Run the 67-NPC scenario with frame timing enabled and inspect accepted frame times.
expected_behavior: Every accepted frame is at most 4.167 ms (240 FPS), with no 239 FPS pass; 999 FPS is aspirational and not required.
acceptance: Journal reports 100 percent of accepted frames at or below 4.167 ms; visible 240 tested on a 240 Hz display.
paths: ['docs/specs/performance/performance.md', 'devscripts/dev-loop.py', 'src/engine/engine-tick-render.cpp']
evidence: {'runtime': 'logs/2026-11-20/20261120_040807', 'observed': '90-120 FPS, MAX spike 2368.59 ms', 'display_hz': 60}

## Agent suggestion

### 2026-11-21T00:10:57.648Z — fps-dip-auto-trace-intake

Related capability proposal: issue-13d080318e1c specifies automatic per-frame detection below the existing 4.167 ms/240 FPS budget, bounded client events.jsonl evidence, and creating/extending diagnosis issues by stable slowdown signature. Keep this frame-consistency issue as the outcome/performance target; use the new issue for instrumentation and issue automation. No implementation or runtime proof is claimed.

## Agent suggestion

### 2026-11-21T14:56:05.541Z — map-editor-freeze-issue

Related editor-specific issue: issue-ba939787ffa3 records intermittent map reload/save/copy-paste freezes, including a user-reported ~0.5-second Payload route copy/paste stall. Use the existing 240 FPS frame-consistency target as the shared outcome; validate editor operations with pinned per-frame timing and owner-level scopes.

## Scheduled independent review

### 2026-11-22T07:18:36.892Z — build-wait-reviewer

A bounded independent review was queued as wake wake-b27560f534ad after 258 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence)

### 2026-11-22T07:33:52.977Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Scope: bounded independent read-only review of issue brief/readiness, proof status, linked artifacts, and the newest relevant journals. No source edit, build, run start, close, or fix is claimed by this review.

OBSERVED
- Readiness: ready=false, score 6/8, state=new, owner=unassigned, missing proof_expectation and risk. Tasks frame-spike-trace and ui-cost-trace are both state=open, owner=unassigned, with empty acceptance/evidence/method; proof status has no record for either task, and this is the issue first review.
- The issue only cited evidence is unavailable. evidence.runtime is logs/2026-11-20/20261120_040807; logs/2026-11-20 contains no run directories and recursive search of logs/ found no *040807* path. The recorded 90-120 FPS, MAX spike 2368.59 ms, display_hz=60 observation therefore cannot be reproduced from its own citation.
- New current-generation client evidence, run 20261122_065419_fcs-route-proof-final, build 2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344, game generation 470, FCS on dust2cyberiav4, about 290 s: 278 one-second performance.fps windows / 31,850 frames, 31,224 over the 4.1667 ms budget = 98.03 percent; zero windows with zero over-budget frames; median window average 8.75 ms; median window p50 7.65 ms; best window p50 4.24 ms; max frame 5,500.36 ms. Neighboring FCS runs are also far over budget: 20261122_064826_fcs-route-proof-stable = 96.96 percent over (229 windows / 25,642 frames, max 4,225.04 ms) and 20261122_063942_fcs-route-proof = 96.32 percent over (431 windows / 52,449 frames, max 5,288.76 ms), both with zero clean windows.
- First over-budget owner in steady state: performance.frame-budget records (4.0 ms capture threshold) show repeated 48-88 ms main-thread frames dominated by Setup::ConfigPolling::MatchRoles (max self 37.6 ms) and Setup::ConfigPolling::Gamemode (max self 28.8 ms), plus Setup::ConfigPolling::Game2. This run has 36 such spikes with MatchRoles self above 10 ms at irregular intervals (median 2.1 s). Worst non-startup frames: t=220.6 s total 88.3 ms, t=29.5 s 87.6 ms, t=225.7 s 87.0 ms, t=269.1 s 78.5 ms. The t=10.1 s frame is a separate 5,500 ms startup/load spike dominated by State=2,975 ms, Rendering::Actors::PlayerBody=668.8 ms, and Audio::AudioUpdate=426.2 ms. Many otherwise near-budget frames also carry Rendering::Skybox::Reload up to 4.4 ms and Swap 0.8-1.4 ms.
- Therefore the acceptance statement (Journal reports 100 percent of accepted frames at or below 4.167 ms) is not met in any of these three current-generation client runs.

INFERENCE / UNCERTAINTY
- The evidence proves the current client still misses the per-frame target badly in real FCS runs. It does not reproduce the issue exact 67-NPC scenario, these client records report npcs_active=0, display refresh is not recorded in the sampled lines, and no 240 Hz visible test was performed, so this is not acceptance proof of the issue.
- The recurring config-polling spikes are a plausible shared stutter owner across modes and editor flows, but this is main-thread self-time attribution, not a complete cause or a fix.

CONTRARY EVIDENCE
- None observed that the issue is fixed or that frames meet budget; there are no issue proofs. The scope mismatch above limits the claim; it does not contradict the over-budget finding.

NEXT DISCRIMINATING CHECK
1. Run the issue 67-NPC scenario on the newest published generation with frame timing and a recorded display refresh; capture performance.fps, performance.frame-budget, and performance.frame-slice for a >=60 s steady window and report p50/p95/p99/p99.99/max plus the exact over-budget count.
2. In that same run, check whether Setup::ConfigPolling::MatchRoles/Gamemode and Rendering::Skybox::Reload still dominate the largest non-startup frames; if yes, measure the polling interval and whether unchanged config is being re-parsed. Smallest experiment: one owner A/B build that debounces or skips config polling when file hashes are unchanged, then compare the count of frames above 4.167 ms over an equivalent steady window. A build or cosmetic-only change is not proof.
3. Add the missing proof_expectation, for example: pinned 67-NPC run, >=60 s steady client window, journal shows 0 accepted frames above 4.167 ms with the full percentile/max report, and a visible 240 Hz check only when a 240 Hz display is actually used.

Confidence: high that current gen-470 client frames are far over the 240 FPS budget in the observed FCS runs; medium that Setup::ConfigPolling is the dominant steady-state stutter owner pending the 67-NPC reproduction; low/unknown for visible 240 Hz behavior.

## Agent suggestion

### 2026-11-22T14:35:33.216Z — codex-networking-stall-fix

Cross-issue frame consistency evidence from pinned FCS run 20261122_134637 (gen495): first measured client frame window had frame_max_ms=4844.5137 and frame_avg_ms=2934.6653; State scope max=2130.1 ms with ~2,995,553 allocations, Audio max=1303.2 ms, MusicUpdate max=1004.5 ms. Server first-frame startup also had a 4547 ms stale classification and the client later applied a snapshot after 5353 ms. These were startup effects separate from the later exact 7.03s Recast query. No claim that the new NPC guard resolves the startup frame budget; fresh-runtime verification is pending.

## Agent suggestion

### 2026-11-22T18:09:33.492Z — codex-networking-stall-fix

Observed pinned runtime run 20261122_174002, build 2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d, game generation 507, client journal logs/2026-11-22/20261122_174002/client-01/events.jsonl. At 17:40:44.364Z stall watchdog reported frame_heartbeat_age_ms=1709, frame 1186, progress engineTickSimulate/Networking line 216. Stack captured 17:40:44.399Z and repeated at 50.145Z and 56.117Z: NtCreateFile -> GetDriveTypeW -> CreateFileW -> wmktemp_s -> wopen -> game-g502.dll+0x12d5f3 resolves to CosmeticSystem::loadCosmeticGLB at src/avatar/cosmetic-system.cpp:91:24, -> loadCosmetics at :108:24 -> AvatarSystem::applyAvatarToPlayer at src/avatar/avatar-atlas.cpp:1062:70. This pins a synchronous GLB cosmetic load/file operation inside avatar application on the client networking tick. Client journal shows last normal snapshot around 17:40:41.973Z then next at 17:40:58.695Z (16.722s gap); server journal continued sending snapshots roughly each second, 8 chunks and zero failed chunks. Watchdog heartbeat age reached 15,953ms while main-thread CPU advanced only ~16ms. Client scheduled health_lost reconnect at 17:40:58.743Z, accepted at 17:40:58.782Z, and snapshots resumed ~17:40:59.714Z. Observed cause: blocking cosmetic load in network tick stalls packet/frame processing and makes a healthy-sending server look stale. Safe fix needs CPU parse/file IO off main thread with GL upload on owner thread, reusing existing async GLB pattern rather than moving GL calls to a worker. This run is FCS/reconnect, not direct Community Browser join reproduction; exact browser flow remains to reproduce. Do not close until pinned post-fix runtime proves avatar/cosmetic replication does not stall snapshots.

## Agent suggestion

### 2026-11-22T21:49:47.573Z — codex-networking-stall-fix

Cross-post from networking trace: pinned FCS client run 20261122_213500 had a 6407.520ms frame at 21:35:30.085Z; 6399.202ms/24,989,192 bytes were attributed to Rendering::Actors::NetworkEntities (src/engine/engine-tick-render.cpp:658). Its prior frame scope at 21:35:21.267Z reported Rendering::Actors::NetworkEntities 36.862ms and 6,683,238 bytes. A 7.08s snapshot interarrival overlapped the 6.4s actor-render stall. I will instrument the shared renderNetworkPlayer pre-draw model/avatar finalization calls before selecting the fix.

## Agent suggestion

### 2026-11-22T22:31:05.842Z — codex-networking-stall-fix

Pinned follow-up run 20261122_222623 (EXE e24aaf16..., DLL generation 537, FCS dust2cyberiav4, 19 NPCs) confirms a second server snapshot-starvation path. At 22:27:32.733Z, npc.nav.recast-query-breakdown event NPC_MOVEMENT_2346 in server-01 measured query_ms=5723.673 and mesh_setup_ms=5721.425; it attempted a full tiled mesh build (full_mesh_build_attempted=true, had_mesh_before_setup=false, needs_build=true), then failed nearest start polygon. Actor 100017's npc.nav.plan-cost at 22:27:32.741Z reports recast_ms=5727.727, goal distance 95.859m, while the server fixed tick stalled 6783.283ms at 22:27:33.685Z (npc_ms=6728.433, snapshot_ms=2.866). Crucial evidence: the Recast breakdown says inside_fixed_tick=false and navigation_policy_allowed_rebuild=true, even though src/network/server.cpp brackets its accumulated simulation loop with ModuleHost::beginFixedTick/endFixedTick (:1529 and :1740). Root source mismatch: the game DLL bridge methods in src/game/module-host-bridge-module.cpp forwarded those calls to the resident host but did not update the DLL-side ModuleHost::mFixedTickActive that Recast reads through insideFixedTick(). I patched begin/end to mirror the state; candidate build gen538 and pinned runtime verification pending. Keep issue open. Separately, async NPC avatar catalog change in gen537 reduced adoption from 10.1s/9.92s spawn phase to 268ms/57.7ms respectively; 19 NPCs, with no >2.5s client snapshot gap in the initial 23 applied snapshots. That addresses one stall but not the Recast rebuild or later resource-pressure stalls.
