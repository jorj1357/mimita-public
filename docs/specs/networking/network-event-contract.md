# Network event contract

- Status: normative implementation table for the connection event chain.
- Owner: networking hot code.
- Companion: `docs/specs/networking/networking.md` lifecycle section.
- This table does NOT define packet layouts, replace the networking
  specification, or claim runtime evidence.

Every event uses the common envelope:

`correlation_id`, `connection_id`, `attempt_id`, `role`, `transport`,
`local_endpoint`, `remote_endpoint`, `player_id`, `server_slot_state`,
`token_fingerprint`, `generation`, `build_id`, `config_revision`,
`client_tick`, `server_tick`, `snapshot_tick`, `sequence`, `duration_ms`,
`queue_depth`, `bytes`, `chunks`, `last_heard_age_ms`, `snapshot_age_ms`,
`stale_threshold_ms`, `hard_timeout_ms`, `grace_remaining_ms`, `attempt`,
`max_attempts`, `interval_ms`, `backoff_ms`, `stage`, `result`, `reason_code`,
`next_expected_event`.

StructuredLogger supplies `run_id`, `pid`, `instance_id`, `event_id`,
`wall_time`, `source`, `line`, and `func`. Raw reconnect tokens are never
logged; only a configured fingerprint prefix.

## Startup, transport, and join

| Event | Role | Owner | Required fields | Next |
|---|---|---|---|---|
| `network.config.loaded` | both | config loader | path, revision, changed keys | `network.config.reloaded` or connect |
| `network.config.reloaded` | both | config loader | old/new revision, changed keys, result | health/connect |
| `network.client.connect.requested` | client | menu/connect policy | mode, endpoint or room, map, name, password flag | preflight |
| `network.client.connect.preflight` | client | connect policy | build/generation, map present, config valid | transport.open |
| `network.client.transport.open` | client | transport policy | transport, local endpoint | ice.state / ready |
| `network.client.ice.state` | client | ICE policy | state, candidate, elapsed | transport.ready/failed |
| `network.client.transport.ready` | client | transport policy | remote endpoint, path, setup_ms | hello.sent |
| `network.client.transport.failed` | client | transport policy | stage, reason, OS error, endpoint | retry/failed |
| `network.client.transport.closed` | client | transport policy | reason, bytes, queue | disconnect/terminal |
| `network.server.transport.open` | server | server transport policy | bind, endpoint | transport.ready |
| `network.server.transport.ready` | server | server transport policy | local endpoint, transport | join.received |
| `network.server.transport.closed` | server | server transport policy | reason, bytes | none |
| `network.client.hello.sent` | client | join policy | attempt, bytes, server | join.request_sent |
| `network.client.join.request_sent` | client | join policy | attempt, bytes, token present | server join.received |
| `network.client.join.accepted` | client | join policy | assigned player, map, build, spawn | map.sync.start |
| `network.client.join.rejected` | client | join policy | reason code, server, map | terminal/retry |
| `network.server.join.received` | server | server join policy | source, bytes, transport | join.validated |
| `network.server.join.validated` | server | server join policy | name, map, password, identity | join.accepted/rejected |
| `network.server.join.accepted` | server | server join policy | player ID, slot, map | player.registered |
| `network.server.join.rejected` | server | server join policy | reason code, slot state | none/retry |
| `network.server.player.registered` | server | session policy | player ID, slot, name, map, team | spawn.assigned |
| `network.client.map.sync.start` | client | map sync policy | map, revision | map.loaded |
| `network.client.map.loaded` | client | map sync policy | map, bytes, duration | map.ready_sent |
| `network.client.map.load_failed` | client | map sync policy | map, reason | retry/terminal |
| `network.client.map.ready_sent` | client | map sync policy | map, player, attempt | server map.ready_received |
| `network.server.map.ready_received` | server | map sync policy | player, map | spawn.assigned |
| `network.server.spawn.assigned` | server | spawn policy | player, spawn generation, position | client spawn.applied |
| `network.client.spawn.applied` | client | spawn policy | player, spawn generation, map | snapshot.received |

## Snapshot and heartbeat

| Event | Role | Owner | Required fields | Next |
|---|---|---|---|---|
| `network.server.snapshot.build` | server | snapshot policy | tick, entities, chunks, bytes, build_ms | snapshot.send |
| `network.server.snapshot.send` | server | snapshot policy | tick, chunks, bytes, copies, failures | snapshot.received |
| `network.client.snapshot.received` | client | snapshot policy | tick, sequence, chunks, bytes, gap | snapshot.applied |
| `network.client.snapshot.applied` | client | snapshot apply | tick, entities, apply_ms, delay | health/metrics |
| `network.client.snapshot.gap` | client | health/snapshot policy | expected/actual tick, missed, age, cause | health.waiting |
| `network.client.snapshot.recovery` | client | health policy | snapshot tick/age, reconnect suppressed | health transition |
| `network.metrics.window` | both | observability policy | packets, bytes, RTT, loss, snapshot Hz, tick Hz, queue | next window |
| `packet.route` | both | packet route census | packet ID, direction, path | none (one-shot census) |

