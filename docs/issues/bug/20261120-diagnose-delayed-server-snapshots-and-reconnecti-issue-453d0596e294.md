# issue-453d0596e294: Diagnose delayed server snapshots and reconnecting state

## Human report (verbatim, immutable)

> we have waiting for server snapshots issue like  redconnecitng issue idk what the deal is


## Agent suggestion

### 2026-11-20T07:56:28.345Z — launch-latest-stable-fix

New exact run 20261120_074813 proves same snapshot/reconnect symptom: client sends hello after UDP transport open; server logs 
etwork.server.client-hello-accepted player_id=1 at 07:49:02.517Z, while client stays at snapshots_received=0 and 
etwork.join.map-required-missing, then tears down as connection failure. Dev-loop reports client_connection timeout at 75.25s although PID 33960 remains alive. Correlate server's first welcome/snapshot sends with client receive; this is not a proven process crash. Run ID/build/request are in docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.

## Agent suggestion

### 2026-11-20T08:21:46.741Z — launch-latest-stable-fix

GUI-invoked run 20261120_081941_01 failed client_connection at 70.703s (45.263s stage timeout), while server/client PIDs 6596/9044 both remained alive. Server/client journals are under logs/2026-11-20/20261120_081941_01/{server-01,client-01}/events.jsonl; launch report .dev/launch-reports/2026-11-20T08-19-41.839Z. The root dev-loop log contains pre-timeout inbound Hello/accept from prior matching run; verify this run for Welcome send/receipt before assigning further cause.

## Agent suggestion

### 2026-11-20T08:34:58.680Z — launch-latest-stable-fix

Correction: full rotated server journal logs for GUI attempt 20261120_081941_01 confirm network.server.client-hello-accepted at 08:20:17.982Z (server-01/events-0001.jsonl); server received/handled Hello. The sendToSourceOrPlayer result and client first-datagram receipt were not logged by this binary, so the precise send/receive split remains unproven until the candidate DLL lands. Client did not reach Connected; launch report code client_connect_timeout at 70.703s.

## Agent suggestion

### 2026-11-22T14:44:44.726Z — codex-networking-stall-fix

Cross-issue evidence from host side: in run 20261122_141131, server startup ended `room_register` failed after 39451 ms (events seq 288) while server `snapshot.build`/`snapshot.send` continued at about 1-second intervals and NPC avatar summaries continued. The pasted headless log also shows ICE candidate gather timeout and empty `serverCode`, so visible NPC activity does not mean a room is registered or that the joining client passed admission/snapshot delivery. This evidence does not identify the first missing packet for this snapshot issue; it is a separate host-registration failure to correlate in the paired GUI reproduction.

## Agent suggestion

### 2026-11-22T14:49:08.763Z — codex-networking-stall-fix

Correction to prior cross-post: current HEAD src/network/server.cpp:1321-1329 / src/network/server-ice.cpp:305-326 are candidate analogues only. User-pasted build header is commit=64b085c; git show at that commit has synchronous coordinatorRegister and no initServerIceListener/ICE gather log path; server-ice.cpp is absent. Observed room-register failure and ongoing snapshots are valid runtime evidence, but do not attribute the exact cause to current lines until process artifact/source generation is matched.

## Agent suggestion

### 2026-11-22T15:44:03.332Z — codex-networking-stall-fix

Run 20261122_150906, FCS/dust2cyberiav4, gen500, remote_npcs=19: 472 logged applied-snapshot intervals; max 4760ms at client seq127784 (15:25:37.707Z); 21 intervals exceeded 2.5s and 468 exceeded 1s. Server window at 15:25:51.935Z recorded loop_max=7587ms; 15:25:59.193Z recorded loop_max=7198.7ms/actual_hz=0.689. Tick window near 15:25:59 had total_avg=1324.96ms, total_max=1856.43ms, npc_avg=1057.45ms, gamemode_avg=200.37ms, gamemode_max=538.28ms. This confirms snapshot delivery is coupled to severe authoritative server tick under-run in this pressure run; it does not yet identify which subowner causes the tick budget overruns.

## Agent suggestion

### 2026-11-22T16:02:37.162Z — codex-networking-stall-fix

New user report now explicitly ties the in-game community browser to a two-sided failure: NPC activity is visible, while the joining client reports snapshots N seconds old or server acceptance fails. No fresh public-browser reproduction has been captured in this run yet, so this is a symptom/acceptance-chain lead, not a proven cause. Keep join acceptance and snapshot age as separate checkpoints.

## Agent suggestion

### 2026-11-22T16:11:18.557Z — codex-networking-stall-fix

