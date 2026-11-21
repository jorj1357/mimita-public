# issue-55bd775c8026: Client crashes while connecting to a new dev-loop server in another mode

## Human report (verbatim, immutable)

> as well as an issue where starting the game thru dev loop gui, leaveing the room i started in, starting a new sever new mode, i crash on connecting, this also can u add as a issue and investigatae and ensure AI can doit full autonomosuly and if not, then suggest gaps we need to fix liek where ai doesnt know about somethign that it would benefit from knowing about


## Triage

### 2026-11-20T06:32:34.736Z — juggernaut-npc-stuck-shooting

Summary: Reproduce the GUI-driven sequence (start in initial room, leave, start a new server in another mode, connect) and trace process/session ownership, mode selection, artifact identities, connect lifecycle, and first crash event across the exact per-process journals.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent discussion and suggestions

### 2026-11-20T06:33:00.845Z — juggernaut-npc-stuck-shooting

Message: `msg-268e2514b485` (ask)

Subject: Review new-mode GUI connection crash and autonomy gaps

Please review issue-55bd775c8026 against the dev-loop launch/process/connect path. Suggest how to reproduce and isolate the first failure using launch IDs, executable/game artifact identity, process manifests, and per-process journals. Also note any concrete context/tool/evidence gap that would prevent an agent from completing this diagnosis autonomously (see issue-91e3384b219b). Review only; do not edit source.

## Agent suggestion

### 2026-11-20T06:45:16.007Z — juggernaut-npc-stuck-shooting

Observed candidate GUI launch log logs/2026-11-20/20261120_063206/dev-loop/events.jsonl, request dashboard-d14f90e4: server process/logger started, then the locally-playable readiness gate timed out after 60 seconds; run.json records only server-01, build_match=false, and no client instance. Thus this is not yet evidence of the reported client-connect crash. The recorded argv has duplicate --mode values (ssbm then payload); correlate against the user's exact new mode before deciding whether this is the same sequence or a separate launch configuration issue.

## Agent suggestion

### 2026-11-20T06:51:23.466Z — juggernaut-npc-stuck-shooting

Available nearby GUI evidence does not reproduce the user's generic connect crash: run 20261120_063206 request dashboard-d14f90e4 spawned a server but no client and timed out waiting for readiness; 063641 and 064201 have client snapshots/spawn applied. The 063206 command also contained duplicate --mode ssbm then --mode payload. Need trace user's leave-room/new-server action from GUI request ID through client process creation, transport, snapshot/spawn, and process exit/crash artifact. Preserve exact mode/map values and pin artifact/build ID.

## Agent suggestion

### 2026-11-20T06:52:21.655Z — review-connect-crash-autonomy

Recent evidence does not yet reproduce a process crash. Run 20261120_063206 (request dashboard-d14f90e4, mode 57/Payload, funworld3) started the server but timed out at server_gameplay_readiness after 60s and never created a client. Run 20261120_063800_01 (dashboard-b2da2248, same mode/map/build) started the client, but launch report recorded client_connect_timeout after 45s; its client journal later shows a second connect request and repeated complete snapshots, so this is a delayed/retried connection, not proof of a crash. The 06:51 report (dashboard-c4d45252) failed client_reconciliation because prior PID 22852 remained alive; it also does not prove a crash. Automation gap: .dev/launch-reports currently omit run_id, mode/map, server/client journal paths, and launched PIDs even though events.jsonl has them; report should join request_id to event log and materialize those fields, then gather client exit code/crash artifacts to distinguish crash from timeout/retry. Need exact user's GUI request/run ID or a newly captured reproduction before attributing root cause.

## Agent suggestion

### 2026-11-20T06:53:41.246Z — juggernaut-npc-stuck-shooting

Peer review narrowed nearest evidence: dashboard request dashboard-b2da2248, launch 2026-11-20T06:38:00.795Z, run 20261120_063800_01 used Payload/funworld3 and build 2026-11-20T05:38:11.772319Z. launch.json recorded client_connect_timeout at 45s for client PID 22852; run.json later marked client stale and its journal contains later repeated snapshots/connection attempts, so this is a failed readiness/connect handshake, not a proven process crash. At 06:51 request dashboard-c4d45252 then failed reconciliation because that same PID remained alive. No user-matching crash artifact or exact old→new mode sequence is identified.

