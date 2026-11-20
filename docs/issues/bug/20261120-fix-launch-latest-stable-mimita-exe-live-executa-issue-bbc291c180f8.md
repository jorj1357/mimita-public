# issue-bbc291c180f8: Fix Launch latest stable MiMITA.exe live executable replacement regression

## Human report (verbatim, immutable)

> a l飭unch latest stable mimtiaexe regression where im pressing it and it doenst close the live running .exe and replace it wht a new one, its quite slow


## User quote

### 2026-11-20T03:46:27.394Z — dashboard-scroll-refresh

Via dashboard-scroll-refresh; supports unspecified:

> log a issue that  alunch latest stable mimtiaexe regression where im pressing it and it doenst close the live running .exe and replace it wht a new one, its  quite slow

## User quote

### 2026-11-20T06:45:25.134Z — launch-latency-investigation

Via launch-latency-investigation; supports Follow-up launch-button attempt: user reports pressing again eventually launched; delay remains long and stage attribution is requested.:

> can u ensure tht  launch button auto issue creation its like it adds to a existing issue and also add that i pressed it again and it launches eventuall just need to track why it takes a long long time and trace each step in it and then log all those times and make them go to 0 as clos to 0 as we can

## Triage

### 2026-11-20T06:45:26.292Z — launch-latency-investigation

Summary: Follow up: button was pressed again and user reports it eventually launches, but the action remains very slow. Correlate the attempt with launch report IDs, determine the first dominant delay, and link findings to the shared launch latency issue.
Owner: launch-latency-investigation
Priority: high

## Task linked

### 2026-11-20T06:45:27.193Z — launch-latency-investigation

Task: launch-report-issue-autowire (step 0)

## Agent suggestion

### 2026-11-20T06:49:14.106Z — launch-latency-investigation

Latency evidence linked to issue-7d3dcfd7433b: saved launch attempts spent 66–69 s in server gameplay readiness; another spent 38.9 s there and 45.1 s in client connection. Need correlate the user-reported eventual successful retry with its launch_id and inspect that attempt’s stages before assigning cause.

## User quote

### 2026-11-20T07:09:44.973Z — launch-latest-stable-fix

Via launch-latest-stable-fix; supports Follow-up report reiterates latest-stable launch latency/failure and adds a distinct GUI process-inventory accuracy concern; keep the existing button issue and track inventory separately.:

> can u aadd a issue that the gui launch latest stable mimita.exe button having long wait times sometimes doesnt work at all and the mimta processes window is inaccuratte like there is no client avialble at all need to ensure accuracy of the gui adn ensurre that launch latest stable miimta.exe button is quick and consistnentl works like u change code ot do that

## Agent discussion and suggestions

### 2026-11-20T07:11:50.915Z — launch-latest-stable-fix

Message: `msg-42a5c4756a25` (ask)

Subject: Coordinate quick-launch readiness fix under your dev-loop lease

I found run 20261120_070219: client logger readiness failed at 20s (PID 23636 remained alive), then the daemon waited another 45s on mode_start even though no client logger was available. launch.json reports client_logger_timeout but takes failure_detail from later mode_start failure. I need a tiny change in devscripts/dev-loop.py near launch_latest(mode_ready gating) plus a launch_report.py first-failure detail fix. Are you editing that launch_latest region, and can you release the dev-loop.py lease or accept a disjoint patch proposal when ready? I will not edit the leased file.

## Assignment

### 2026-11-20T07:12:55.705Z — launch-latest-stable-fix

Owner: launch-latest-stable-fix

## Agent suggestion

### 2026-11-20T07:59:00.534Z — launch-latest-stable-fix

Fresh mode-57 run 20261120_074813 on build 2026-11-20T07:47:09.981634Z measured accepted request in 15ms, client reconciliation 1.906s, versioninfo 2.109s total (157ms subprocess), server logger ready at 2.593s, server gameplay readiness at 29.500s, and client logger at 30.093s. The launch then failed at client_connection after 75.250s while the process remained alive; exact failure and snapshot evidence are in issues 55bd775c8026/453d0596e294. Button's underlying launch path is responsive, but end-to-end completion is still blocked by connection behavior.

## Agent suggestion

### 2026-11-20T08:21:47.029Z — launch-latest-stable-fix

