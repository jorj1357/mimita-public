# REG: client looked frozen while a different server kept reporting 60 Hz

Date: 2026-11-04
Status: fixed in source; live journal proof recorded
Owner: dev-loop run presentation / client snapshot health

## Symptom

The player saw NPCs jitter and the round timer stop while the console kept
printing `hz=60.6` and NPC simulation activity. The game looked frozen even
though a server was running at 60 Hz.

## Evidence before the fix

Run journal `logs/2026-11-04/20261104_135620/events.jsonl`:

- Client `12344` connected to `127.0.0.1:57569` at `13:56:26`.
- At `13:58:49` the same client connected to `127.0.0.1:1357` (server PID `3988`).
- At `14:02:51` the client logged
  `network.reconnect.resource-probe teardown-before/after` and
  `runtime.objects.cleared`.
- Server `3988` wrote its last event at `14:02:52` and was no longer running.
- Server `24208` (the dev-loop server) was still alive at 60 Hz but showed
  `players=0`.
- The client had no `network.client-snapshot-window` events after teardown and
  no `network.session.health` loss event.

The 60 Hz line belonged to a different, playerless server. The client was
replaying its last snapshot.

## Root cause

The run had two servers and one shared journal. The client switched to the old
server (`1357`), that server died, and the client silently kept rendering the
last snapshot. There was no per-instance identity, no snapshot watchdog, and no
dashboard row showing which server the client was on.

## Fix

- Per-executable journals plus `run.json` manifest under one run folder.
- Every record carries `run_id`, `instance_id`, `role`, `pid`, `launch_id`.
- Dashboard/status rows for each server/client with state, PID, journal, bind,
  room, snapshot age, and connected server.
- `network.snapshot.timeout` when authoritative snapshots stop.
- `network.session.health` records loss/reconnect transitions.
- Client state becomes `frozen` when its server is gone and snapshots stop.

## Live proof

Run `logs/2026-11-04/20261104_142829/`:

- `run.json` lists `server-01` and `client-01` with separate journals.
- `server-01/events.jsonl` and `client-01/events.jsonl` are distinct.
- `logger.started` reports `run_id=20261104_142829`, `instance_id=server-01`
  and `instance_id=client-01`.
- After `stop-server`, the dashboard/manifest reported
  `server-01=stopped` and `client-01=frozen` with `snapshot_age_ms≈21591`.
- The client journal contains `network.snapshot.timeout` and
  `network.session.health` reconnect transitions.

Human visual review of the dashboard rows remains pending.
