# REG: client looked frozen while a different server kept reporting 60 Hz

Date: 2026-11-04
Time last updated: 2026-11-05T20:10:26Z
Status: ATTEMPTED FIX (2) — controlled blackout runtime proof passed; human review pending
Owner: dev-loop run presentation / client snapshot health

Related changelog: `docs/changelog/2026-11-05/20261105_201026-networking-tick-stall-investigation.md`

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

---

## Regression Occurrence 2 — Snapshot starvation did not enter reconnecting

### Observed

Time: `2026-11-04T15:10:23Z`

Run: `logs/2026-11-04/20261104_151006/`

### Expected Behavior

When authoritative snapshots stop beyond the stale threshold, the client
enters `Reconnecting`, even if unrelated packets continue to refresh the
general heartbeat. A successful reconnect must then be confirmed by a later
`Connected` transition and fresh snapshots.

### Actual Behavior

Both client journals logged `network.snapshot.timeout` with snapshot age about
6172 ms, stale threshold 2500 ms, hard timeout 10000 ms, and last snapshot tick
323. Each journal shows only the initial `Connecting -> Connected` health
transition. This archive does not show an ensuing reconnect attempt or a
permanent failure: both clients later resumed receiving snapshots. It therefore
does not prove this was the user's “never reconnects” incident.

### Confirmed Cause

The source state machine did not use snapshot starvation as an independent
reconnect condition. The packet handler could select `WeakConnection` while
the general heartbeat was recent, and the fallback only forced reconnect when
the selected state equaled the previous state. A timeout could therefore be
logged without entering `Reconnecting`. This is a confirmed source logic
defect; the surviving run does not prove it caused its later stale-client
manifest entries.

### Incorrect Code

Before correction, `src/network/multiplayer-packets.cpp:1909` treated only
`Connected` as snapshot-starved:

```cpp
const bool snapshotStarved =
    before == ConnectionState::Connected &&
    ctx.lastSnapshotReceivedMs > 0 && snapshotAge > staleThreshold;
```

It then used this fallback after evaluating the packet-age state machine:

```cpp
ConnectionState next = mpNextConnectionHealth(
    before, now, lastHeardAge, heardSinceDisconnect,
    staleThreshold, hardTimeout, ctx.reconnectGraceDeadlineMs, graceMs);
if (snapshotStarved && next == before)
    next = ConnectionState::Reconnecting;
```

When a connected client had a recent non-snapshot packet, the state machine
selected `WeakConnection`; because that differed from `before`, the fallback
did not enter `Reconnecting`.

### Corrected Code

Current code includes both `Connected` and `WeakConnection` in the snapshot
starvation condition at `src/network/multiplayer-packets.cpp:1909` and passes
it to `mpNextConnectionHealth()` at line 1939:

```cpp
const bool snapshotStarved =
    (before == ConnectionState::Connected ||
     before == ConnectionState::WeakConnection) &&
    ctx.lastSnapshotReceivedMs > 0 && snapshotAge > staleThreshold;
ConnectionState next = mpNextConnectionHealth(
    before, now, lastHeardAge, heardSinceDisconnect,
    staleThreshold, hardTimeout, snapshotStarved,
    ctx.reconnectGraceDeadlineMs, graceMs);
```

At `src/network/connection-health.cpp:34`, both states return
`Reconnecting` when `snapshotStarved` is true.

### Fix and Proof

The fix is in the source and compiled into dev-loop artifact
`2026-11-05T20:05:47.562559Z` (SHA-256
`9a89b2f24e8f01e6306d5847a4832cbf0b191f9156d21ff527d212003fdf0183`). Fresh
verification used run `20261105_200615`; client-02 used bad-connection preset
10 (Immediate 5 second blackout). At `20:07:14.103Z`, after a 6017 ms snapshot
gap, it logged `Connected -> Reconnecting` (attempt 1); at `20:07:14.189Z` it
logged `Reconnecting -> Connected` after snapshots resumed. The server accepted
the reconnect at `20:07:14.107Z`. This verifies the failure transition and
recovery under the controlled blackout. Human visual review remains open; this
does not prove that the old two-server incident or a permanent reconnect
failure had the same cause.

### Reproducibility

In launch mode 45 on `dust2cyberiav4`, start a second client with
`python devscripts/dev-loop.py --start-client --server server-01 --name RegressionCheck --headless --badconn-preset 10`.
Preset 10 is the configured Immediate 5 second blackout. The latest proof's
per-client journal is
`logs/2026-11-05/20261105_200615/client-02/events.jsonl`; its exact
server/client build ID is in that run's `run.json`.
