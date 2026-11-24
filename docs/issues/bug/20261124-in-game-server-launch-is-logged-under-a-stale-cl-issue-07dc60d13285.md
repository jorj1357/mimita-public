# issue-07dc60d13285: In-game server launch is logged under a stale client identity

## Human report (verbatim, immutable)

> Observed: in run 20261124_053435, a process whose command line starts mimita.exe --server --bind 0.0.0.0:1357 --map slaughterhouse1 --mode bloodtest --npcs 1 wrote server-role events to client-01/events.jsonl as PID 30424, while run.json still listed server-01 PID 32184 on dust2cyberiav4 and client-01 PID 33892 as stale.


## Agent suggestion

### 2026-11-24T06:03:27.689Z — bloodtest-dev-launch

Runtime evidence from run 20261124_053435 was overwritten/rotated by the bounded journal: current retained file is almost entirely PID 30424 process=server and has snapshots marked connection_stale=true, followed by peer_sends=[]/players=0. Historical observed client events had join accepted and snapshots_received; therefore the client did connect then stopped refreshing server lastHeardMs. Current retained slice lacks the initial hello/welcome events, so exact first lost packet remains unknown. Server marks stale when silence exceeds configured 2500ms in server-packet-chat.cpp.

## Agent suggestion

### 2026-11-24T06:04:04.135Z — bloodtest-dev-launch

Additional timing evidence from same server process PID 30424, run 20261124_053435: at 06:03:38Z server loop reported actual_hz=13.84, loop_avg_ms=360.47, max/p95=695.58, 3 overruns in 1.084s; tick window had 3/15 ticks >100ms, NPC avg 54.03ms/max 256.54ms. Tick-stall events show totals 224-313ms, dominated by NPC phase. At 06:03:39Z it recovered to 39.65Hz, but continued overruns; 06:03:41Z 49.15Hz. This is direct evidence that slow server output can coincide with severe server update stalls. It does not prove these late stalls caused the earlier peer timeout, since first disconnect timing/handshake was lost from the rolling journal. Correlate client heartbeat/input and server tick stalls on a fresh pinned run.