## Agent suggestion

### 2026-11-20T06:55:14.323Z — juggernaut-npc-stuck-shooting

Closer read of client-01 journal in run 20261120_063800_01: at 06:38:43 it requested/connect-preflighted to new server 127.0.0.1:55589 on funworld3; launch report later marked client_connect_timeout. At 06:40:47 the same PID requested a new connection to 127.0.0.1:1357 on badhouse, and at 06:40:52 applied a complete snapshot/spawn, with snapshots continuing. This is a delayed/replaced connection and readiness failure, not evidence of client process crash. Exact old→new GUI flow remains unverified; inspect request/run stage chronology and user-visible window identity before assigning root cause.

## Agent suggestion

### 2026-11-20T07:55:10.096Z — launch-latest-stable-fix

Fresh run 20261120_074813, launch request 2026-11-20T07:48:13.803Z-7921096c, mode 57/Payload funworld3, build 2026-11-20T07:47:09.981634Z:9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071. Server logger was ready in 2.59s and gameplay port ready at 29.50s; client logger ready at 30.09s. Client began direct UDP connect after requested-map load (11.48s), sent hello, and server accepted it at 07:49:02.517Z, but client journal still had 0 snapshots and 
etwork.join.map-required-missing; dev-loop failed client_connection at 75.25s while PID 33960 remained alive. This reproduces a connect/readiness failure, not a process crash. The process scanner checkout_mimita_processes(strict=True) returned exactly server PID 33832 and client PID 33960 with their full command lines/start times. Correlate server hello/snapshot send with client receive and reconcile the 75s timeout; link the existing snapshot/reconnect investigation.

## Agent suggestion

### 2026-11-20T08:00:26.741Z — review-connect-crash-autonomy

New matching GUI run 20261120_074813 / build 2026-11-20T07:47:09.981634Z rules out an actual process crash and narrows the first missing transition. Client loaded requested funworld3 and opened UDP to 127.0.0.1:54013. At 07:49:02.503 it sent PACKET_HELLO; server inbound packet.route ID 1 at 07:49:02.515 and client-hello-accepted at 07:49:02.517 (player 1, same run/build/map). Across both rotated client journal files there is no packet.route at all, no network.client.join.accepted/network.map-required, and zero snapshots. Thus client never processed PACKET_WELCOME (or any server packet); client PID remained alive and stayed in join/connect until launch gate timed out. Likely next owner is server UDP reply -> client receive/dispatch, before map sync: handleHello in src/network/server-packets.cpp emits the acceptance event BEFORE calling sendToSourceOrPlayer(PACKET_WELCOME); that helper's send result/byte count/destination is not journaled, so 'accepted ... and sent authoritative welcome' does not prove a successful welcome send. Trace the first divergence by adding bounded owner-level evidence for server WELCOME sendto return, bytes, local/remote endpoint, then client raw recvfrom source/bytes/type and validated packet.route/dispatch outcome. If send succeeds but client has no inbound packet, inspect host firewall/socket binding/routing; if received, inspect client packet validation/dispatch. 
etwork.join.map-required-missing at multiplayer-packets.cpp:2455 is a downstream join-state symptom, not proof a welcome was received. Avoid treating timeout as crash.

## Agent suggestion

### 2026-11-20T08:21:46.439Z — launch-latest-stable-fix

New GUI button-invoked reproduction run 20261120_081941_01 on build 2026-11-20T07:47:09.981634Z: server ready at 25.148s; client logger ready at 25.438s; client PID 9044 remained alive but never Connected; client_connection timed out after 45.263s (total 70.703s). Exact report .dev/launch-reports/2026-11-20T08-19-41.839Z and per-process journals logs/2026-11-20/20261120_081941_01/{server-01,client-01}/events.jsonl. Automatic duplicate-signature issue issue-5873d2c2d033 appended recurrence seq 1069; do not create another issue.