Source lead for differentiating this user's two reported UI states: src/network/multiplayer-packets.cpp::mpConnectionHealthText lines 2280-2328 renders snapshots N seconds old from WaitingForServer and lastSnapshotReceivedMs; WaitJoinAccept instead renders waiting for server acceptance. A server JoinReject has a distinct client event at src/network/multiplayer-tick.cpp:1786-1807. Paired runtime evidence is needed to determine whether these are two observations in one stalled connection or separate failures.

## Agent suggestion

### 2026-11-22T16:19:43.363Z — community_join_trace

Source trace note: public server row click (online-menu.cpp:224-235) bypasses manual code lookup and forwards listed e.code; gui-main.cpp:1842-1900 hands that code to gPendingConnect then switches to GAME_PLAYING. The existing run 20261122_150906 only proves a local direct connection (server client-hello-accepted at server journal line 617; EJJWLLH registered at line 93), not the browser/ICE flow. Lowercase typed-code lookup failures are a separate path. Please capture the first missing browser transition with a pinned GUI click and paired journals.

## Agent suggestion

### 2026-11-22T17:20:01.309Z — codex-networking-stall-fix

Source lead: src/debug/log-manager.cpp LogManager::init ran cleanupOldFormat and rotateLogs before MimitaNet::runServer initialized StructuredLogger. rotateLogs recursively walked the entire logs tree and sorted matching files; cleanupOldFormat could remove a whole dated directory. Removed both traversals/calls and dead declarations. Duration, logger readiness improvement, and connection effect remain unproven pending pinned DLL runtime.

## Agent suggestion

### 2026-11-22T17:28:09.645Z — codex-networking-stall-fix

Source trace for the two UI waits: src/network/multiplayer-tick.cpp sendJoinRequest emits request_sent at lines 1484-1502 while Connecting/WaitJoinAccept; only PACKET_JOIN_ACCEPT switches to Connected at 1669-1729, and PACKET_JOIN_REJECT is logged separately at 1761-1791. Snapshot age text is derived from lastSnapshotReceivedMs and is not itself proof of acceptance. Runtime browser-row correlation is still pending.

## Agent suggestion

### 2026-11-22T17:45:38.035Z — codex-networking-stall-fix

Cross-agent live FCS run 20261122_174002 is active in root dev-loop while a 642-source/10-job game-DLL build runs. EXE build 3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d; server room RQN8QSG, local client joined. Server room_register took 953.878ms after world_load; full startup to room registration was about 18.6s (logger 4.14s, config 4.986s, world_load 8.204s; server.policy.world reports NPC collision build 4.177s). Client accepted at 17:40:32.431Z, then snapshot_starved age 2514ms at 17:40:35.283Z. Server continued network.server.snapshot.send every ~1s with peer connection_stale=false and 8 chunks while client received no snapshots for 16.722s (last 17:40:41.973Z, next 17:40:58.695Z); client performance/frame events continued. Client then scheduled health_lost reconnect at snapshot_age_ms=16087, reconnect accepted, status waiting_for_snapshot (room_code empty in client reconnect event), and snapshots resumed 0.93s later and about 1.0s intervals. This separates server send cadence from client receive/reassembly/recovery, but does not yet identify packet transport loss vs client receive handling. Journals: logs/2026-11-22/20261122_174002/{server-01,client-01}/events.jsonl.

## Agent suggestion

### 2026-11-22T18:08:38.950Z — codex-networking-stall-fix

Observed pinned runtime run 20261122_174002, build 2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d, game generation 507, client journal logs/2026-11-22/20261122_174002/client-01/events.jsonl. At 17:40:44.364Z stall watchdog reported frame_heartbeat_age_ms=1709, frame 1186, progress engineTickSimulate/Networking line 216. Stack captured 17:40:44.399Z and repeated at 50.145Z and 56.117Z: NtCreateFile -> GetDriveTypeW -> CreateFileW -> wmktemp_s -> wopen -> game-g502.dll+0x12d5f3 resolves to CosmeticSystem::loadCosmeticGLB at src/avatar/cosmetic-system.cpp:91:24, -> loadCosmetics at :108:24 -> AvatarSystem::applyAvatarToPlayer at src/avatar/avatar-atlas.cpp:1062:70. This pins a synchronous GLB cosmetic load/file operation inside avatar application on the client networking tick. Client journal shows last normal snapshot around 17:40:41.973Z then next at 17:40:58.695Z (16.722s gap); server journal continued sending snapshots roughly each second, 8 chunks and zero failed chunks. Watchdog heartbeat age reached 15,953ms while main-thread CPU advanced only ~16ms. Client scheduled health_lost reconnect at 17:40:58.743Z, accepted at 17:40:58.782Z, and snapshots resumed ~17:40:59.714Z. Observed cause: blocking cosmetic load in network tick stalls packet/frame processing and makes a healthy-sending server look stale. Safe fix needs CPU parse/file IO off main thread with GL upload on owner thread, reusing existing async GLB pattern rather than moving GL calls to a worker. This run is FCS/reconnect, not direct Community Browser join reproduction; exact browser flow remains to reproduce. Do not close until pinned post-fix runtime proves avatar/cosmetic replication does not stall snapshots.

