# Launch reliability and dashboard live evidence

## Scope

Continued the latest-stable launch and GUI process inventory investigation.
Added explicit standing guidance that agents may restart the checkout-owned
daemon and dashboard GUI when needed for live proof. The dev-loop restart
authority and runbook already existed; this update makes the dashboard GUI
case visible at the repository instruction entry point.

## Source changes

- `AGENTS.md`: explicitly authorizes and encourages safe daemon/dashboard GUI
  restarts for stale code, wedged control paths, or required live proof, with
  process preservation and post-restart verification requirements.
- `devscripts/dev_loop/gui.py`: client counts now use rendered live client
  PIDs; external server/client roles use exact command-line flags; scan errors
  identify that the displayed list is stale.
- `devscripts/dev-loop.py`: downstream connection/mode/NPC readiness gates are
  skipped when client logger startup fails.
- `devscripts/dev_loop/launch_report.py`: failure code and detail now identify
  the first failed launch stage; a focused regression case covers a later
  timeout masking an earlier failure.
- `src/game/game-cli.cpp`: `--versioninfo` avoids initializing the full
  structured logger during lightweight executable identity checks.
- `src/debug/structured-log.cpp`: added bounded, categorized startup markers
  around config, run lease, quota lock/state, reconciliation, prune, and
  journal open. They pinpoint the first missing logger startup stage in the
  existing server log.

## Build and tests

- Root dev-loop build succeeded: build ID
  `2026-11-20T07:47:09.981634Z:9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071`,
  source generation 3.
- `python devscripts/quality/test_launch_report.py`: 6 tests passed.
- Python byte-compilation for the changed dev-loop modules passed.

## Live evidence

- Controlled daemon restart used PID 32440 and verified current daemon source
  identity. The dashboard was restarted separately; new window process PID
  33232 responds to Windows window messages. Computer accessibility could not
  read the dashboard tree, so visual row rendering remains unverified.
- Run `20261120_074813` used the exact new build. Launch request acceptance was
  15 ms; versioninfo exited in 157 ms; server logger readiness was 2.593 s from
  launch; server gameplay readiness was 29.500 s; client logger readiness was
  30.093 s.
- Server PID 33832 reached `logger.started`; the new startup markers show the
  quota lock acquired in 3 ms and the journal opened in 86 ms. The previously
  reported server logger timeout did not recur in this instrumented run.
- Client PID 33960 remained alive, but the `client_connection` gate timed out
  at 75.250 s. The server accepted its hello; the client still had zero
  snapshots and logged `network.join.map-required-missing`. This is a connection
  readiness failure, not evidence of a process crash.
- The dashboard's exact process provider returned one server (PID 33832) and
  one client (PID 33960), each with executable, command line, and start time.
  The process-window text and removal-on-exit behavior were not visually
  accepted.

## Remaining work

The launch/process-panel issues remain open. The live launch did not complete
the client join, and accessibility prevented visual confirmation of the
process rows. Connection evidence was added to existing issues
`issue-55bd775c8026` and `issue-453d0596e294`; duplicate automatic ticket
`issue-5873d2c2d033` was deferred to those investigations. The intermittent
server logger timeout remains assigned as `issue-7a22b715568b` pending another
reproduction or a conclusive cause.

## Follow-up: GUI launch and packet boundary trace

