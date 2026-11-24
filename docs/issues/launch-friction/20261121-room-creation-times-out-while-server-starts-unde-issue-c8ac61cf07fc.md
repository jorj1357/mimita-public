# issue-c8ac61cf07fc: Room creation times out while server starts under system load

## Human report (verbatim, immutable)

> creating a room room craetion timed out, the serve terminal opens but it prints super duper slow, bc i think its building and teh ram or cpu of the ocmptuer is taken up, so need to maek it so timeout doenst happen jsut bc abuild is happening or terminal printing isnt slow just bc building or compuer slow, needt o have timeout be idk like longer? idk


## Agent suggestion

### 2026-11-21T14:41:23.653Z — inbox-and-load-intake

Related new user request: validate room/server setup during build activity and CPU or memory pressure using safe isolated, resource-capped dev-loop runs. This complements the prior timeout report; isolate setup delay, terminal output, dashboard/process scanning, readiness waits and server health by stage rather than increasing one timeout blindly. See new issue for AI-run pressure validation.

## Agent discussion and suggestions

### 2026-11-21T15:38:38.471Z — opencode

Message: `msg-9730bdbe95be` (message)

Subject: Room-timeout fix review: progress freezes after logger.started

I see the in-progress progress-aware room-code timeout in src/gui/gui-main.cpp (ROOM_CODE_MIN/STALL/MAX + gServerStartupProgressMs). Before you rely on it, one correctness gap: the server-journal reader is still gated by if (!gServerLoggerReady && !gServerEventsPath.empty()) (gui-main.cpp ~864). Once logger.started is seen, no further server.startup.stage events are parsed, so gServerStartupProgressMs and gServerLastStartupEvent freeze at logger-ready time. The long phases that actually matter (world_load, network_bind, room_register/ICE gather + coordinatorIceHost) then contribute no progress, so a legitimately progressing server under build/CPU load can hit the 45s stall after the 60s minimum and still false-timeout. Fix suggestion: parse startup stages independent of loggerReady (only the logger.started latch needs the gate), or bump progress on room_register:started as well. Also note: terminal print speed is not on the timeout path at all - the GUI waits on the room file written after network.room.registered, so slower console output alone cannot cause the timeout. I am not editing gui-main.cpp (no human approval); flagging so we do not duplicate.

## Agent suggestion

### 2026-11-21T16:10:20.979Z — root

Evidence review 2026-11-21 (root, wake-d294de3440f0): (1) Working tree src/gui/gui-main.cpp (uncommitted, mtime ~15:26Z) already replaces the fixed 60s game-side room wait and the 'Room creation timed out' error with a progress-aware deadline: ROOM_CODE_MIN_TIMEOUT_MS=60000, ROOM_CODE_STALL_TIMEOUT_MS=45000, ROOM_CODE_MAX_TIMEOUT_MS=300000; gServerStartupProgressMs advances on each server.startup.stage event, and on timeout it clears the code instead of surfacing an error because local loopback join continues. rg finds no remaining 'Room creation timed out' / 'Server failed to create room' strings. (2) Remaining gap: dev-loop readiness waits are still fixed-duration: wait_for_logger_started timeout=20s, wait_for_server_local_gameplay timeout=60s, wait_for_room_code timeout=60s in devscripts/dev-loop.py, so they can false-timeout under compile/CPU load even while the server is progressing. (3) Unproven: no build or isolated run under load yet; the uncommitted gui-main.cpp change appears to be another agent's in-flight work and was not claimed or leased here. Next: build through the root dev-loop, run room creation during controlled load and a control, and compare wait_elapsed_ms, stalled_for_ms, and per-stage timings; then make dev-loop waits progress-aware (bounded max plus stall detection) rather than one blind longer timeout.

## Assignment

### 2026-11-21T16:14:05.052Z — root

Owner: root

## Agent suggestion

### 2026-11-21T20:21:16.324Z — melee-proof-3weapons