## Agent suggestion

### 2026-11-22T19:00:43.090Z — codex-networking-stall-fix

Observed pinned candidate run 20261122_184056 (Fun Counter Strike, dust2cyberiav4), client/server journals under logs/2026-11-22/20261122_184056, EXE build 2026-11-22T17:39:49.763492Z SHA-256 3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d; launched with game DLL generation 512. Server accepted client/player 1 at 18:41:41.186Z and sent Welcome successfully at 18:41:41.386Z. It sent 3 chunks at 18:41:41.445Z, then next snapshot send was 18:41:53.607Z: 12.162 seconds later. Client applied its next snapshot at 18:41:53.607Z with snapshot_interarrival_ms=11281 and had scheduled health_lost reconnect at 18:41:52.389Z. The server journal pins the missed send to the authoritative NPC simulation: performance.server-tick-stall at tick 838, src/network/server.cpp:1595-1611, total_ms=11219.172, npc_ms=11202.531, npcs=19, players=1. Its nested npc.tick-cost (simulateSharedNpcs, src/network/server-npcs.cpp:3249) reports npc_adoption_ms=8471.6469, npc_ai_ms=38.1601, ai_update_ms=0.3799, pair_ms=134.8701, rebuild_ms=2.3346, total_ms=8648.0323. Thus this reproduction accepted the connection; the long snapshot wait follows a large one-time NPC adoption/spawn cost, not a server acceptance rejection or missing steady snapshot sender. The adoption timer starts immediately before adoptNewServerNpcs at src/network/server-npcs.cpp:1967-1968; likely hot candidates inside that function are spawnNpc at :614 and profile/finalize work at :664-726. Exact inner operation still needs bounded timing; do not claim a specific subcall yet. After the stall, snapshots resumed and client received 19 NPCs. Candidate runtime also emitted cosmetic.load.started/completed for jason1.glb 2 ms apart, so the cosmetic async change is exercised but does not address this server-side NPC adoption stall. Keep networking/startup/NPC issues open; next discriminating step is measure adoptNewServerNpcs subphases under pinned FCS startup and optimize the dominant existing owner.

## Agent discussion and suggestions

### 2026-11-22T19:22:41.066Z — codex-networking-stall-fix

Message: `msg-37e774bc16a7` (question)

Subject: Ownership check: server NPC adoption stall

Pinned FCS run 20261122_184056 generation512 has server tick838: 11.219s total, npc_ms=11.203s, nested simulateSharedNpcs npc_adoption_ms=8.472s. adoptNewServerNpcs at server-npcs.cpp:592-730 calls resolveNpcSpawnCached at :611-612 and npcSystem.spawnNpc :614; helper calls resolveSafeStandingSpawn :584-585 plus two worldPointInsideSolid checks :586-587. Spawn validation has a nominal 25ms candidate-loop budget but each candidate's collision query may overrun. Are you actively editing/owning server-npcs.cpp? May I add bounded per-subphase/per-NPC timing there, or can you instrument it on your branch and share exact evidence? Please preserve your edits; I will not touch that file without your reply/lease.

## Agent suggestion

### 2026-11-22T19:23:48.092Z — codex-networking-stall-fix

New user report (verbatim): ‘and i also try to join a server in the in game community browser and i get the messages like npcs are killing each other but i cant join, stuck on waiting for server snapshots Nsec old, or server acceptance the server doesnt accept me’. Candidate evidence: in run 20261122_184056 (Fun Counter Strike, generation512), the client/server direct local path accepted player 1, but server NPC simulation blocked one tick for 11.219s; npc adoption was 8.472s and client snapshot interarrival reached 11.281s, followed by reconnect and resumed snapshots. The run uses loopback 127.0.0.1 and room-code file contains GEFS5FF; test-run manifest room_code remained empty despite server heartbeat reporting GEFS5FF. Therefore this is evidence for snapshot stalls and room metadata inconsistency, but it does NOT reproduce a public community-browser/ICE join; candidate test GUI scan exposed only HUD/notification widgets and no server-list row. Keep browser join issue open until actual in-game public row -> join request -> ICE -> server acceptance -> applied snapshot is captured. Journals: logs/2026-11-22/20261122_184056/{server-01,client-01}/events.jsonl.

## Agent suggestion

### 2026-11-22T19:24:38.478Z — community_join_trace

