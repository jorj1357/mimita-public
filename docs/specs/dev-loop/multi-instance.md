# Dev-loop multi-instance topology and presentation

**Status:** Desired behavior contract for one dev-loop run that owns one or
more servers and clients.

## Purpose

The dev loop must let the human and an AI answer these questions at a glance:

- Which server instances are running?
- On which bind/port and room code?
- Which client instances are running?
- Which server is each client connected to?
- Which instances are actually online, stale, frozen, failed, or mismatched?
- Which per-executable journal belongs to each instance?
- Where is the first divergence when one instance fails?

The current one-server/one-client status strings are insufficient. A live
`hz=60` line from any server must never be presented as proof that a client is
healthy. The presentation must show the server and the client independently.

## Topology

One dev-loop run owns a **run manifest** and zero or more **instances**.

```text
run
├── manifest (run.json)
├── dev-loop instance (daemon/watchdog/dashboard journal)
├── server-01
├── client-01  -> server-01
├── client-02  -> server-01
├── server-02
└── client-03  -> server-02
```

An instance is one executable process. `server` and `client` are the gameplay
roles. `daemon`, `watchdog`, and `dashboard` are dev-loop roles.

Every instance has a stable `instance_id` within the run, for example
`server-01` or `client-02`. IDs are never reused within a run. A client names
the `server_instance_id` it is configured to connect to.

## Run manifest

The run manifest is the single index for the run. It is written atomically and
is readable while the run is active.

Minimum fields:

```json
{
  "schema_version": 1,
  "run_id": "20261104_135620",
  "checkout_root": "C:/mimita-v9",
  "created_utc": "...",
  "updated_utc": "...",
  "build_id": "...",
  "artifact_id": "...",
  "source_generation": 0,
  "journal_root": "logs/2026-11-04/20261104_135620",
  "instances": [
    {
      "instance_id": "server-01",
      "role": "server",
      "label": "Fun CS server",
      "pid": 24208,
      "executable": "C:/mimita-v9/.dev/current/mimita.exe",
      "build_id": "...",
      "launch_id": "...",
      "started_utc": "...",
      "state": "online",
      "bind": "127.0.0.1:57569",
      "room_code": "DL57B3W",
      "map": "dust2cyberiav4",
      "mode": "funcounterstrike",
      "journal_path": "logs/2026-11-04/20261104_135620/server-01/events.jsonl",
      "last_heartbeat_utc": "...",
      "last_event_utc": "...",
      "roster_count": 1,
      "build_match": true,
      "exit_code": null,
      "reason": ""
    },
    {
      "instance_id": "client-01",
      "role": "client",
      "label": "NPC Dev",
      "pid": 12344,
      "executable": "C:/mimita-v9/.dev/current/mimita.exe",
      "build_id": "...",
      "launch_id": "...",
      "started_utc": "...",
      "state": "frozen",
      "server_instance_id": "server-01",
      "connected_address": "127.0.0.1:1357",
      "journal_path": "logs/2026-11-04/20261104_135620/client-01/events.jsonl",
      "last_heartbeat_utc": "...",
      "last_event_utc": "...",
      "last_snapshot_tick": 10849,
      "last_snapshot_utc": "...",
      "snapshot_age_ms": 4200,
      "build_match": true,
      "exit_code": null,
      "reason": "no_snapshot_after_server_exit"
    }
  ]
}
```

## Per-executable journals

Each executable writes its own canonical journal:

```text
logs/<yyyy-mm-dd>/<run-id>/
  run.json
  dev-loop/events.jsonl
  server-01/events.jsonl
  client-01/events.jsonl
  client-02/events.jsonl
```

Rules:

- One process writes to one instance journal. Two servers never share a file.
- Every record includes `run_id`, `instance_id`, `role`, `pid`, `launch_id`,
  and `build_id` when available.
- `StructuredLogger` stays the only writer.
- Rotation uses `events-NNNN.jsonl` inside the instance folder.
- The run-level quota covers the whole run folder. The manifest records each
  journal's current bytes.
- Readers merge instance journals by UTC wall time and correlate by
  `run_id`/`instance_id`, `actor_id`, `entity_id`, `projectile_id`, and tick.
- `--versioninfo` reports the exact journal path for that process.
- A standalone launch without a run manifest keeps the existing per-process
  `events.jsonl` behavior.

## Instance states

The dashboard and status JSON use these states:

| State | Meaning |
|---|---|
| `starting` | Process exists but no heartbeat or readiness evidence yet. |
| `online` | Heartbeat fresh. Servers also require a live roster; clients also require advancing snapshots. |
| `stale` | Process alive, but heartbeat or snapshot evidence is older than the configured threshold. |
| `frozen` | Client process alive and rendering, but no authoritative snapshot for the threshold while its server has no matching player. |
| `offline` | Instance is not running and was not intentionally stopped in this run. |
| `stopped` | Explicitly stopped by the human or agent. |
| `failed` | Process exited with a non-zero code or crashed. |
| `mismatch` | Build, executable, bind/port, room, or journal identity differs from the manifest. |