## Agent suggestion

### 2026-11-20T08:34:58.369Z — launch-latest-stable-fix

Correction from full rotated journals for the same GUI-invoked run 20261120_081941_01: server-01/events-0001.jsonl contains network.server.client-hello-accepted at 2026-11-20T08:20:17.982Z for player 1. Thus the server did receive and accept this client's Hello; the first missing transition is after server acceptance and before client join acceptance. The previous note called this only a prior-run observation; that was incorrect. Client journal still has no network.client.join.accepted; its later network.metrics.window reports packets_received_total=0 (telemetry is not conclusive for UdpTransport). The old server event text claimed Welcome was sent before the send result; the pending instrumentation now records the actual result and first client datagram.

## Agent suggestion

### 2026-11-20T08:40:30.235Z — launch-latest-stable-fix

Live reproduction 20261120_083807, request dashboard-96a2e1ea, using published EXE 2026-11-20T07:47:09.981634Z (source generation 3, marked stale) plus published game DLL generation 399. The Tk Launch latest stable button callback was invoked via GUI hook and request acceptance took 25.5 ms. Server gameplay readiness took 27.587 s; client logger readiness took 28.000 s; launch failed at client_connection after 45.480 s (total 78.944 s). Paired journals: server PID 34032 emitted network.server.client-hello-accepted, then network.server.client-hello-welcome-send with send_succeeded=true, bytes=352, transport=ice, remote=127.0.0.1:56261. Client PID 27716 never emitted network.client.first-packet.received or join.accepted and had snapshots_received=0/map-required-missing before disconnect. Failure is reproducible; current trace narrows to after server ICE send success and before client packet handler receive. Does not yet prove network delivery. GUI scan visual rows unverified.

## Agent suggestion

### 2026-11-20T09:05:19.301Z — launch-latest-stable-fix

Run 20261120_085207 connects to retained server PID 12748 on matching mode/map/build but rejects its JoinAccept due server runtime run ID 20261120_084810 versus client expected 20261120_085207. Evidence: client packet route ID 21 and server identity mismatch event. Launch timed out after 45.032 s. This is a dev-loop retained-server identity/association defect, not a crash.

## User quote

### 2026-11-20T14:45:42.286Z — root-connection-reliability

Via root-connection-reliability; supports ctx-a05309c9f2cd:

> /goal can u add an issue to amkeit so  ai needs much better eyes fro the game to be able to  mkae it so regressiosn dotn happen like right now in the lvie run  the npcs aer floating up above teh cart in payload, and starting team dathmatch from the same .exe  its sas timeout connection timed out, then it conencts eventually, like if its not actully disconnected donnt show disconnected show like loading or connecning&#x20;
adn i start a mode and i dotn connectto it, like i started tdm on duest2cyberiav4 and i didnt ever connect it just says disconnected at the top, need to ensure that every single time  i start a servre i conecnt to it, ad if not its super duper obvious why
collaborate with other agents if tehy are awake on this  if needed or that makes ti help, the goal is that u can close, open, restart, etc, break and stress mimita.exe, but u can always start wahtevr mode u are trying to start, get into it, connect, leave, re connect, close .exe, openi t again, connect atgain to the same server, etc, do all of this consistently, and track why u cant right now,
end goal is  that u can consistnetly connect to a server whenever u start it, never a disconnectin gissues it just works&#x20;
and track why itsnot like that right n ow, and all discoveries and things to do better/different as result of ur disocveries ,

## Context

### 2026-11-20T14:46:40.106Z — root-connection-reliability

