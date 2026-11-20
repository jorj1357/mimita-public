# Multi-client dev-loop testing

**Purpose:** run one server and multiple clients from the dev loop, see all
processes in the dashboard, and verify that clients interact on the same server.

Use `docs/specs/dev-loop/multi-instance.md` for the state model and
`docs/specs/networking/bad-connection-simulation.md` for impairment presets.

Agents run this workflow from the shared checkout root dev-loop. The root
dev-loop is the only build/run authority: saved compiler inputs trigger
publication automatically; agents do not create lanes, request builds, invoke
compilers, or wait/poll. Before launch, confirm the published artifact is
current for the edited source generation. Each run has a build identity and a
separate journal for every server/client instance. See
[`../specs/dev-loop/agent-lanes-build-authority.md`](../specs/dev-loop/agent-lanes-build-authority.md)
and [`runtime-scenario-validation.md`](runtime-scenario-validation.md).

## Start an isolated agent test run

Use this when another live run, such as the map editor, must stay open. The
controller selects the current published EXE and starts a loopback server and
client in a new run directory. All parallel runs share that immutable artifact;
each has its own run ID, process pair, command queue, and journals.

```text
python devscripts/dev-loop.py --start-test-run --test-profile tdm-validation --test-launch-mode 41
python devscripts/dev-loop.py --list-test-runs
python devscripts/dev-loop.py --run-id <run-id> --client client-01 --command "agent.snapshot melee-check"
python devscripts/dev-loop.py --stop-test-run <run-id>
```

The start command refuses a stale/unvalidated EXE or an unavailable gameplay
DLL. Record the returned `run_id`, pinned EXE SHA-256/build ID, DLL generation,
and exact server/client journal paths. `--stop-test-run` checks process start
time and executable against the manifest; it does not stop the primary run.
This is independent runtime isolation, not an independent build or executable.

## Start one server and two clients

```text
python devscripts/dev-loop.py --launch-mode 41 --start-server
python devscripts/dev-loop.py --start-client --server server-01 --name ClientA
python devscripts/dev-loop.py --start-client --server server-01 --name ClientB
python devscripts/dev-loop.py --list-instances
```

The first command starts the server and the primary client (`client-01`).
`start-client` adds one more client with its own journal, command queue, and
bad-connection preset.

Do not launch against a stale artifact to claim the edit passed. Record the
artifact ID, executable hash/path, source generation, run ID, and the exact
`EVENTS_JSONL_PATH` for the server and every client. Verify the client and
server build IDs match before interpreting combat outcomes.

Dashboard:

```text
python devscripts/dev-loop.py --dashboard
```

It shows the server/client counts, one row per process with PID, state, uptime,
snapshot age, and journal path, plus Start server, Start client,
Stop selected, and Restart selected.

## Verify the clients see each other

Read each client journal:

```powershell
$p='logs/<date>/<run>/client-01/events.jsonl'
Select-String -LiteralPath $p -Pattern 'network.client.snapshot.applied' | Select-Object -Last 1
```

Expected for two clients:

- `remote_players: 1` in each client journal (each sees the other).
- `remote_npcs` matches the server's NPC count.
- `snapshots_missed: 0` during healthy play.
- both client rows are `online` with a fresh `snapshot_age_ms`.
- server `roster_count: 2`.

Send one command to each client:

```text
python devscripts/dev-loop.py --client client-01 --command "agent.snapshot A"
python devscripts/dev-loop.py --client client-02 --command "agent.snapshot B"
```

## Bad-connection tests

Start the impaired client with a preset:

```text
python devscripts/dev-loop.py --start-client --server server-01 --name Impaired --badconn-preset 1
```

Preset families:

| Preset | Condition |
|---|---|
| 1 | Antipodal bad internet (latency + loss + reorder + rare blackout) |
| 2 | High steady latency |
| 3 | Packet loss bursts |
| 4 | Reordering |
| 5 | Severe combined |
| 6 / 7 | 5 s / 15 s blackouts |
| 8 / 9 | Worse / 1.5x variants |
| 10 | Immediate 5 s blackout for reconnect testing |

Live change in that client's terminal:

```text
badconn 3
badconn 0
```

Expected evidence:

- healthy client stays `online`;
- impaired client records delay/loss/reorder/blackout metrics;
- `network.snapshot.timeout` and `network.session.health` appear when snapshots
  stop;
- the client either reconnects or is shown as `frozen`/`stale`, never silently
  replaying the last frame;
- roster and death/respawn recover after reconnect;
- server fixed tick remains within budget.

## Stop or restart one instance

```text
python devscripts/dev-loop.py --stop-instance client-02
python devscripts/dev-loop.py --restart-instance client-02
python devscripts/dev-loop.py --stop-instance server-01
```

Stopping one instance must not stop the others. Check `--list-instances` after
each action.

## Troubleshooting

- `frozen` client: its server is gone or not sending snapshots. Read that
  client's journal and compare `snapshot_age_ms` with the server state.
- `mismatch`: check build ID, port, room, and journal path in `run.json`.
- `offline`/`failed`: read that instance's journal and `exit_code`.
- Two servers: confirm each client's `server_instance_id` and never assume a
  console `hz=60` line belongs to the client you are watching.

## Evidence rules

Report source, build, each instance journal, and human visual review separately.
Build success is not gameplay acceptance.