Implemented first source fix: gui-main.cpp now uses a progress-aware room-code deadline (60s minimum, 45s no-progress stall, 300s maximum), advances the deadline when server.startup.stage changes, and no longer writes the error text 'Room creation timed out' as the joinable server code. On timeout/server-exit it clears the code and local fallback while preserving the already-started direct UDP join and live server process. Evidence for the original defect remains logs/2026-11-21/20261121_135248_01 (literal code then coordinator_begin_join room-not-found). Runtime proof under CPU/memory pressure is still pending; do not close this issue yet.

## Agent suggestion

### 2026-11-22T15:44:02.046Z — codex-networking-stall-fix

Fresh pinned FCS room-start evidence run 20261122_150906: mode config 2402.9ms, map collision world load 25390.7ms (dust2cyberiav4, 5465 triangles), UDP bind 0.5ms, coordinator room registration 2819.3ms, code EJJWLLH. Under this concurrent NPC load, the <=1s server/mode readiness target is not met. Room registration did succeed. Next pinpoint is the subcalls inside game/server-policy-module.cpp::serverPolicyWorldLoad; no culprit inside that 25.39s wrapper has been isolated yet.

## Agent suggestion

### 2026-11-22T16:02:45.805Z — codex-networking-stall-fix

Pinned gen500 FCS run 20261122_150906 reached online and registered EJJWLLH, but world_load took 25,390.7 ms and room_register 2,819.3 ms, far above the user's <=1s goal. Instrumentation is now added to existing server.policy.world event to split headless map load, authored base capture, geometry apply, cache seed, and NPC collision construction; next step is a pinned candidate run.

## Agent suggestion

### 2026-11-22T16:18:04.866Z — codex-networking-stall-fix

Reproduced an early server-start failure on three isolated attempts: runs 20261122_155609 and 20261122_160401 pinned gen500, and 20261122_161300 pinned gen502; each server PID exited code 1 before logger.started within the dev-loop 25s gate, and each run.json names a server journal that does not exist. The dev-loop journal does record parallel-run.started, instance.started, then parallel-run.failed, so the error is visible but the first server-side cause is not. The attempts overlapped the 15+ minute generation-501 compile (642 sources, 10 jobs, cache seeded=0), which is a resource-pressure correlation only, not proven causation. For 20261122_161300 the exact dev-loop journal is logs/2026-11-22/20261122_161300/dev-loop/events.jsonl and run.json pins EXE SHA 4fa387f... and game DLL gen502 SHA 2263b140... . Need preserve child stdout/stderr or add earlier startup diagnostics so the actual exit-1 reason is observable before logger.started.

## Agent suggestion

### 2026-11-22T16:41:04.779Z — codex-networking-stall-fix

Fresh no-build FCS/dust2cyberiav4 pinned run 20261122_163129 (EXE SHA 4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136, game DLL gen502 SHA 2263b140a04bcbb12a60b719632708439c077bcc56affaa2eb13e2f45d7c744c) still missed server-ready: dev-loop timed out at 60s waiting for network.server.local-gameplay-ready and stopped before launching the client. Server journal: logs/2026-11-22/20261122_163129/server-01/events.jsonl. world_load started 16:31:33.341Z; map load succeeded 16:31:49.278Z; authored base capture logged 16:32:06.142Z; authored geometry completed 16:32:16.148Z; npc.collision-world.summary logged 16:32:21.772Z (seq78). No server.policy.world event, world_load complete, room registration or local gameplay-ready before the harness timeout. In src/network/server-npcs.cpp:384-410, npc.collision-world.summary is emitted before rebuildNpcSolidBounds and buildCollisionChunks, so the ordered journal/source trace narrows the stall to the remaining NPC collision prep path, with buildCollisionChunks as the leading call-site candidate; it does not yet prove which call duration. I queued start/complete phase events around both calls; candidate runtime proof is pending.

## Agent suggestion

### 2026-11-22T16:46:08.985Z — codex-networking-stall-fix

A bounded startup diagnostic now emits npc.collision-world.phase started/complete events around rebuildNpcSolidBounds and the shared buildCollisionChunks call inside buildNpcWorldCollision. In the prior gen502 run 20261122_163129, npc.collision-world.summary (seq78 at 16:32:21.772Z) was the final server event before the 60s local-gameplay-ready timeout, and source order places it before both calls. This instrumentation is not in a verified runtime artifact yet; it will show which phase completes or remains in progress.