reproduction: User-reported sequence: start Payload and observe NPCs floating above the cart; start Team Deathmatch on dust2cyberiav4 from the same MiMITA.exe and observe timeout/Disconnected, delayed eventual connect or no connect; leave, start another mode, reconnect; close/reopen the executable and reconnect to the same server. Exact user session is not yet reproduced. Available adjacent evidence: run 20261120_143117 launch report says launch succeeded; client-01 journal later records a connect request for dust2cyberiav4 and an applied authoritative snapshot; dev-loop journal records managed client PID 10676 exiting with code 3221225477 at 2026-11-20T14:40:11.958Z, after which dev-loop status showed the server alive and no client. Match PIDs, correlation IDs, mode, map, EXE hash, generation, and both journals on a dedicated pinned reproduction.
expected_behavior: Every local server start automatically connects its intended client to the selected mode. Pending transport, join, map sync, spawn, and retry phases display Connecting/Loading/Reconnecting; Disconnected appears only after explicit leave or a terminal failure, with the stage and actionable reason plainly shown. Restarting or reopening MiMITA.exe can reconnect to the same running server. Payload NPCs remain correctly grounded and positioned around the cart. Agents can inspect the live game visually and correlate screenshots with exact runtime journals so visible regressions are caught and diagnosed.
acceptance: Run a pinned, repeatable 20-cycle local matrix across Payload and Team Deathmatch on dust2cyberiav4: start server and client in the same executable, leave and start another mode, reconnect, close/reopen the executable and reconnect to the same server, and exercise one transient interruption/recovery. Each cycle either reaches the authoritative connected/ready state within the configured connection policy or ends with an explicit terminal stage/reason; no attempt is reported Disconnected while nonterminal. Record all failures and first missing event. In Payload, inspect representative live screenshots and correlated NPC/cart position or grounding diagnostics to verify NPCs are not floating above the cart. Preserve build/artifact hash, generation, run ID, paired journals, launch reports, connection correlation IDs, and visual evidence.
paths: ['src/network/multiplayer-packets.cpp', 'src/network/multiplayer-tick.cpp', 'src/network/server-packets.cpp', 'src/engine/engine-tick-state.cpp', 'src/npc/npc.cpp', 'src/network/server-npcs.cpp', 'src/gamemode/payload-runtime.cpp', 'docs/specs/networking/networking.md', 'docs/specs/networking/network-event-contract.md', 'docs/specs/gui/guiv2.md']
evidence: {'reported_symptoms': 'User reports Payload NPCs floating above cart; TDM same-executable timeout followed by delayed connection; separate TDM start remains Disconnected; wants reliable start/join/leave/reconnect/restart and obvious failure reason plus AI visual checking.', 'observed_run_id': '20261120_143117', 'launch_id': '2026-11-20T14:31:17.836Z', 'launch_report': 'C:\\mimita-v9\\.dev\\launch-reports\\2026-11-20T14-31-17.836Z\\launch.json: result=succeeded; reason=launch completed with matching ready server/client', 'client_journal': 'C:\\mimita-v9\\logs\\2026-11-20\\20261120_143117\\client-01\\events.jsonl', 'dev_loop_journal': 'C:\\mimita-v9\\logs\\2026-11-20\\20261120_143117\\dev-loop\\events.jsonl', 'later_client_event': 'At 2026-11-20T14:38:59.739Z client-01 logged network.client.connect.requested for dust2cyberiav4; an authoritative snapshot was applied shortly afterward. This adjacent evidence does not prove the user-reported HUD state or exact failure.', 'client_exit': 'dev-loop.instance.exited at 2026-11-20T14:40:11.958Z; client PID 10676; exit code 3221225477; server PID 3940 remained alive in subsequent status.', 'source_observation': 'src/network/multiplayer-packets.cpp::mpConnectionHealthText returns Disconnected when ctx.active is false; active nonterminal non-ready states use Loading; reconnect terminal states have explicit status text. This points to tracing the actual top-HUD owner and state transition; it does not identify the user session cause.', 'related_issues': ['issue-453d0596e294', 'issue-5873d2c2d033']}
risk: Interleaving mode switches, reconnects, server lifetimes, and executable restarts can confuse process ownership or pair evidence from different sessions; keep tests on the loopback dev server and correlate process IDs, launch IDs, connection IDs, build IDs, generations, and run IDs. A broad status change could hide real terminal failures; only show progress states while the attempt is nonterminal. NPC/cart movement or collision fixes can create unrelated gameplay regressions, so preserve separate runtime and visual evidence and change one owner at a time.
proof_expectation: Integration proof from the exact pinned root-dev-loop executable and game generation: paired server/client events.jsonl plus launch report for each connection cycle, correlated from connect.requested through preflight, transport, hello/join accepted or rejected, map ready, spawn, authoritative snapshot, health transition, and terminal reason; include 20-cycle matrix results and explicit forbidden-state check for premature Disconnected. Human visual acceptance of live Payload NPC/cart positioning and the connection HUD, with screenshots or video tied to run ID and timestamps. Build success alone is not proof.
context_id: ctx-a05309c9f2cd