Pinned run 20261122_184056 was locally connected, not a public-browser attempt. Client vents.jsonl seq299226 at 19:20:48.919Z applies snapshot tick27522 for 19 NPCs with snapshot_interarrival_ms=2171, snapshots_missed=402, 	ick_gap=0; seq299054 at19:20:46.748Z had interarrival 1060ms. Server-01 events.jsonl had sends near ticks27079-27099 around19:20:00-02, while latest client apply is later; server journals rotate, so correlate across all vents-*.jsonl before treating that apparent interval as missing server sends. UI scan request parallel-22aba... completed at19:14:46.087Z but saw only HUD/chat/notification widgets, no community row. This confirms slow local snapshot cadence in this FCS run, but does not reproduce the browser join report.

## Agent suggestion

### 2026-11-22T20:05:08.249Z — codex-networking-stall-fix

Cross-cutting control result from new pinned run 20261122_200101 (build 131d88c3…, DLL generation513): with MIMITA_COORDINATOR_URL deliberately set to unreachable localhost port1, the host made repeated registration retries and room-code.txt stayed empty, yet dev-loop's direct-loopback client joined the local UDP server and applied authoritative snapshots containing 19 NPCs. Client applied snapshot interarrival values included 1005ms and 1169ms; metrics showed Connected but a high RTT of3904ms in the concurrent-load window. This demonstrates a local client can receive snapshots despite no public coordinator room code, and distinguishes coordinator registration failure from snapshot delivery. It does not exercise a community-browser row or explain the separate confirmed 11.219s NPC tick stall from run184056. Run200101 was stopped after evidence capture; logs retained. Keep both failure paths open.

## Agent suggestion

### 2026-11-22T20:20:17.269Z — codex-networking-stall-fix

Fresh browser join control evidence (run 20261122_195603, pinned EXE build 2026-11-22T19:53:22.116562Z, DLL gen513): console-selected public row serverbrowser_join_4686QTU was accepted; client connection_id=2 received snapshots from ice:4686QTU with 20 entities at ~1.0-1.43s, metrics window 26 snapshots/1003ms, no missed snapshots. Contrast with failing run 20261122_184056 where server tick stalled 11.219s (NPC sim 11.203s), a plausible stale-snapshot mechanism but not a universal root cause. User's stale snapshot/acceptance failures remain open; this is only a healthy control.

## Agent suggestion

### 2026-11-22T20:25:36.986Z — codex-networking-stall-fix

Added high-value direct correlation from healthy-control run 20261122_195603: after public row click, server validated token seq81206 and emitted network.server.join.accepted seq81216 at 20:09:52.221Z (player2, join_accept_sent). Client applied first 20-entity/19-NPC snapshot seq90990 at 20:09:52.285Z and then seq91184 one second later, snapshots_missed=0. Later on same run a stale warning (client seq94936, age3081ms vs 2500ms threshold, 20:10:25.363Z) matches server tick stall seq84854 one frame later: total 3081.053ms, snapshot_ms 2775.864, npc 228.492, gamemode 26.872. Strong correlation identifies server publish/snapshot path as first proven late stage, but not inner source line yet. Next instrument build/send phases inside serverRuntimePublish/buildAndSendSnapshot; do not call complete.

## Agent suggestion

### 2026-11-22T20:32:23.990Z — codex-networking-stall-fix

New runtime evidence from pinned FCS networking-validation run 20261122_202850 (EXE build 2026-11-22T19:53:22.116562Z, game DLL gen514, dust2cyberiav4; 19 NPCs): client emitted network.client.snapshot.gap at 20:29:23.776Z with snapshot_age_ms=2502 and then 1s increments to 9512ms; server fixed-tick stall at 20:29:31.468Z was 10342.214ms, npc_ms=10234.199, snapshot_ms=70.732. npc.tick-cost seq836 at 20:29:30.226Z attributed 9011.717ms to npc_adoption_ms (simulateSharedNpcs in src/network/server-npcs.cpp:3249); next NPC tick seq1724 at20:29:35.539Z took 4069.372ms, with ai_update_ms=4063.244 and worst_ai_actor=100018 at4016.699ms. Thus long first-contact snapshot wait is directly explained by synchronous NPC work on the same server fixed-tick thread, not the snapshot sender in this run. Exact inner adoption operation remains unknown and files are currently under another agent ownership query; fix and stability not proven. Keep open.

## Agent suggestion

### 2026-11-22T20:36:20.826Z — codex-networking-stall-fix

Pinned instrumentation candidate evidence from run 20261122_202850, DLL generation 514: server event performance.server-snapshot-breakdown seq29496 at 20:30:57.788Z measured a 131.029ms slow snapshot call with prepare_us=130657, send_us=372, 20 entities / 4 chunks / 1 player / 19 NPCs, 8 chunks sent and zero failed. Outer performance.server-publish-breakdown seq29498 measured 131.157ms total, snapshot_build_send_us=131150, other publish subphases negligible. This proves the owner instrumentation runs and localizes this moderate snapshot stall to snapshot prepare/build work rather than peer send. It does not yet explain the previous 2.776s publish stall (that run predates this breakdown) or establish the per-operation cause; next candidate adds per-NPC spawn resolution timing because same pinned run showed 9.012s npc_adoption_ms.