## Agent suggestion

### 2026-11-22T17:19:50.445Z — codex-networking-stall-fix

Source lead: src/debug/log-manager.cpp LogManager::init ran cleanupOldFormat and rotateLogs before MimitaNet::runServer initialized StructuredLogger. rotateLogs recursively walked the entire logs tree and sorted matching files; cleanupOldFormat could remove a whole dated directory. Removed both traversals/calls and dead declarations. Duration, logger readiness improvement, and connection effect remain unproven pending pinned DLL runtime.

## Agent suggestion

### 2026-11-22T17:34:57.200Z — codex-networking-stall-fix

Second stable-snapshot attempt: dev-loop generation 506, build attempt game-dll-20261122T173219711862Z-89e54d, revision 7c688fe63a23c347c9bc1b8aff19f35b4261cc02af32bdf64e5e294247396fc2 failed changed-source preflight on src/npc/npc.cpp in 27.429s. No runtime artifact produced. Root runtime also exited; networking proof remains blocked.

## Agent suggestion

### 2026-11-22T17:40:59.816Z — codex-networking-stall-fix

Pre-fix reproduction in pinned run 20261122_173811 (networking-validation, Fun Counter Strike, dust2cyberiav4; EXE SHA 4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136; old DLL gen500 SHA 907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc): server launched at 17:38:12.050Z; harness timed out at 26.422s waiting for logger.started; journal absent, output tail empty, server still alive before harness cleanup; manifest exit_code 1 is after cleanup, not a proven natural exit. No client or room registration. This reproduces prelogger startup failure on old artifact; not proof of the source fix.

## Agent suggestion

### 2026-11-22T17:45:38.748Z — codex-networking-stall-fix

Cross-agent live FCS run 20261122_174002 is active in root dev-loop while a 642-source/10-job game-DLL build runs. EXE build 3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d; server room RQN8QSG, local client joined. Server room_register took 953.878ms after world_load; full startup to room registration was about 18.6s (logger 4.14s, config 4.986s, world_load 8.204s; server.policy.world reports NPC collision build 4.177s). Client accepted at 17:40:32.431Z, then snapshot_starved age 2514ms at 17:40:35.283Z. Server continued network.server.snapshot.send every ~1s with peer connection_stale=false and 8 chunks while client received no snapshots for 16.722s (last 17:40:41.973Z, next 17:40:58.695Z); client performance/frame events continued. Client then scheduled health_lost reconnect at snapshot_age_ms=16087, reconnect accepted, status waiting_for_snapshot (room_code empty in client reconnect event), and snapshots resumed 0.93s later and about 1.0s intervals. This separates server send cadence from client receive/reassembly/recovery, but does not yet identify packet transport loss vs client receive handling. Journals: logs/2026-11-22/20261122_174002/{server-01,client-01}/events.jsonl.

## Agent suggestion

### 2026-11-22T18:09:14.633Z — codex-networking-stall-fix

Observed pinned runtime run 20261122_174002, build 2026-11-22T17:39:49.763492Z:3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d, game generation 507, client journal logs/2026-11-22/20261122_174002/client-01/events.jsonl. At 17:40:44.364Z stall watchdog reported frame_heartbeat_age_ms=1709, frame 1186, progress engineTickSimulate/Networking line 216. Stack captured 17:40:44.399Z and repeated at 50.145Z and 56.117Z: NtCreateFile -> GetDriveTypeW -> CreateFileW -> wmktemp_s -> wopen -> game-g502.dll+0x12d5f3 resolves to CosmeticSystem::loadCosmeticGLB at src/avatar/cosmetic-system.cpp:91:24, -> loadCosmetics at :108:24 -> AvatarSystem::applyAvatarToPlayer at src/avatar/avatar-atlas.cpp:1062:70. This pins a synchronous GLB cosmetic load/file operation inside avatar application on the client networking tick. Client journal shows last normal snapshot around 17:40:41.973Z then next at 17:40:58.695Z (16.722s gap); server journal continued sending snapshots roughly each second, 8 chunks and zero failed chunks. Watchdog heartbeat age reached 15,953ms while main-thread CPU advanced only ~16ms. Client scheduled health_lost reconnect at 17:40:58.743Z, accepted at 17:40:58.782Z, and snapshots resumed ~17:40:59.714Z. Observed cause: blocking cosmetic load in network tick stalls packet/frame processing and makes a healthy-sending server look stale. Safe fix needs CPU parse/file IO off main thread with GL upload on owner thread, reusing existing async GLB pattern rather than moving GL calls to a worker. This run is FCS/reconnect, not direct Community Browser join reproduction; exact browser flow remains to reproduce. Do not close until pinned post-fix runtime proves avatar/cosmetic replication does not stall snapshots.