## Agent discussion and suggestions

### 2026-11-20T14:47:05.898Z — root-connection-reliability

Message: `msg-2d280a734afa` (coordination)

Subject: Coordinate on launch, network, and Payload issue

I found your active task overlaps with the user's new high-priority report. I appended the exact report and current run evidence to existing issue issue-55bd775c8026 and completed its missing reproduction/acceptance/evidence/proof fields; context ctx-a05309c9f2cd is linked. Existing related traces are issue-453d0596e294 and deferred duplicate issue-5873d2c2d033. Current source observation: mpConnectionHealthText shows Loading for active nonterminal non-ready states but Disconnected when ctx.active is false or a terminal state, so trace the actual HUD owner/state transition. Please use issue-55bd775c8026 as the shared record for your launch/network/Payload findings and avoid creating a duplicate; add any independent AI visual inspection capability proposal separately only if your evidence supports it.

## Agent discussion and suggestions

### 2026-11-20T14:58:05.910Z — root-connection-reliability

Message: `msg-c1f525fbef59` (coordination)

Subject: Connection report linked to end-to-end umbrella

Coordination notice: the current user report is now linked primarily to umbrella issue issue-cf9cd77eb056. I preserved issue-55bd775c8026 as the related detailed connection/crash trace and appended the exact new report plus current run evidence there. Please keep the specific TDM/new-mode connection trace in issue-55bd775c8026; the umbrella owns cross-mode lifecycle, visual Payload proof, and AI evidence completeness.

## Agent discussion and suggestions

### 2026-11-20T15:51:12.993Z — root-connection-reliability

Message: `msg-58b14ba36908` (evidence)

Subject: Related TDM live reconnect failure in run 152743

For the detailed client-crashes-while-connecting trace: related run 20261120_152743 began with a successful same-EXE TDM join on dust2cyberiav4 (client PID34144/server PID26136; EXE sha256 9e4a6288…, game DLL gen404 361a6835…). Later, snapshots stalled for 10s while last packet age was only 336ms. Client attempted reconnect 10 times; still-running server received each request but reported players=0 and rejected token fingerprint qZ60I0KD as token_not_found. Client terminal ReconnectFailed at 15:40:34.121Z, then teardown set active=false; both process IDs remained alive at 15:47 and run manifest was still stale online. No server player-removal/timeout event was found in the scanned journal, so reason the server had no player record remains unresolved. Evidence and screenshots are saved under logs/2026-11-20/20261120_152743/ and detailed in canonical issue issue-cf9cd77eb056. This does not reproduce the exact GUI mode-switch sequence.

## Scheduled independent review

### 2026-11-21T16:07:35.428Z — root-dll-status-review

A bounded independent review was queued as wake wake-a3735a5dadad after 149 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Scheduled independent review

### 2026-11-21T16:29:05.418Z — root

A bounded independent review was queued as wake wake-2c5d6ff131e2 after 154 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive - recurrence isolated to server reconnect-token erase path; no fix claimed)

### 2026-11-21T16:53:31.753Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper (independent, read-only; no source/build/close; no fix claimed).

Scope and readiness: issue readiness is ready (8/8, 3 open tasks). Proof status has no task-proof record for new-mode-connect-crash-trace or welcome-transport-label, and welcome-delivery-trace is unproven; no completion claim is supported. Inspection covered the issue doc, proof status, launch reports, and the latest pinned journals on build 2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871.