The GUI's button callback triggered request dashboard-5bab952c and was accepted in 1.5ms. Total attempt 70.703s; server gameplay readiness dominated 25.148s and connection wait 45.263s. This disproves a slow click/control acknowledgment for this attempt; remaining latency is required cold-server readiness plus failed connection. Current GUI script now ignores duplicate Launch clicks while a tracked request is pending, avoiding serialized redundant relaunches. No successful launch yet.

## Agent suggestion

### 2026-11-20T08:40:30.439Z — launch-latest-stable-fix

Same GUI-button run 20261120_083807 isolates button response from total time: request accepted in 25.5 ms; 112.6 ms to stage launch bundle; 2.103 s client reconciliation; server gameplay ready at 27.587 s; client connection gate failed after 45.480 s; total 78.944 s. Therefore reported long wait is dominated by mode-57 server/map readiness and a failed 45 s join wait, not the GUI button/daemon acknowledgement. GUI duplicate-request guard is implemented; outcome still fails to connect.

## Agent suggestion

### 2026-11-20T08:41:40.251Z — launch-latest-stable-fix

Process inventory backend evidence for run 20261120_083807: strict OS scan returned exactly server PID 34032 and client PID 27716 (0.836 s) with executable, full command line, and start time. Both are same-checkout processes. After the 45 s join timeout the client remained alive, but run manifest labeled its connection state stale; inventory should distinguish running process from connected/healthy state rather than hide it.

## Agent suggestion

### 2026-11-20T09:05:19.161Z — launch-latest-stable-fix

Second actual GUI launch run 20261120_085207/request dashboard-11cf6a4d with same mode/map/build retained healthy server PID 12748 but expected new run ID 20261120_085207. Client got a 352-byte valid PACKET_JOIN_ACCEPT carrying server run ID 20261120_084810, then logged network.server-identity-mismatch and timed out after 45.032 s. This confirmed retained-server run identity mismatch; run manifest also incorrectly labeled the server with new launch ID/journal, and the new client reused command queue agent-command-20261120_084810.jsonl. Fix in progress separates server identity, keeps old server journal, and creates a per-run client command queue.

## Agent suggestion

### 2026-11-20T09:16:52.802Z — launch-latest-stable-fix

Final live proof: five GUI Launch latest stable runs (084810, 090819, 090948, 091107, 091222) all completed with network.client.join.accepted and gameplay movement/action receipts; acknowledgement 2.0–72 ms. Runs 090819–091222 reused server and completed in 26.714–32.874s. Run 085207 isolated retained-server run identity mismatch; fix preserves server-owned run/journal metadata and gives each client a fresh queue, confirmed in 090819 manifest/journal. Keep dashboard visual process-row acceptance open; do not close broader issue until that and launch latency targets are resolved.

## Agent suggestion

### 2026-11-20T09:23:26.834Z — launch-latest-stable-fix

Failure audit: run 20261120_090530_01 failed after server readiness with TypeError expected str, bytes or os.PathLike object, not NoneType before client spawn because queue_command_path was None. The daemon journal recorded launch.exception/control failed, but launch.json remained running without failure code or automatic issue. This was an intermediate source bug fixed by moving fresh queue initialization before client args. I am closing the general exception-to-report gap so future failures are terminal and issue-linked.

## Agent suggestion

### 2026-11-20T09:30:34.004Z — launch-latest-stable-fix

Post-exception-fix runtime proof: GUI callback run 20261120_092642 from restarted current-source daemon completed in 55.166s, accepted at 2.5ms, with client join/map/mode/hot-client/build-identity readiness; run manifest associates client PID 28452 with retained server PID 3940, original server run 20261120_090706_01 and original server journal. Client journal then shows 120 movement ticks and completed snapshot/jump/aim/shoot commands. The retained-server task has a new proof linked to this current source snapshot.

## Agent suggestion

### 2026-11-20T09:41:54.406Z — launch-latest-stable-fix

Latency iteration: incremental _JournalCursor reduced client_connection/mode detection from 29.6/48.0s elapsed to 18.4/23.2s in run 20261120_093725; total fell 55.2→33.3s despite separate run conditions. A further fixed 5s firewall monitor is now skipped only when preflight proves LOOPBACK_ONLY; one post-start observation remains. New post-ready timing event will measure save_state/status overhead. Stress completed: 120 movement ticks, jump/aim/shoot and start/end snapshots.