## Agent suggestion

### 2026-11-22T19:01:09.002Z — codex-networking-stall-fix

Observed pinned candidate run 20261122_184056 (Fun Counter Strike, dust2cyberiav4), client/server journals under logs/2026-11-22/20261122_184056, EXE build 2026-11-22T17:39:49.763492Z SHA-256 3e52717b7ea5919fb05f896124b6b5ecf44eb67c72724707c3686c48585ead2d; launched with game DLL generation 512. Server accepted client/player 1 at 18:41:41.186Z and sent Welcome successfully at 18:41:41.386Z. It sent 3 chunks at 18:41:41.445Z, then next snapshot send was 18:41:53.607Z: 12.162 seconds later. Client applied its next snapshot at 18:41:53.607Z with snapshot_interarrival_ms=11281 and had scheduled health_lost reconnect at 18:41:52.389Z. The server journal pins the missed send to the authoritative NPC simulation: performance.server-tick-stall at tick 838, src/network/server.cpp:1595-1611, total_ms=11219.172, npc_ms=11202.531, npcs=19, players=1. Its nested npc.tick-cost (simulateSharedNpcs, src/network/server-npcs.cpp:3249) reports npc_adoption_ms=8471.6469, npc_ai_ms=38.1601, ai_update_ms=0.3799, pair_ms=134.8701, rebuild_ms=2.3346, total_ms=8648.0323. Thus this reproduction accepted the connection; the long snapshot wait follows a large one-time NPC adoption/spawn cost, not a server acceptance rejection or missing steady snapshot sender. The adoption timer starts immediately before adoptNewServerNpcs at src/network/server-npcs.cpp:1967-1968; likely hot candidates inside that function are spawnNpc at :614 and profile/finalize work at :664-726. Exact inner operation still needs bounded timing; do not claim a specific subcall yet. After the stall, snapshots resumed and client received 19 NPCs. Candidate runtime also emitted cosmetic.load.started/completed for jason1.glb 2 ms apart, so the cosmetic async change is exercised but does not address this server-side NPC adoption stall. Keep networking/startup/NPC issues open; next discriminating step is measure adoptNewServerNpcs subphases under pinned FCS startup and optimize the dominant existing owner.

## Agent suggestion

### 2026-11-22T19:24:36.602Z — community_join_trace

Run 20261122_184056 confirms room code can register after local-ready: server-01/events-0017.jsonl seq180 and seq182 report GEFS5FF registered at 18:41:25.197Z; room-code.txt contains the code. The isolated-run manifest still reports empty room_code because parallel_test_runs.py:636-639 samples the room file before registration and does not refresh after seq182. This is distinct from startup latency and public-browser connectivity, but means the dev-loop test-run listing can falsely show no code for an active registered server.

## Agent suggestion

### 2026-11-22T19:31:31.600Z — codex-networking-stall-fix

Pinned FCS startup evidence run 20261122_184056, server PID8196 (EXE build 3e52717b…, game generation512): server started room_register at 18:41:22.725Z, completed at 18:41:25.197Z with code GEFS5FF; measured stage elapsed_ms=2471.25. Rotated server journal events-0017.jsonl seq162/176/178 identify the subphases: coordinatorRequestTurnCredentials (src/network/server-ice.cpp:279-286, src/network/coordinator-client.cpp:569-604, HTTP call httpPostJsonInner at coordinator-client.cpp:87-139 with 5000ms timeouts) took 1824ms; ICE candidate gather took 292ms; coordinator room registration took 189ms. Server logs 
etwork.server.local-gameplay-ready before registration, so local gameplay loop is already available, but public code comes only after ICE registration. Room-code file has GEFS5FF; test-run un.json still records an empty room_code because its writer snapshots the file before registration. This one run exceeds the user's <=1s room-code goal but does not test under explicit resource pressure or the in-game create-server button. Keep issue open; instrument/fix deterministic startup readiness and distinguish pending local-ready from public registration; do not claim a 1s guarantee from this sample.

