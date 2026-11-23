# issue-8bfc1ca56222: Juggernaut 67-NPC runtime misses 60 Hz and 240 FPS

## Human report (verbatim, immutable)

> record both do ur recommendation but get 240fps


## Triage

### 2026-11-20T03:15:34.343Z — coordination-v2

Summary: Pinned baseline run 20261120_031127 produced 67 NPCs and the expected arcade path, but server loop was 11-13 Hz with 100-570 ms stalls and client was 11-17 FPS; server NPC AI and client frame budget need separate owners.
Owner: coordination-v2
Priority: high

## Assignment

### 2026-11-20T03:15:34.834Z — coordination-v2

Owner: coordination-v2

## Task linked

### 2026-11-20T03:15:36.683Z — coordination-v2

Task: juggernaut-60hz-baseline (step 0)

## Task linked

### 2026-11-20T03:15:37.033Z — coordination-v2

Task: juggernaut-server-60hz (step 0)

## Task linked

### 2026-11-20T03:15:37.458Z — coordination-v2

Task: juggernaut-client-240fps (step 0)

## User quote

### 2026-11-20T03:33:16.111Z — coordination-v2

Via coordination-v2; supports issue-8bfc1ca56222:

> Pinned baseline 20261120_032618: 67 NPCs, 67 remote NPCs, physical_contact_ms=0; server 11-13 Hz with 100-570 ms stalls (npc_ms 30-220+; ai_ms~818-888/s); client 8-15 FPS, NetworkEntities up to 922 ms, Ragdoll::WorldCollision up to 810 ms; snapshots missed 697, interarrival ~1.0-1.4s. Source culling edit is pending build.

## Task linked

### 2026-11-20T03:33:16.506Z — coordination-v2

Task: remote-cull-240fps (step 0)

## Task linked

### 2026-11-20T03:44:50.364Z — runtime-60hz

Task: connection-stability (step 0)

## Agent suggestion

### 2026-11-22T14:35:31.676Z — codex-networking-stall-fix

Cross-issue evidence from the FCS runtime, relevant to NPC/tick budget: run 20261122_134637 (Fun Counter Strike, 19 NPCs, generation 495) recorded server npc.slow-update actor 100011 at 2026-11-22T13:59:45.064Z with total_ms=7035.3385, navigator_ms=7032.7936, decision_ms=7034.2276; npc.nav.plan-cost seq 95522 had recast_ms=7030.6396 and to_goal_m=0.0. The same tick stalled 8404.226 ms. This identifies a shared NPC navigation hotspot relevant to high-NPC server rate, but does not prove the 67-NPC scenario has the same cause. A guard was added in src/npc/npc-navigator.cpp and generation 499 published; fresh FCS runtime evidence is pending.

## Agent suggestion

### 2026-11-22T15:44:04.864Z — codex-networking-stall-fix

Current FCS pressure run 20261122_150906 (gen500) had 19 NPCs. Around 15:25:59Z server tick window: 5/5 ticks over 100ms; total_avg=1324.96ms/max=1856.43ms, npc_avg=1057.45ms, gamemode_avg=200.37ms/max=538.28ms; outer loop 0.689Hz and max iteration 7198.7ms. User-visible snapshot interval max was 4760ms. This gives matched live FCS evidence to separate NPC/game-mode costs rather than treating it as only a transport issue; source subowner attribution remains open.

## Agent suggestion

### 2026-11-22T16:41:16.163Z — codex-networking-stall-fix

Fresh no-build FCS/dust2cyberiav4 pinned run 20261122_163129 (EXE SHA 4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136, game DLL gen502 SHA 2263b140a04bcbb12a60b719632708439c077bcc56affaa2eb13e2f45d7c744c) still missed server-ready: dev-loop timed out at 60s waiting for network.server.local-gameplay-ready and stopped before launching the client. Server journal: logs/2026-11-22/20261122_163129/server-01/events.jsonl. world_load started 16:31:33.341Z; map load succeeded 16:31:49.278Z; authored base capture logged 16:32:06.142Z; authored geometry completed 16:32:16.148Z; npc.collision-world.summary logged 16:32:21.772Z (seq78). No server.policy.world event, world_load complete, room registration or local gameplay-ready before the harness timeout. In src/network/server-npcs.cpp:384-410, npc.collision-world.summary is emitted before rebuildNpcSolidBounds and buildCollisionChunks, so the ordered journal/source trace narrows the stall to the remaining NPC collision prep path, with buildCollisionChunks as the leading call-site candidate; it does not yet prove which call duration. I queued start/complete phase events around both calls; candidate runtime proof is pending.

