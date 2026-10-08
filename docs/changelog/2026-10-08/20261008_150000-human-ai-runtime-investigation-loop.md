# Human-and-AI runtime investigation loop

## Source and ownership evidence

- Extended `devscripts/dev-loop.py` as the single daemon plus short-lived localhost IPC request client.
- Added atomic `.dev/current-run.json` state alongside the existing `.dev/state.json`.
- Kept `devscripts/dev-launch-modes.json` as the launch-recipe authority.
- Added run identity fields to the canonical `StructuredLogger` journal through `run.started`, with `logger.started` and `--versioninfo` confirmation.
- Added bounded client `network.client-snapshot-window` events and preserved the existing server `performance.server-tick-window` owner diagnostic.
- Added run/build identity fields to `WelcomePacket` and rejected mismatched expected identities in the real multiplayer client path. Protocol version is now 41.

## Build and executable identity

- Canonical build: success after forcing the touched translation units to recompile.
- Published executable used by the verified run: `C:\mimita-v9\.dev\builds\1714\mimita.exe`.
- SHA-256: `357e0d2c14801e4f42bb85b7df153d281934b9b6d7e5878aa705246a8378e245`.
- Build number: `1714`.
- Python validation: `python -m py_compile devscripts/dev-loop.py` passed.
- `git diff --check` passed.

## Server/client synchronization evidence

- Dev-loop control endpoint: one daemon on localhost; the launch request was accepted through `--request launch --launch-mode 8`.
- Run ID: `20261008_190452`.
- Server PID: `28968`.
- Client PID: `16016`.
- `.dev/current-run.json` reported `server_ready=true`, `client_ready=true`, and `builds_match=true`.
- Both processes reported the same published executable and build identity in the journal.

## Exact journal and runtime scenario

- Journal: `C:\mimita-v9\logs\2026-10-08\20261008_190452\events.jsonl`.
- Scenario: launch mode 8 (`zombie towerrrrr`), map `atdm`, one startup NPC, real server and client.
- The first journal record for each process was `run.started`; the preflight also wrote `versioninfo.executed` before the server/client launch.

## Runtime evidence

- `performance.server-tick-window` appeared from the live server with `npcs=1`, `snapshot_avg_ms` below 0.15 ms in the observed windows, and `total_avg_ms` generally around 0.4–22.5 ms. An earlier one-NPC sample before coordinator instrumentation contained a roughly 611 ms total window, motivating the additional coordinator diagnostic.
- `network.coordinator-ice-poll` now records the previously unmeasured coordinator work; the live run showed individual polls around 100–145 ms while the fixed-tick windows remained around 0.9–1.1 ms in the later sample.
- `network.client-snapshot-window` appeared from the live client with complete chunked snapshots, `entity_count=2`, `snapshots_missed=0`, and `tick_gap=1` in the observed steady-state records.
- The client diagnostic is rate-limited to approximately once per second; its `snapshot_interarrival_ms` measures diagnostic-summary spacing, while `snapshots_received` demonstrates that many snapshots were applied between records.

## Evidence boundary

- This proves the daemon/request path, newest published executable discovery, exact journal discovery, identity agreement, and real runtime diagnostic visibility.
- It does not prove a 60 Hz performance fix. A multi-NPC baseline and a later before/after fix run are still required.
- No human visual or multiplayer acceptance claim is made here.

## Crash-tracing extension and Juggernaut evidence

### Source and ownership evidence

- Added `performance.server-loop-window` at the server outer-loop owner, including actual Hz, loop average/P95/max, catch-up, projectile workload, triangle queries, and correction counts.
- Extended `performance.server-tick-window` with projectile and physical-contact-weapon stage timings.
- Added bounded `performance.client-projectile-window` records with active projectiles, simulation steps, maximum step duration, invalid-state count, bounces, and explosions.
- Added crash breadcrumbs for severe server loop overruns, expensive/invalid client projectile steps, severe client frame spikes, and exceptions thrown while rendering local, remote-player, or remote-NPC actors.

### Build and executable identity

- Build status: success through the dev-loop build pipeline.
- Published executable: `C:\mimita-v9\.dev\builds\1724\mimita.exe`.
- Build number: `1724`.
- SHA-256: `fe5c9f52ebf1d57687335de99def8d64e757cef1f7a782c8de16bc8ed1c41996`.
- Server and client were verified by `.dev/current-run.json` as using the same build.

### Runtime scenario and exact journal

- Dev-loop request: `python devscripts/dev-loop.py --request launch --launch-mode 9`.
- Scenario: Juggernaut on `dust2cyberiav4`.
- Run ID: `20261008_191804`.
- Server PID: `11912`.
- Client PID: `4664`.
- Journal: `C:\mimita-v9\logs\2026-10-08\20261008_191804\events.jsonl`.

### Runtime evidence