## Health and reconnect

| Event | Role | Owner | Required fields | Next |
|---|---|---|---|---|
| `network.session.health` | client | health policy | from/to, trigger, ages, thresholds, grace, attempt, false-positive | reconnect/scheduled or connected |
| `network.connection-state.changed` | client | notification policy | from/to, reason, UI title/message | same chain |
| `network.client.health.waiting` | client | health policy | snapshot age, hard timeout, last tick | reconnect.scheduled |
| `network.server.peer.health` | server | server session policy | player, silence, stale/lost/recovered, slot state | reconnect/timeout |
| `network.client.reconnect.scheduled` | client | reconnect policy | reason, next attempt, interval, grace | reconnect.attempt |
| `network.client.reconnect.attempt` | client | reconnect policy | attempt, send result, interval, backoff, elapsed, grace | server reconnect.received |
| `network.client.reconnect.accepted` | client | reconnect policy | player, map, spawn, token rotated | snapshot.recovery |
| `network.client.reconnect.rejected` | client | reconnect policy | server reason, token state, slot state | attempt/terminal |
| `network.client.reconnect.terminal` | client | reconnect policy | elapsed, grace deadline, attempts sent, reason | none |
| `network.server.reconnect.received` | server | server reconnect policy | player, token fingerprint, transport | reconnect.lookup |
| `network.server.reconnect.lookup` | server | server reconnect policy | token state, previous-token match, slot state | accepted/rejected |
| `network.server.reconnect.accepted` | server | server reconnect policy | player, slot state, token state, copies | client join.accepted |
| `network.server.reconnect.rejected` | server | server reconnect policy | token fingerprint/state, slot state, players | client attempt |
| `network.server.reconnect.expired` | server | server reconnect policy | player, silence, grace deadline | player.removed |
| `network.client.disconnect.requested` | client | session policy | reason | disconnect.sent |
| `network.client.disconnect.sent` | client | session policy | bytes | transport.closed |
| `network.server.disconnect.received` | server | session policy | player, reason | player.removed |
| `network.server.player.removed` | server | session policy | player, reason, slot state | none |
| `network.server.timeout.stale` | server | timeout policy | player, silence, deadline | peer.health |
| `network.server.timeout.expired` | server | timeout policy | player, silence, grace deadline, slot released | player.removed |

## Hot swap

| Event | Role | Owner | Required fields | Next |
|---|---|---|---|---|
| `game.generation.swap_begin` | host | host loader | old/new generation, swap ID, connections | hotswap.suspend |
| `game.generation.candidate_loaded` | host | host loader | generation, path, duration | old_shutdown |
| `game.generation.old_shutdown` | host | host loader | generation, duration | new_init |
| `game.generation.new_init` | host | host loader | generation, duration | first_tick |
| `game.generation.first_tick` | host | host loader | generation, tick | first_snapshot |
| `game.generation.first_snapshot` | host/game | network policy | generation, snapshot tick/age | swap_end |
| `game.generation.swap_end` | host | host loader | generation, duration, connections | hotswap.resume |
| `network.hotswap.suspend` | both | hot-swap policy | connection, snapshot age, queue | resume |
| `network.hotswap.resume` | both | hot-swap policy | duration, packets drained | health_rebased |
| `network.hotswap.health_rebased` | client | health policy | old/new ages, reason | snapshot.recovery |
| `network.hotswap.queue_drained` | both | transport policy | messages, bytes, overflow | normal tick |

Until the resident transport pump exists, the game-side first tick emits
`network.hotswap.queue_drained` with `result=skipped`,
`reason_code=resident_pump_not_installed`, and zero messages. That is an honest
placeholder, not proof that a queue was drained.

## Rate rules

- Transitions, errors, accepts, rejects, terminal states, and swap phases always
  emit.
- Per-packet events only for gaps, duplicates, reorders, rejects, send failures,
  and sampled packets.
- Snapshot diagnostics and metrics are bounded by
  `network_observability.snapshot_diagnostics_interval_ms` and
  `network_observability.metrics_window_ms`.
- `network_observability.max_events_per_second` is the hard ceiling.
- No new event may add a second journal, raw token, or unconfigured rate.