## Agent suggestion

### 2026-11-22T16:46:15.726Z — codex-networking-stall-fix

A bounded startup diagnostic now emits npc.collision-world.phase started/complete events around rebuildNpcSolidBounds and the shared buildCollisionChunks call inside buildNpcWorldCollision. In the prior gen502 run 20261122_163129, npc.collision-world.summary (seq78 at 16:32:21.772Z) was the final server event before the 60s local-gameplay-ready timeout, and source order places it before both calls. This instrumentation is not in a verified runtime artifact yet; it will show which phase completes or remains in progress.

## Agent suggestion

### 2026-11-22T19:01:40.224Z — codex-networking-stall-fix

Observed pinned candidate run 20261122_184056 (Fun Counter Strike, dust2cyberiav4), client/server journals under logs/2026-11-22/20261122_184056, EXE build 2026-11-22T17:39:49.763492Z SHA-256 3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d; launched with game DLL generation 512. Server accepted client/player 1 at 18:41:41.186Z and sent Welcome successfully at 18:41:41.386Z. It sent 3 chunks at 18:41:41.445Z, then next snapshot send was 18:41:53.607Z: 12.162 seconds later. Client applied its next snapshot at 18:41:53.607Z with snapshot_interarrival_ms=11281 and had scheduled health_lost reconnect at 18:41:52.389Z. The server journal pins the missed send to the authoritative NPC simulation: performance.server-tick-stall at tick 838, src/network/server.cpp:1595-1611, total_ms=11219.172, npc_ms=11202.531, npcs=19, players=1. Its nested npc.tick-cost (simulateSharedNpcs, src/network/server-npcs.cpp:3249) reports npc_adoption_ms=8471.6469, npc_ai_ms=38.1601, ai_update_ms=0.3799, pair_ms=134.8701, rebuild_ms=2.3346, total_ms=8648.0323. Thus this reproduction accepted the connection; the long snapshot wait follows a large one-time NPC adoption/spawn cost, not a server acceptance rejection or missing steady snapshot sender. The adoption timer starts immediately before adoptNewServerNpcs at src/network/server-npcs.cpp:1967-1968; likely hot candidates inside that function are spawnNpc at :614 and profile/finalize work at :664-726. Exact inner operation still needs bounded timing; do not claim a specific subcall yet. After the stall, snapshots resumed and client received 19 NPCs. Candidate runtime also emitted cosmetic.load.started/completed for jason1.glb 2 ms apart, so the cosmetic async change is exercised but does not address this server-side NPC adoption stall. Keep networking/startup/NPC issues open; next discriminating step is measure adoptNewServerNpcs subphases under pinned FCS startup and optimize the dominant existing owner.

## Agent suggestion

### 2026-11-22T20:32:24.422Z — codex-networking-stall-fix

New runtime evidence from pinned FCS networking-validation run 20261122_202850 (EXE build 2026-11-22T19:53:22.116562Z, game DLL gen514, dust2cyberiav4; 19 NPCs): client emitted network.client.snapshot.gap at 20:29:23.776Z with snapshot_age_ms=2502 and then 1s increments to 9512ms; server fixed-tick stall at 20:29:31.468Z was 10342.214ms, npc_ms=10234.199, snapshot_ms=70.732. npc.tick-cost seq836 at 20:29:30.226Z attributed 9011.717ms to npc_adoption_ms (simulateSharedNpcs in src/network/server-npcs.cpp:3249); next NPC tick seq1724 at20:29:35.539Z took 4069.372ms, with ai_update_ms=4063.244 and worst_ai_actor=100018 at4016.699ms. Thus long first-contact snapshot wait is directly explained by synchronous NPC work on the same server fixed-tick thread, not the snapshot sender in this run. Exact inner adoption operation remains unknown and files are currently under another agent ownership query; fix and stability not proven. Keep open.

## Agent suggestion

### 2026-11-22T20:38:50.899Z — codex-networking-stall-fix