Observed facts (no compiler/build invoked):
1. Run 20261121_144350, request dashboard-e59a3fb1, launch report result=succeeded. After explicit leave at 15:14:53Z, a new connection attempt (connection_attempt_id 3) reached network.connection-state.changed Connecting->Connected at 15:18:19.084Z as local player 1 on dust2cyberiav4 (logs/2026-11-21/20261121_144350/client-01/events-0010.jsonl). Same-EXE TDM leave/reconnect succeeded on the current build.
2. Run 20261121_140624, TDM dust2cyberiav4: server-01 emitted network.server.player.removed at 14:31:30.073Z for player 1 with removal_reason=reliable_event_failure, reconnect_token_retained=true, players 1->0 (logs/2026-11-21/20261121_140624/server-01/events.jsonl line 3919). Client-01 then flagged false_positive_suspected=true at 14:31:45.072Z, entered Reconnecting attempts 1..10 across a 60s grace, and every network.server.reconnect.lookup returned token_not_found (slot_state not_found, token_fingerprint a9783ce6; server lines 6170-6172 onward). Client terminal ReconnectFailed plus notification Server status: Disconnected - reconnect failed at 14:32:45.137Z (client events-0015 line 75345).
3. First divergence in source: src/network/reliable-gameplay-events.cpp:119-144 erases the whole player slot (it = players.erase(it)) after logging removal; src/network/server-packets.cpp:523 computes reconnect_token_retained from !player.reconnectToken.empty() immediately before that erase, so the field means the token existed, not that it was retained. handleReconnectRequest scans only live players (server-packets.cpp:2352-2367), so an erased slot makes every retry token_not_found. This explains run 140624 and resolves the Nov 20 run 152743 open note that the server-player-record loss was unexplained.
4. No new crash artifact: no 3221225477 or dev-loop.instance.exited rows in logs/2026-11-21. The reported crash (run 20261120_143117, client PID 10676, exit 3221225477) is not reproduced by the latest runs.

Inference and uncertainty:
- The reconnect failure is a server reconnect-token lifecycle defect on the reliable-event connection-failure removal path, not a client crash and not a premature Disconnected notification: the client stayed Reconnecting for the full grace and the terminal text was truthful. Run 144350 shows reconnect still succeeds when this erase path is not involved, so the fault is path-specific.
- The user-visible shows Disconnected while still connecting symptom is not journal-proven in these runs. Run 144350 has no network.connection-state.changed event for entering Connecting/Reconnecting, and HUD text is derived at src/network/multiplayer-packets.cpp:2274-2322; no screenshots were captured. Visual acceptance stays unproven.
- Whether the Nov 20 access-violation crash and the token-loss recurrence share a root cause is unknown.

Contrary evidence:
- Run 144350 leave -> retry -> Connected succeeded (attempt 3), with a succeeded launch report, so the intended lifecycle can work on this build.
- Run 140624 terminal came after grace expiry (disconnect_elapsed_ms 60058, last_heard_age_ms 60007), so that run alone does not violate the no-Disconnected-while-nonterminal rule.
- reconnect_token_retained=true is contradicted by the immediately following token_not_found; treat that field as unreliable evidence.

Next discriminating check (not performed by this reviewer):
1. On a pinned root-dev-loop build, force or await a reliable_event_failure removal and reconnect inside the token grace; assert server reconnect.lookup result=ok and client health returns Connected. A remaining token_not_found confirms the erase-path defect.
2. For the owning task: preserve reconnect-token disposition across this erase path (retired-token lookup or delegate to the normal disconnect/timeout owner) and report the actual disposition (retained-until or false) in the removal event.
3. Journal a network.connection-state.changed transition for entering Connecting/Reconnecting attempt N, then capture HUD screenshots during retry to close the visual gap.

Process review: repeated missing-evidence/attribution delay. Runs 20261120_152743 and 20261121_140624 both ended in token_not_found, and the misleading reconnect_token_retained=true plus absent removal/token attribution blocked immediate root cause for more than 24h. One concrete improvement proposal is appended to issue-91e3384b219b with a measurable target and smallest experiment.
