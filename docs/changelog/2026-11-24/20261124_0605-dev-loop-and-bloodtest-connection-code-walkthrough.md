# Dev-loop launch and Bloodtest connection walkthrough

## Scope

Added launch mode 59 for Bloodtest on Slaughterhouse1 with one NPC, and updated agent instructions/spec to automatically capture actionable recurring friction as a complete, discoverable issue while leaving follow-up unowned unless claimed. Created issues for launch reliability, start-client mode selection, and server/client instance identity. This entry records the follow-up code walkthrough requested by the user.

## Launch path findings

- The dev-loop launch report for `2026-11-24T05:49:58.844Z` failed at `client_reconciliation`, before either server or client process start. The reported cause was `WinError 5: Access is denied` while strictly scanning same-checkout MiMITA processes.
- `checkout_mimita_processes(strict=True)` in `devscripts/dev-loop.py` calls `psutil.process_iter`, then reads matching process executable, command line, and start time. `reap_stray_mimita_processes` fails closed when that scan raises, and the launch coordinator emits a failed reconciliation stage. This preserves the safety rule against starting a duplicate while process identity is unknown.
- The current exception message does not identify which PID or process operation caused this `WinError 5`; the exact denied process/operation is unknown. A separate earlier report named PID 18752 and a cleanup timeout; that is a different failure.
- GUI and CLI were both exercised during the investigation. A dashboard “Launch latest stable MiMITA.exe” run (`20261124_052552`) eventually reached a matching server/client result after roughly 70 seconds to server gameplay readiness and 72 seconds to client logger readiness. The later CLI request failed before process start during reconciliation. They were not a controlled same-mode comparison, so this does not establish that one launch method is more reliable.

## In-game connection path findings

- Client connection proceeds through transport opening, periodic `PACKET_HELLO` (or join-token request), server hello acceptance and Welcome send, then client Welcome validation. In `src/network/multiplayer-tick.cpp`, a valid Welcome sets `ctx.connected`, `ConnectionState::Connected`, assigned player ID, and required map, and emits `network.client.join.accepted`.
- The server updates `lastHeardMs` for received client packets. `handleClientTimeout` in `src/network/server-packet-chat.cpp` marks a peer stale when silence exceeds `stale_packet_threshold_ms` (2500 ms in `config/networkingconfig.json`), freezes the body, and holds its slot for reconnect grace. This explains the “connected, then stale/disconnected” state transition when client packets stop arriving.
- In run `20261124_053435`, earlier observed events proved the client had joined, received snapshots, and saw one remote NPC; server PID 30424 also applied damage to NPC 1000. Later server snapshots marked the peer `connection_stale=true`, and subsequent snapshots had no peers and zero players. The bounded journal had rolled past the original hello/Welcome events by the time of this review, so the first missing packet and the cause of the silence remain unproven.
- The same server process later showed severe timing overruns: at `2026-11-24T06:03:38Z`, a 1.084-second loop window reported 13.84 Hz, 360.47 ms average loop time, 695.58 ms max/p95, and 3 overruns. Tick samples showed NPC work averaging 54.03 ms and peaking at 256.54 ms; individual tick totals reached 224–313 ms. Performance improved in later windows but remained below 60 Hz with overruns. This supports slow server progress as a real symptom, but these late samples do not establish that the stalls caused the earlier connection loss.
- In-game server PID 30424 wrote server-role records to `client-01/events.jsonl` while `run.json` retained different server/client identities. This evidence is recorded in `issue-07dc60d13285` and prevents treating the dashboard's instance view as authoritative for that in-game-launched pair.

## Issue records

- `issue-a5ca672b6628`: loud, precise dev-loop failure reporting and agent follow-up. Added the latest `WinError 5` evidence and linked the user's walkthrough request.
- `issue-07dc60d13285`: in-game server/client identity and journal mismatch. Added later server timing evidence and linked the user request as related context.
- `issue-b2b4e7dd17fe`: `--start-client` selected a different launch mode than the user expected.

## Validation and limits

- Parsed `devscripts/dev-launch-modes.json` successfully after adding mode 59.
- Read the relevant dev-loop launch, process reconciliation, handshake, server Welcome, and stale-peer code paths; compared the CLI report, dashboard launch report, process/manifest identities, and retained `events.jsonl` evidence.
- No source code change, build, or synthetic test was performed for this walkthrough. No process was stopped or restarted. The original handshake is not retained, and the latest connection attempt was not repeated with added telemetry; therefore the precise root cause of the earlier disconnect is still open.