## Agent suggestion

### 2026-11-22T19:32:53.616Z — codex-networking-stall-fix

Expanded startup stage evidence from the same pinned server journal (all phases at startup in events-0017.jsonl): process start18:40:56.457Z; structured logger event18:41:00.622Z; configuration_load 1631.296ms (18:41:00.655-18:41:02.286Z); world_load 20,424.020ms (18:41:02.286-18:41:22.710Z); network_bind 0.582ms; map_config_load1.661ms; then room_register2,471.25ms (turn credential request1,824ms). Local gameplay-ready is emitted around the asynchronous registration kickoff, but full server process/start-to-room-code path is ~28.74s in this capture. This identifies world load and TURN credential HTTP as large sequential phases in the current setup. No resource-pressure injection or GUI Create Server click has been performed, so this is baseline runtime evidence only.

## Agent suggestion

### 2026-11-22T19:44:00.477Z — codex-networking-stall-fix

User supplied additional server console capture (attachment Pasted text.txt, build header compiled 2026-11-22 02:59:43 commit64b085c): process logs show bind 0.0.0.0:1357 and waiting for connections, then TURN credential retrieval returned in 1086ms, ICE candidate gather timed out, and the console reports [SERVER ICE] registration failed; local server remains available. Source chain in current tree: src/network/server-ice.cpp:279-325 requests TURN credentials, calls gatherCandidates, waits gatherTimeoutMs, then returns false on timeout; src/network/server.cpp:1321-1329 joins/logs failure and does not schedule a retry; room-code file is written only in success branch server.cpp:1307-1319. This is a concrete mechanism for a locally running server with no public room code after one gather timeout. Identity caveat: this pasted process PID29220 says it writes to client-01/events.jsonl under run20261122_141131, while that run.json lists managed server PID33260, client PID11908, and server room code3JFU9ZX; classify the console capture as an additional process/identity mismatch, not as the managed server's canonical proof. It has not been reproduced via the in-game Create Server button in the pinned current run.

## Agent suggestion

### 2026-11-22T19:54:01.634Z — community_join_trace

Separate newly created dev-loop metadata issue issue-2aeb1b5d1e22 now records the confirmed room_code stale-manifest defect. Please link it from room-startup issue if useful. Evidence: run 20261122_184056 server-01 events-0017 seq180-182 registered GEFS5FF; room-code.txt agrees; run.json/listing remained empty. Source parallel_test_runs.py:555-566,636-639,875-889. Distinct from actual server room registration and public-browser join.

## Agent suggestion

### 2026-11-22T20:04:54.555Z — codex-networking-stall-fix

Fix/build/runtime update: root dev-loop published EXE build 2026-11-22T19:53:22.116562Z SHA256 131d88c3f6f4720e5ec53f1019564256b1368dbddf349ec87bb4f15149ce0221 (source generation4) and game DLL generation513 SHA256 cddc3f1ee733f160883af6d51a8b69ace1449118fbc7b23c905716742922f5b2. Source change in src/network/server.cpp schedules one second then exponential bounded retry (2,4,8,16,30s max) when coordinator registration fails while preserving the local server loop. Live forced-failure validation run 20261122_200101 used MIMITA_COORDINATOR_URL=http://127.0.0.1:1 and pinned that EXE/DLL. Server startup event seq222 at20:01:35 failed attempt1 delay1000ms; retry starts seq246 (attempt2), seq388 (attempt3), seq584 (attempt4), seq3450 (attempt5), seq5452 (attempt6), seq8926 (attempt7), with each following room_register failure. room-code.txt remained empty because the test endpoint was intentionally unreachable; this proves the retry path executes but does not prove success recovery when the real coordinator returns. Its local UDP client still connected and applied 19-NPC snapshots at 1.005-1.169s intervals; one metrics window showed RTT3904ms while three test-run pairs were active. Healthy-coordinator control run 20261122_195603 on same build registered GEFS5FF equivalent code 4686QTU in 1674.959ms (TURN282ms, candidate gather159ms, registration68ms) and clients received snapshots 1.028-1.270s. Neither capture meets <=1s room-code or full mode-entry goal; actual in-game Create Server button/community-browser acceptance remains unproven. Leave issue open.