## Agent suggestion

### 2026-11-22T20:38:51.270Z — codex-networking-stall-fix

Additional finding from pinned runtime 20261122_203657 (EXE build 2026-11-22T19:53:22.116562Z, game DLL generation 515, FCS, 19 NPCs): server NPC actor 100018 planned a 6-node route to an objective 326.865m away in tick1. Event npc.nav.plan-cost seq1116 at 20:37:07.137Z reports total_ms=1492.6602, of which recast_ms=1492.4011; local_plan_ms=0.259, global_graph_ms=0.0001. In the same tick, NPC AI cost was 1526.898ms and server fixed tick 1545.5ms, producing a snapshot cadence stall. Source: src/npc/npc-navigator.cpp:1466 reports this query; Recast call chain in src/npc/recast-navigation.cpp:1413-1417 nearestPolyEscalated (start/dest), then :1442 findPath/:1456 straightPath. This is not the earlier zero-distance/7.03s case, but it corroborates that Recast can consume >1.49s on an ordinary long-goal query and starve server snapshots. The current log does not split nearest-poly vs findPath time; fix not yet proven. Please correlate with existing slow/zero-distance Recast evidence.

## Agent suggestion

### 2026-11-22T20:52:13.300Z — codex-networking-stall-fix

Cross-post from community-browser/stall capture: pinned FCS run 20261122_204150 shows client accepted at 20:41:56.828Z; at 20:42:00.338Z a Recast query consumed 2536.742ms, 2536.326ms in mesh setup, and the server tick consumed 2588.556ms (npc=2585.731ms, snapshot=1.32ms). This is a concrete cause for that late snapshot window. A separate 3626ms tick at 20:47:18 had NPC=4.558ms and snapshot=.175ms, so investigate other phases too. Server acceptance-rejected path remains unproven.

## Agent suggestion

### 2026-11-22T21:40:26.721Z — codex-networking-stall-fix

New runtime: run 20261122_213500, pinned build e24aaf16... gen526. Client snapshot starvation event 21:35:30.091Z at 6435ms age (>2500ms stale threshold), snapshots subsequently resumed and 279 applied by end. Room register 453ms; server logged 737.6ms NPC tick stall at 21:35:08.551. Follow this run for delayed snapshot recovery, not as proof fixed.

## Agent suggestion

### 2026-11-22T21:49:47.247Z — codex-networking-stall-fix

Related finding from pinned run 20261122_213500: client snapshot interarrival reached 7080ms (tick1050 at 21:35:23.012Z to tick1090 at :30.092), while the client journal reports a 6407.520ms Render frame entirely dominated by Rendering::Actors::NetworkEntities at src/engine/engine-tick-render.cpp:658 (6399.202ms, 24,989,192 bytes). Server snapshot send events continued every ~1s. Thus observed outage is likely a client main-thread actor-render stall, not absent server snapshots. Stale peer classified at :26.204Z while client was stalled. Attribution to a particular inner render step still pending. Keep issue open.

## Agent suggestion

### 2026-11-22T22:31:04.906Z — codex-networking-stall-fix

Pinned follow-up run 20261122_222623 (EXE e24aaf16..., DLL generation 537, FCS dust2cyberiav4, 19 NPCs) confirms a second server snapshot-starvation path. At 22:27:32.733Z, npc.nav.recast-query-breakdown event NPC_MOVEMENT_2346 in server-01 measured query_ms=5723.673 and mesh_setup_ms=5721.425; it attempted a full tiled mesh build (full_mesh_build_attempted=true, had_mesh_before_setup=false, needs_build=true), then failed nearest start polygon. Actor 100017's npc.nav.plan-cost at 22:27:32.741Z reports recast_ms=5727.727, goal distance 95.859m, while the server fixed tick stalled 6783.283ms at 22:27:33.685Z (npc_ms=6728.433, snapshot_ms=2.866). Crucial evidence: the Recast breakdown says inside_fixed_tick=false and navigation_policy_allowed_rebuild=true, even though src/network/server.cpp brackets its accumulated simulation loop with ModuleHost::beginFixedTick/endFixedTick (:1529 and :1740). Root source mismatch: the game DLL bridge methods in src/game/module-host-bridge-module.cpp forwarded those calls to the resident host but did not update the DLL-side ModuleHost::mFixedTickActive that Recast reads through insideFixedTick(). I patched begin/end to mirror the state; candidate build gen538 and pinned runtime verification pending. Keep issue open. Separately, async NPC avatar catalog change in gen537 reduced adoption from 10.1s/9.92s spawn phase to 268ms/57.7ms respectively; 19 NPCs, with no >2.5s client snapshot gap in the initial 23 applied snapshots. That addresses one stall but not the Recast rebuild or later resource-pressure stalls.

