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