- Added a GUI guard that ignores repeated Launch latest stable requests while one is pending. The process panel now says “Scanning running MiMITA processes…” while the scan is active instead of briefly claiming there are zero processes.
- Restarted only the verified checkout dashboard (old PID 2364); new dashboard PID 27780 inherited the GUI launch-button test hook. The root daemon PID 30848 was healthy and did not need a restart. This follows the user-authorized restart rule and preserved unrelated processes.
- Actual Tk button callback launched run `20261120_083807` (request `dashboard-96a2e1ea`). Button acknowledgement was 25.5 ms. It selected published EXE `2026-11-20T07:47:09.981634Z` (source generation 3, stale versus current source) with game DLL generation 399. Server gameplay readiness was 27.587 s; client logger readiness 28.000 s; join timed out at 45.480 s; total launch 78.944 s. No successful connection.
- Generation 399 was built by root dev-loop attempt `game-dll-20261120T083155607100Z-c98479`, succeeded and published. Total build time 190.043 s; object-cache scan 87.589 s (scan parse 79.977 s), preflight 25.269 s, compile 50.658 s, link 31.150 s. The over-60-second wait auto-created `issue-63dccbd8f8b6`; its timing evidence is recorded there.
- Run journals now prove server PID 34032 accepted the client Hello and `sendToSourceOrPlayer` returned success for a 352-byte ICE Welcome to loopback port 56261. Client PID 27716 emitted no `network.client.first-packet.received`, no join acceptance, and zero snapshots before disconnect. The missing boundary is after the successful server ICE send and before the client's packet dispatch; network delivery is not yet proven.
- The strict Windows process scan returned both live same-checkout PIDs (34032 server, 27716 client), with executable, full command line and start time, in 836.2 ms. The run manifest correctly retained both, but marked the failed-join client `stale` while the OS process remained alive. This clarifies that process-running state and connection-health state need separate presentation. GUI panel row rendering remains unverified because accessibility inspection was unavailable.
- Both processes were stopped through the root dev-loop after collecting evidence; a follow-up OS process scan returned no running `mimita.exe` processes.
- Python byte-compilation of `devscripts/dev_loop/gui.py` passed. `git diff --check` reports pre-existing whitespace warnings in unrelated/concurrent files, including issue records; the GUI file itself has no whitespace errors.
- Updated the existing launch/connect, process-inventory and wait-friction issue discussions with the matched run/build evidence. No behavior issue is closed. The requested five successful launch-and-gameplay exercises remain outstanding; this run does not count because it failed to connect.

## Final follow-up: retained-server fix and five live launch exercises

- Confirmed the failure mechanism in run `20261120_085207`: a new client was checked against the newly created run ID even though it connected to a retained server from `20261120_084810`. The client received a valid `PACKET_JOIN_ACCEPT`, then rejected the retained server identity. Run metadata also mislabeled the server launch/journal and initially reused the previous command queue.
- Updated `devscripts/dev-loop.py` to preserve server-owned launch/run/journal metadata, give each client its own command queue, and use loopback to reconnect to a healthy same-machine retained server. This is a dev-loop source change; it did not require another DLL build.
- Repeated GUI launches through the actual Launch latest stable Tk callback. All five completed and each client journal contained `network.client.join.accepted`:

  | Run | Result | Total | GUI acknowledgement | Gameplay exercise |
  |---|---:|---:|---:|---|
  | `20261120_084810` | connected | 58.205 s | 2.0 ms | 120-tick movement, snapshots, aim, shoot |
  | `20261120_090819` | connected | 27.899 s | 2.5 ms | 120-tick movement, jump, aim, shoot |
  | `20261120_090948` | connected | 26.714 s | 72 ms | 90-tick movement, snapshots, aim, jump, shoot |
  | `20261120_091107` | connected | 29.123 s | 3.2 ms | 120-tick movement, snapshots, jump, aim, shoot |
  | `20261120_091222` | connected | 32.874 s | 8.4 ms | 120-tick movement, snapshots, jump, aim, shoot |

- Movement command receipts reported all requested ticks applied, with before/after snapshots and action receipts in the isolated run journals. The last run remains available in the root dev-loop. Server reuse accounts for the shorter later launches; cold server readiness is still materially slower than the button acknowledgement.
- The strict process provider previously returned exact same-checkout server/client PIDs, executable paths, command lines and process start times in 836 ms. The later corrected launch manifests preserve retained server identity and journal separately from each new client. Dashboard row rendering and visual removal-on-exit behavior remain unverified because UI accessibility inspection could not read the row tree.
- Root dev-loop and dashboard GUI restarts were performed only for the verified checkout-owned processes after source changes, then their replacement processes were checked. Repository instructions now explicitly authorize and encourage these controlled restarts when required for live proof, with unrelated processes preserved.
- Build generation 400 succeeded, but took about 136 s; its object-cache scan alone exceeded 106 s. The existing build-friction issue records this; build success is separate from the five runtime launch results.
- Python byte-compilation and focused diff checks passed. No behavior issue is closed solely on these results; the process panel’s visual acceptance remains open.