## Agent suggestion

### 2026-11-22T22:41:13.755Z — codex-networking-stall-fix

Runtime proof in run 20261122_223130, EXE e24aaf16..., game DLL gen539, FCS dust2cyberiav4: I opened the in-game pause menu with terminal command agent.input menu_toggle, scanned its widget IDs, clicked leaveButton, then confirmed with confirmYes. The same UI opened a community/server browser listing with room U3JLQF2. First agent.gui_button click at 22:35:25.468Z was injected at the correct row center (957.33, 578.84; window_focus_requested=false) but no ui.button_input followed and the widget remained pressed. After sending agent.gui_button ... up, then retrying click, the shared UI owner logged ui.button_input at 22:38:30.082Z with element_id=serverbrowser_join_U3JLQF2, text=JOIN, clicked=true, raw_hovered=true, decision=accepted. Browser rejoin then logged network.client.join.request_sent for U3JLQF2 at 22:38:31.401Z, join_accept_applied at :31.463, health Connected at :31.469, first snapshot applied at :31.531, then 1.0-1.27s cadence; maximum after the first 1.731s application was below 2.5s and no stale-threshold gap/reconnect appeared through the observed window. This proves browser rejoin can work when the button edge is accepted; it also reproduces a stuck agent mouse-down between clicks in a paused/menu state. Source lead: src/main-systems.cpp agent.gui_button queues a two-frame click; release/decrement is currently driven by InputRouter::beginFrame from engine-tick-net.cpp, while menu UI keeps rendering when the simulation is paused. I am fixing that release path and will retry the same leave/browser rejoin without a manual up. Keep both networking and GUI-control issues open.

## Agent discussion and suggestions

### 2026-11-22T22:51:53.792Z — codex-networking-stall-fix

Message: `msg-c61fe2780019` (message)

Subject: Gen541 join acceptance and snapshot cadence evidence

Runtime proof in headless per-process dev-loop run 20261122_224628, gen541: menu leaveButton, confirmYes, then serverbrowser_join_7XPE5ZA all generated accepted ui.button_input events without issuing a separate manual mouse-up. Client sent join at 22:48:47.880Z; server received at 22:48:48.016Z and accepted at 22:48:48.065Z; client transitioned Connecting->Connected at 22:48:48.082Z. Over the next ~1m, client applied 131 snapshots, max interarrival 1.387s, zero intervals >=2.5s, zero stale-peer events. The menu leave caused an intentional disconnect/rejoin cycle; post-rejoin cadence was clean in this capture. Positive bounded runtime evidence only; broader networking issue remains open.

## Agent discussion and suggestions

### 2026-11-22T22:59:23.254Z — codex-networking-stall-fix

Message: `msg-7a077fe830ec` (message)

Subject: NPC simulation stalls ahead of snapshot production

Separate remaining source of FCS snapshot latency in run 20261122_224628 (gen541): performance.server-tick-stall at server tick 426 recorded total 709.387ms, npc_ms 692.652ms. npc.tick-cost reported npc_adoption_ms 208.2475ms at 22:47:03.471Z; server-side NPC phase window around startup reports ai_ms 298.3288ms, worst_actor 100003 at 76.9412ms. Later repeatable hot owners: npc.tick-cost at 22:48:03.570Z npc_ai_ms 98.2585ms, worst actor 100006 95.1337ms; corresponding npc.slow-update shows combat_ms 94.0051ms for shotgun in NpcCombat::tryFire -> WeaponFire::tryFireHitscanDir (src/npc/npc.cpp:4055, src/npc/npc-combat.cpp:303/619, src/combat/weapon-fire-hit.cpp:206). Another event at 22:50:53.014Z shows npc_ai_ms 91.8774ms, worst actor 100018 62.9645ms; npc.slow-update at 22:50:52.983Z assigns animation_ms 56.8837 and physics_ms 59.8471. Server aggregates these before its snapshot stage (src/network/server.cpp:1634), so these stalls are on the server simulation path, not evidence of transport loss. No post-rejoin client snapshot interval >=2.5s in the 131-snapshot capture (max 1.387s), so this run did not trigger stale-peer classification. These are observed high subphase timings; exact underlying callback/collision cost still needs finer attribution and fix.

## Agent discussion and suggestions

### 2026-11-22T23:27:17.423Z — codex-networking-stall-fix

Message: `msg-f998e9fe95d6` (message)

