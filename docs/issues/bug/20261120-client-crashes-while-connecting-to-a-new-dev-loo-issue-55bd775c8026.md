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