## Final-source follow-up: five additional GUI launches and measured waits

- Tested the final Python source state with five consecutive actual dashboard `Launch latest stable MiMITA.exe` button callbacks. Each completed successfully with `network.client.join.accepted`, `client.first-frame`, hot-client readiness, and server/client build identity match. All used published EXE build ID `2026-11-20T07:47:09.981634Z:9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071`, SHA-256 `9e4a62885adb255cc2b666abbee4e233f628bb9d5b56518cc6f5a7b47005c071`, game-DLL generation 400, and the retained server PID 3940.

  | Run | Result | Ready | Press-to-daemon accept | Queue wait | Client PID |
  |---|---|---:|---:|---:|---:|
  | `20261120_094225` | succeeded | 22.195 s | 147.8 ms | 210.1 ms | 19384 |
  | `20261120_094509` | succeeded | 27.575 s | 2008.9 ms | 294.3 ms | 24424 |
  | `20261120_094642` | succeeded | 25.944 s | 799.1 ms | 1170.5 ms | 10048 |
  | `20261120_095024` | succeeded | 24.132 s | 568.8 ms | 1230.9 ms | 29772 |
  | `20261120_095213` | succeeded | 23.920 s | 3.3 ms | 1202.8 ms | 10540 |

- Across the five, median time to full readiness was 24.132 s and median press-to-daemon acceptance was 568.8 ms; queue wait is a separate stage. The longest press-to-daemon acceptance was 2008.9 ms, and queue waits exceeded one second on three runs; these remain linked to `issue-7d3dcfd7433b`. The recent full-readiness times benefit from a retained ready server, so they do not measure a cold server start. Stage `accept_latency_ms` measures press-to-daemon receipt, while `queue_wait_ms` measures daemon delay before processing. The dashboard status measures button-to-response round trip; the screenshot for run `20261120_095213` displayed 1203 ms, consistent (to rounding) with the 3.3 ms acceptance plus 1202.8 ms daemon queue wait. The exact GUI callback round-trip should be persisted as a separate metric.
- Each client received before/after snapshots, movement commands (120 requested ticks), aim commands and primary-fire holds. Control receipts completed in all five runs. In run `20261120_094642`, an additional `agent.jump` command returned `terminal.command.unknown`; do not count that unsupported input as a successful jump test. Later animation logs show jump/fall states, but they are not proof that this command succeeded.
- Bounded post-ready instrumentation exposed additional delay after gameplay readiness: `save_state` measured 424, 5250, 2758, 3796 and 4490 ms, with `print_status` measuring 146, 493, 451, 1458 and 290 ms. This is recorded as further latency work, not attributed to a proven internal root cause. `state()` currently calls `latest_build_identity()` twice; profile its cost and the manifest write separately before changing behavior.
- The dashboard screenshot showed the visible “Launch succeeded” status and timing for run `20261120_095213`. The process rows were outside the observed viewport, so visual process-panel row accuracy and removal-on-exit remain open under `issue-35fd6df3d947`. The manifest and Windows process list both identified the current client as PID 10540, same checkout executable, same build identity and current run journal/queue.
- Audited final-series reports contain no failed launch. Earlier failed attempts remain separately recorded in issues `issue-5873d2c2d033` (client connect timeouts) and `issue-f8b80ddaea17` (repaired orphaned `launch_exception` report); the timeout reports were linked to the connect investigations. A runtime proof for retained-server identity was refreshed against run `20261120_095213` and verified by `network.client.join.accepted` with `result=ok`.
- `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/gui.py devscripts/dev_loop/launch_report.py` passed. Focused `git diff --check` passed with only Git line-ending conversion notices. No EXE/DLL build was requested for this Python-only follow-up. Current daemon PID 34136 and server PID 3940 remained healthy; current run/client is `20261120_095213` / PID 10540.
- The GUI launch callback test wrapper used a fixed 60-second sleep while waiting for the report even though these launches reached ready in 22–28 seconds. This avoidable automation wait is added to the launch latency issue; future callback runs should poll the terminal launch report and stop immediately.