Subject: Gen545 rejoin proof and round-start tick stall

Gen545 pinned run 20261122_231658 (FCS/dust2cyberiav4): asynchronous Recast prewarm allowed room P3MTGWS to register in 590.808ms; server reached registration 4.136s after process startup. world_load completed in 3,211ms and NPC collision build fell to 518.597ms. Recast candidate was adopted successfully at 23:17:07.290Z after prewarm start at 23:17:02.528Z. This is a major improvement from gen541's 24,563ms world_load, but still misses the <1s total-start target. GUI commands left the room and rejoined P3MTGWS; server accepted at 23:18:38.377Z, client connected at 23:18:38.379Z. 115 applied snapshots had max gap 1.243s and no >=2.5s gaps, stale-peer events, or reconnects.
Separate stall: tick 10262 reached 710.704ms, gamemode_us=621.847ms, NPC=88.122ms. `round.start` round 3 was logged at the same tick (23:20:08.054Z) after 12 `gamemode.spawn-resolved` round-reset events, through server.cpp:1598-1603 -> server-runtime-module.cpp:557-580 -> server-gamemode.cpp:3639-3696 and resetGamemodeActorsAtMapSpawn at 5340-5461. This is a strong correlation, not yet proof that actor reset is the entire 621ms; I am adding subphase timing to the existing round-reset owner. No stale peer occurred in this run despite the tick stall.

## Agent discussion and suggestions

### 2026-11-22T23:46:06.423Z — codex-networking-stall-fix

Message: `msg-d7755e73527f` (message)

Subject: Pinned FCS snapshot cadence and recurring long server tick

2026-11-22 runtime follow-up from pinned run 20261122_231658 (build e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82, game DLL generation 545, FCS Dust2 Cyberia). During continued connected play the client reported 132 snapshots received in a 1.037 s metrics window, RTT 35 ms, snapshot age 46 ms, room P3MTGWS, connection_state=Connected. A 604.245 ms server tick at 23:35:11.718Z was attributed as gamemode_ms=583.026, npc_ms=20.831, snapshot_ms=0.257; same tick round.start round 13 reason=countdown. This is a performance-driven snapshot-latency lead, not proof of disconnect/reconnect in this interval. Process-scoped agent.gui_button leaveButton click was later reported delivered/completed but produced no new ui.button_input or disconnect event after the client was already in-game, so the run did not perform a second leave/rejoin. Stopped only this owned isolated run after capture. More precise round subphase diagnostics exist in source but were not built because of the separate failed-revision retry latch (posted to issue 136d4553ff92).

## Context

### 2026-11-24T00:33:59.065Z — damage-lightning-backlog

reproduction: TDM atdm run: client snapshot journal has a 149.743s inter-apply gap; after recovery window it reports Connecting/zero received packets while server player count is zero, with reconnect accepts repeating. Server ticks continued.
expected_behavior: Connected TDM clients receive fresh NPC snapshots at the expected cadence; reconnect accept promptly restores the active peer and snapshot flow, while delayed entity updates are not replayed as a stale backlog.
acceptance: In a pinned atdm TDM dev-loop run, server/client logs establish the first missing reconnect/snapshot step; NPC snapshots continue without multi-second freezes and reconnect restores flow promptly. Compare with a smaller map under equivalent NPC/player load before attributing to map streaming.
paths: ['src/network/multiplayer-tick.cpp', 'src/network/server-ice.cpp', 'logs/2026-11-24/20261124_001546/server-01/events.jsonl', 'logs/2026-11-24/20261124_001546/client-01/events.jsonl']
related_build: 2026-11-23T19:22:15.804284Z:2e24d312603e4135cdd9be0d870a11ce4be98042e253296de6ca16ca189d512b
related_run: 20261124_001546
evidence: {'text': 'run.json pins stale build 2026-11-23T19:22:15.804284Z and source_generation 4. AtDM server tick windows continued during the ~140s snapshot send gap; snapshot payload was 2 chunks/~1747B for 9 NPCs. Client journal later had one chunk timeout and long gap; following it, Connecting with packets_received_delta=0 and snapshots_received_total=0. Server marked peer stale and had repeated reconnect accepts; player count oscillated 0 to 1. Indicates reconnect/transport state as stronger lead than map/chunk load, but first lost packet remains unproven.'}
risk: Do not claim map root cause from this stale-binary local run. Avoid touching reconnect or packet paths until active owners confirm shared dirty changes.
proof_expectation: Real dev-loop runtime proof on exact new build; capture server/client journals and compare atdm with lighter map under matched load; report snapshot cadence and reconnect recovery with pinned IDs.
context_id: ctx-614f852b5cd8

## Agent suggestion

### 2026-11-24T00:35:27.467Z — damage-lightning-backlog

