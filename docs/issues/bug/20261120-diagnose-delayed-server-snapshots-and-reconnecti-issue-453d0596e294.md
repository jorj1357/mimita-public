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