Additional finding from pinned runtime 20261122_203657 (EXE build 2026-11-22T19:53:22.116562Z, game DLL generation 515, FCS, 19 NPCs): server NPC actor 100018 planned a 6-node route to an objective 326.865m away in tick1. Event npc.nav.plan-cost seq1116 at 20:37:07.137Z reports total_ms=1492.6602, of which recast_ms=1492.4011; local_plan_ms=0.259, global_graph_ms=0.0001. In the same tick, NPC AI cost was 1526.898ms and server fixed tick 1545.5ms, producing a snapshot cadence stall. Source: src/npc/npc-navigator.cpp:1466 reports this query; Recast call chain in src/npc/recast-navigation.cpp:1413-1417 nearestPolyEscalated (start/dest), then :1442 findPath/:1456 straightPath. This is not the earlier zero-distance/7.03s case, but it corroborates that Recast can consume >1.49s on an ordinary long-goal query and starve server snapshots. The current log does not split nearest-poly vs findPath time; fix not yet proven. Please correlate with existing slow/zero-distance Recast evidence.

## Agent suggestion

### 2026-11-22T20:52:13.523Z — codex-networking-stall-fix

Fresh FCS runtime 20261122_204150: 20:42:00.338Z recast-query-breakdown was 2536.742ms total, 2536.326ms mesh setup; nearest-poly and findPath stages were negligible/zero. Same tick npc_ms=2585.731ms; tick total=2588.556ms. A separate 3626ms server tick at 20:47:18 had npc_ms=4.558ms and snapshot_ms=.175ms, showing another server phase can stall independently. Instrumentation of the exact mesh rebuild decision is queued for a pinned run; first diagnostic build attempt failed disk reserve before compile.

## Agent discussion and suggestions

### 2026-11-22T21:51:59.446Z — codex-networking-stall-fix

Message: `msg-f19f240c8160` (evidence)

Subject: Shared DLL build blocked on playerThreat scope in server-npcs.cpp

Root dev-loop candidate game-dll-20261122T215013158759Z-3603d1 compiled render-player.cpp instrumentation but failed at server-npcs.cpp:2415:42: playerThreat was not declared. In current source, playerThreat lambda is declared inside the TargetSelectV1 local block around line 2312, but reused in the later fallback scoring block at 2415 outside that scope. Please inspect/fix your owned NPC change or coordinate ownership; I will not edit server-npcs.cpp. Attempt generation 529, source hash 97db53dc, request 2026-11-22T21:50:32.886Z-e902a3f0; no DLL published.

## Agent suggestion

### 2026-11-22T22:31:07.635Z — codex-networking-stall-fix

Pinned follow-up run 20261122_222623 (EXE e24aaf16..., DLL generation 537, FCS dust2cyberiav4, 19 NPCs) confirms a second server snapshot-starvation path. At 22:27:32.733Z, npc.nav.recast-query-breakdown event NPC_MOVEMENT_2346 in server-01 measured query_ms=5723.673 and mesh_setup_ms=5721.425; it attempted a full tiled mesh build (full_mesh_build_attempted=true, had_mesh_before_setup=false, needs_build=true), then failed nearest start polygon. Actor 100017's npc.nav.plan-cost at 22:27:32.741Z reports recast_ms=5727.727, goal distance 95.859m, while the server fixed tick stalled 6783.283ms at 22:27:33.685Z (npc_ms=6728.433, snapshot_ms=2.866). Crucial evidence: the Recast breakdown says inside_fixed_tick=false and navigation_policy_allowed_rebuild=true, even though src/network/server.cpp brackets its accumulated simulation loop with ModuleHost::beginFixedTick/endFixedTick (:1529 and :1740). Root source mismatch: the game DLL bridge methods in src/game/module-host-bridge-module.cpp forwarded those calls to the resident host but did not update the DLL-side ModuleHost::mFixedTickActive that Recast reads through insideFixedTick(). I patched begin/end to mirror the state; candidate build gen538 and pinned runtime verification pending. Keep issue open. Separately, async NPC avatar catalog change in gen537 reduced adoption from 10.1s/9.92s spawn phase to 268ms/57.7ms respectively; 19 NPCs, with no >2.5s client snapshot gap in the initial 23 applied snapshots. That addresses one stall but not the Recast rebuild or later resource-pressure stalls.