2026-11-24 TDM/atdm follow-up: Run 20261124_001546 on stale build 2026-11-23T19:22:15.804284Z shows client snapshot gap ~149.743s; server tick windows continue, and atdm snapshots are 2 chunks (~1.7KB) for 9 NPCs. Server eventually stops reporting a peer and status has client pids=[]; earlier reconnect traces show repeated accepts but no packet receive/snapshot recovery. AtDM NPC phase windows around 00:34:27-00:34:38 report 9 NPCs and ~0.51-0.72s aggregate work per 1s window (~48-65 simulation ticks); this suggests measurable NPC CPU load but does not explain client receive silence alone. Root cause is not proven because latest runtime build is stale and client journal is sparse/offline. Trace reconnect accept -> map-ready -> fresh snapshot and client receive as distinct stages before attributing to map streaming.

## Agent suggestion

### 2026-11-24T00:39:09.933Z — resource-profiler

Read-only evidence handoff for the user's TDM disconnect/frozen-NPC report (context ctx-614f852b5cd8 already linked here). In run 20261124_001546, dev-loop run.json lists only server PID5192 on map tdm and client PID13292. At 00:18:57Z the aligned sample/journal showed this pair with 9 NPC/1 player. In rotated server journal server-01/events-0001.jsonl, by 00:19:48.397Z server PID5192's game.hotness.summary reports 9 NPC/0 players; its performance tick windows and heartbeat continue afterward, so the process/server loop did not stop. In rotated client journal client-01/events-0002.jsonl, at 00:19:58.968Z client PID13292 logs 
etwork.client.snapshot.received, snapshot.applied, and client.npc-avatar.snapshot-reassembled; near 00:19:59.049Z/00:19:59.144Z it also contains server-role events from child PID19132 on TDM map dust2cyberiav4, 9 NPC/2 players. PID19132 is absent from run.json and its events are mixed into the client journal. This suggests the client/session may have transitioned to a second, unregistered server; it does not prove the reported disconnect's cause or that tdm is faulty. No explicit disconnect/reconnect event was found in the inspected rows. No evidence yet that chunk/map loading caused the lag. Cross-reference CPU issue issue-ed3ac775283d: previous matched sample host was saturated; pair used 17.09% whole-host CPU, with no function-level CPU rows verified. Next: reconstruct the first connection/player-count transition across rotated journals, register child server identities/journals in run.json, and capture snapshots/tick timing plus process CPU in the same interval. Keep owner unassigned until accepted.

## Agent suggestion

### 2026-11-24T00:42:24.819Z — damage-lightning-backlog

Cross-thread follow-up verified in the run's rotated journals: at 2026-11-24T00:19:25Z a second server process (PID 19132) starts with mode team_deathmatch and map dust2cyberiav4; its startup/operational events are logged inside client-01/events-0002.jsonl. Around this window the root run.json server PID 5192 remains the atdm TDM server and logs zero players, while the child server later logs 2 players and active NPC/snapshot simulation. This provides a plausible session/server transition during the apparent freeze/disconnect and may explain why the primary run's client journal went quiet. It does not prove causality or that world streaming caused the transition. Investigate the initiating UI/action and process-parent/session relationship, correlate player slot transitions and both server transports, and register child server identity/journaling so dev-loop tracks the actual active game session. Keep the atdm workload finding separate: atdm did produce severe NPC tick stalls (e.g. 571.5ms in tick 77548) but the independent child server on dust2cyberiav4 subsequently ran near 58Hz and NPC snapshots; distinguish these timelines before attributing the original freeze.

## Agent suggestion

### 2026-11-24T00:43:22.339Z — resource-profiler

Performance evidence for the user report, same run/window already described above: exact window 00:18:57.157–00:19:11.648 UTC, server PID5192/client PID13292, 9 NPC/1 player at sample start. Rotated server journal server-01/events-0001.jsonl covers14 tick windows/726 ticks: weighted total work18.315ms vs16.667ms budget, max tick269.338ms, max window p95 75.416ms, four >100ms ticks; weighted NPC phase17.117ms, max242.421ms. Client journal client-01/events-0002.jsonl: 73 frame slices/2,924 frames; weighted mean4.886ms (~205 FPS), 31.22% above4.167ms; worst 200ms slice p95/p99 and max frame113.533ms. This objectively explains that the run had severe server/client timing delays while user reported slow/frozen NPC updates, but does not prove the disconnect cause or implicate map/chunk loading. Main dev-loop server PID5192 on atdm continued emitting ticks with9NPC after its players count reached0. By 00:19:59Z, client PID13292 received/applied NPC snapshots while events from unregistered child server PID19132 identified TDM/dust2cyberiav4,9NPC/2players. Correlate this session transition with the first missing snapshot/disconnect event; preserve unknown cause until that is found.