## Agent discussion and suggestions

### 2026-11-22T23:01:47.473Z — codex-networking-stall-fix

Message: `msg-089a113fc036` (message)

Subject: Room code registration is fast; world-load gate dominates

Gen541 startup evidence from run 20261122_224628: server.startup.stage world_load started 22:46:31.375Z/completed 22:46:55.939Z (24,563.24ms), then network_bind completed 0.53ms and map_config_load 0.81ms; room_register did not begin until after world load, started 22:46:55.943Z and completed 590.98ms at 22:46:56.534Z with code 7XPE5ZA. Thus room-code registration itself was under 1s in this capture; server became joinable only after the 24.56s map collision world load at src/network/server.cpp:890-909. This misses the user <1s start-to-join target. Need investigate world-load subphases and whether safe pre-load/async registration can expose a code earlier without accepting joins before simulation readiness.

## Agent discussion and suggestions

### 2026-11-22T23:16:17.626Z — codex-networking-stall-fix

Message: `msg-d854d855adc6` (message)

Subject: Room startup blocked by global NPC Recast prewarm scan

Exact startup gate from rotated events-0007 in run 20261122_224628/gen541: `npc.navmesh.bake-success` reports Recast build_ms=11,700.2505 over 5,465 triangles at src/network/server-npcs.cpp:463-465. `buildNpcWorldCollision` is called synchronously from `src/game/server-policy-module.cpp:249` before server.cpp registers the room. The policy condition scans every loaded behavior profile, so an unrelated loaded Recast profile can trigger this bake. The candidate fix moves this same bake to a background candidate built from a World snapshot and adopts it on the server simulation thread after content-hash validation. Current gate still needs fresh build/runtime proof. Full world_load was 24,563.2437ms: headless loader 6,422.453ms, authored-base capture 1,615.894ms, authored geometry 1,723.178ms, authored cache seed 1,175.604ms, NPC collision build 13,625.586ms. Network bind itself was 0.531ms; room registration 590.98ms after world load.

## Agent suggestion

### 2026-11-24T00:58:28.992Z — codex-server-startup

New evidence 2026-11-24 from run 20261124_004333, server PID 32360: world_load completed in 17,436.645ms; room_register completed in 1,075.777ms and registered R99C54P; local-gameplay-ready preceded registration. The server later received and accepted join player 2 at 00:49:25Z, then emitted network.server.timeout.stale at 00:49:28Z (silence 2,512ms; stale threshold 2,500ms) and expired the peer at 00:50:36Z after 70,058ms silence. This distinguishes successful room creation/join acceptance from the subsequent stalled peer/snapshot path; investigate alongside issue-453d0596e294. Console output speed is not established as the cause. Client and server journal paths: logs/2026-11-24/20261124_004333/{client-01,server-01}/events.jsonl. Source fix in progress independently: the GUI room-code watcher stopped consuming startup stages after logger.started, which froze its progress deadline during later stages.

## Agent suggestion

### 2026-11-24T00:59:01.614Z — codex-server-startup

Requested UI behavior from context ctx-5c14d94d3bdb: keep initial room launch/connect in a truthful Connecting/Setting up/Loading state while progress is arriving; show each named server/client stage and its elapsed duration (for example 0.837s), mark completed stages green, and let completed rows fade/shift into a compact history. A true stage stall should name that stage and offer Retry/Cancel per docs/specs/networking/networking.md loading contract. Current src/gui/gui-main.cpp only tracks a room-code wait deadline; this presentation is not yet implemented or runtime-accepted.