- The live journal reached `67` NPCs and `68` entities.
- `performance.server-tick-window` showed NPC averages around `0.24–0.26 ms`, projectile simulation approximately `0 ms`, contact-weapon work approximately `0.004 ms`, and snapshot work around `0.2 ms` in the observed final windows.
- `performance.server-loop-window` still showed actual Hz between approximately `50.8` and `58.8`, with outer-loop maxima up to `200.7 ms`.
- Coordinator polls remained approximately `77–198 ms` in the observed window, confirming that outer-loop stalls remain measurable separately from fixed-tick simulation.
- Client snapshot records continued to show `67` remote NPCs, `snapshots_missed=0`, and `tick_gap=1`.
- `performance.client-projectile-window` appeared and reported zero invalid projectile states in the observed window. No local predicted projectile was active during that sample, so projectile-crash behavior was not reproduced.
- The prior crash report `C:\Users\guita\AppData\Local\MiMITA\crashes\crash-2026-10-08_15-13-11-32012.txt` remains an uncaught C++ exception with no usable breadcrumb; no new crash occurred during the verified 1724 observation window.

### Evidence boundary

- The new logging is present in the newly built executable and visible in the real Juggernaut journal.
- This run strengthens the conclusion that server outer-loop stalls, especially coordinator polling, are separate from cheap per-tick NPC/snapshot work in this sample.
- It does not yet identify the exact crash owner because the crash was not reproduced after the new breadcrumbs were installed.
- The next crash report should contain the last render, projectile, frame-spike, or server-loop breadcrumb; a repeat run must inspect that report together with the same journal before implementing a crash fix.

## Coordinator stall fix and Juggernaut before/after evidence

### Source and ownership evidence

- The recurring coordinator HTTP poll in `tickIceCoordinator` was blocking the authoritative server loop even though the fixed-tick NPC, projectile, and snapshot stages were inexpensive.
- Moved only `coordinatorIceHostPoll` to one synchronized background worker. The authoritative server thread consumes the result and remains the sole owner of `IceAgent`, `gPendingIcePeers`, and peer-container mutation.
- Added an explicit worker shutdown at dedicated-server and listen-server shutdown boundaries. The existing join-only TURN/ICE peer setup remains on the server thread and is not claimed to be asynchronous.

### Build and executable identity

- Build pipeline status: SUCCESS; the fresh published executable used by the runtime run was `C:\mimita-v9\.dev\builds\1727\mimita.exe`.
- Build number: `1727`; SHA-256: `95f2e7da008f8bfd09374816b0c7eb5532b9ad366852fc28484ee4d03462389c`.
- `--versioninfo` confirmed the exact executable path and build timestamp `2026-10-08 19:24:59 UTC`.
- `git diff --check` passed.

### Server/client synchronization evidence

- The existing single dev-loop daemon accepted `python devscripts/dev-loop.py --request launch --launch-mode 9`; no second daemon was started.
- Run ID: `20261008_192635`.
- Server PID: `5168`; client PID: `17020`.
- The run state reported `server_ready=true`, `client_ready=true`, and both processes used build `1727` with the same executable path and identity.

### Exact journal and runtime scenario

- Scenario: Juggernaut on `dust2cyberiav4`, with `67` NPCs and `68` total entities observed by the client.
- Journal: `C:\mimita-v9\logs\2026-10-08\20261008_192635\events.jsonl`.
- Baseline journal: `C:\mimita-v9\logs\2026-10-08\20261008_191804\events.jsonl`.

### Runtime before/after evidence

- Baseline outer-loop windows: actual Hz mean `48.564`, minimum `2.126`; loop-max mean `261.144 ms`, maximum `5,663.534 ms`; coordinator poll maximum `5,661 ms`.
- After the fix, the live journal contains `network.coordinator-ice-poll` records with `background=true`. The observed coordinator requests took `71–1,086 ms`, but those waits no longer occupy the authoritative loop.
- After the fix, outer-loop windows had actual Hz mean `57.170`, minimum `8.126`, with the mature windows commonly at `59.8–59.9 Hz`; loop-max mean `86.478 ms`, maximum `1,314.527 ms`. Snapshot diagnostics continued to report `snapshots_missed=0` and `tick_gap=1`.
- This is a material improvement for the coordinator-induced stall, but it is not yet a proof of a perfectly flat `59.9–60.1 Hz`: the new run still contains rare large NPC-stage spikes (`npc_max_ms` up to about `1,259.5 ms`) and corresponding outer-loop overruns. Those spikes are now separated from coordinator duration and are the next performance owner to investigate.
- No crash/failure event was recorded during this runtime interval, and the server/client remained alive when checked.

### Evidence boundary

- Confirmed: the blocking recurring coordinator poll was removed from the authoritative loop, the new executable was used by both processes, and the real Juggernaut journal proves the change is active.
- Ruled out for the remaining stalls: coordinator HTTP wait as a direct blocker of the fixed server loop during this run.
- Remaining hypothesis supported by the new evidence: rare NPC simulation/navigation work or another NPC-adjacent operation is producing the remaining multi-hundred-millisecond loop spikes. No 60 Hz completion claim is made yet.