A server is not proof of client health. A client is not proof of server health.
Each row carries its own evidence.

Required evidence fields per state:

- `last_heartbeat_utc` and `heartbeat_age_ms`.
- `last_event_utc` and `event_age_ms`.
- For clients: `last_snapshot_tick`, `last_snapshot_utc`, `snapshot_age_ms`.
- For servers: `roster_count`, `bind`, `room_code`.
- `build_id` and `build_match`.
- `exit_code` and `reason` when not online.

## Dashboard presentation

The native dashboard is the primary presentation. The terminal dashboard is a
readable fallback with the same data.

### Process table

One row per instance, grouped by role, with columns:

```text
INSTANCE     ROLE    STATE     PID    BUILD   MAP/MODE            PORT/ROOM      ROSTER  SNAP AGE  UPTIME   JOURNAL
server-01    server  online    24208  a1b2c3  dust2/funcs        57569/DL57B3W  1       —         00:12:31 server-01/events.jsonl
client-01    client  frozen    12344  a1b2c3  dust2/funcs        ->server-01    —       42.1s     00:12:29 client-01/events.jsonl
```

### Summary

```text
2 servers online · 2 clients online · 1 frozen · 1 offline
```

The summary must count by state, not by process liveness alone.

### Selected instance

Selecting a row shows:

- Full executable path, PID, role marker, script path when Python.
- Build ID, launch ID, source generation.
- Bind/port, room code, map, mode.
- `connected_to` server instance for clients.
- Heartbeat age, last event, last snapshot tick/age, roster ids/count.
- Journal path with copy/open actions.
- Last reason/error/exit code.
- Per-instance actions: stop, restart, focus window, open journal.

### Visual states

- Green: `online`.
- Yellow: `starting`, `stale`.
- Blue: `frozen`.
- Red: `failed`, `mismatch`.
- Grey: `offline`, `stopped`.
Color never carries meaning alone; the text state is always present.

### Required warnings

- A server row may say `online` while a client row says `frozen`.
- If a client is connected to a server instance that is not in the manifest,
  show `mismatch` with both addresses.
- If two servers exist, the client row must name its `connected_to` instance.
- If a client has no snapshots for the threshold, show `frozen` even when its
  process is alive and its `network.session.health` says `Connected`.

## Start/stop contract

The target command surface, implemented incrementally:

```text
dev-loop.py --start-server --launch-mode <id> --label <name>
dev-loop.py --start-client --server <instance_id> --label <name>
dev-loop.py --stop-instance <instance_id>
dev-loop.py --restart-instance <instance_id>
dev-loop.py --list-instances [--json]
```

Rules:

- Each server instance owns one bind/port and one room code.
- Each client instance names exactly one `server_instance_id`.
- Starting a second server must not reuse a port or mix its journal with the
  first.
- Stopping one instance must not stop unrelated instances.
- Duplicate `instance_id` is rejected.
- The dashboard uses the same control requests as the CLI.
- No cross-run adoption. A process from another run is reported, never adopted.

## Required events

Dev-loop journal:

- `dev-loop.instance.created`, `dev-loop.instance.started`,
  `dev-loop.instance.state`, `dev-loop.instance.exited`,
  `dev-loop.instance.stopped`, `dev-loop.instance.mismatch`.
- `dev-loop.client.frozen` when a client stops receiving snapshots while alive.
- `dev-loop.identity.mismatch` when build/port/room/journal identity differs.

Game journals:

- `logger.started` with `instance_id`, `role`, `pid`, `build_id`, `journal`.
- `network.snapshot.timeout` from the client with last snapshot tick and age.
- `network.session.health` on every transition, including loss.
- `server.player.registered`, `server.player.unregistered`,
  `server.player.stale` with roster count.

## Runbook: what to do better

1. Read the process table first. Do not infer the active server from a console
   line or the newest journal file.
2. If a client is `frozen`, compare its `snapshot_age_ms` with its server's
   `roster_count` and `state`.
3. If two servers exist, confirm the client's `connected_to` instance, port, and
   room code.
4. If `mismatch`, compare build ID, launch ID, executable, port, room, and
   journal path before acting.
5. If `offline` or `failed`, read that instance's journal and `exit_code`; do
   not read a different instance's journal as a substitute.
6. Never report `hz=60` from a server as client acceptance. Report server tick
   evidence and client snapshot evidence separately.
7. When a client reconnects, record the old server instance, new server instance,
   reason, and snapshot gap.

## Acceptance

- A run with two servers and two clients shows all four rows with independent
  states, ports, rooms, and journal paths.
- Killing server A while client A is connected produces `failed` for A and
  `frozen`/`stale` for client A, while server B and client B remain `online`.
- No two executables write to the same instance journal.
- `run.json` lists every instance and its journal before and after the run.
- The dashboard never reports a client as healthy without a recent snapshot.
- The journal records the first divergence for each failed/frozen instance.
