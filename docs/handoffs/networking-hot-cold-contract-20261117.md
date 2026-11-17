# Networking hot/cold contract handoff (2026-11-17)

- Status: approved coordination brief. This is not a specification.
- Owners: Agent A = ICE hot/cold split; Agent B = reconnect, health, events,
  config, trace, client transport continuity. These labels apply to this brief
  only; map them to the humans/agents currently working.
- Authority: `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/hotreload/hotreload.md`,
  `docs/specs/networking/networking.md`, and
  `docs/handoffs/hot-reload-agent-focus.md`. If this brief disagrees with those
  documents, the authoritative documents win.
- Goal: one running EXE, hot MiMITA networking policy, resident mechanism only,
  connection survives generation swaps, and every connection step is followable
  in `events.jsonl`.
- Audience: the two agents doing this networking split, plus the human
  integrator who resolves shared-file conflicts.

This brief does NOT replace the specifications, does NOT approve a cold ABI
change by itself, does NOT define packet layouts, and does NOT claim build,
runtime, or human acceptance evidence.

## 1. Non-negotiable working rules

- Work in the existing shared checkout. No branches, worktrees, mode-2 lanes,
  or second checkouts.
- One writer per shared file. Do not edit the same file at the same time.
- The root dev-loop is the only build authority. Agents edit and save; they do
  not run compilers, request builds, or wait for builds.
- Do not create claim JSON files or lock files. This brief plus the user's
  coordination is the claim protocol.
- Preserve unrelated edits and do not rewrite another agent's evidence.
- A cold change, `src/host/abi/**` change, or new resident behavior requires an
  explicit note in this handoff before implementation.
- Do not move performance-owned code into this work. The other performance agent
  owns simulation, physics, rendering, and frame pacing.

## 2. Target boundary

### Resident mechanism (cold)

- libjuice agent, callback thread, socket/WSA syscalls, OS timers.
- Raw socket/ICE handles, bounded receive/send queues, wakeups, pump thread.
- Process/window/loader lifetime and the transport ABI.
- Generation/swap state, raw transport events, and raw counters.
- Maintenance payload storage and transmission during a swap.
- No join, reconnect, health, token, map, snapshot, or player-slot decisions.

### Hot MiMITA policy

- Packet IDs, tombstones, schemas, encode/decode, handlers, routing.
- HELLO/JOIN/WELCOME, map sync, spawn, session lifecycle.
- Reconnect, retry, grace, terminal failure, health, snapshot starvation.
- Token lookup, accept/reject, server player slots, peer expiry policy.
- ICE negotiation policy, ICE listener policy, peer admission, room publication.
- Coordinator policy, server-browser policy, room-code/HUD policy.
- Snapshot build/publish/apply policy, movement validation, lag compensation.
- All semantic `network.*` events, event fields, levels, and rate limits.
- All tunable timings, rates, thresholds, and limits in
  `config/networkingconfig.json`.

Resident code may expose state and move bytes. It must not duplicate hot policy
or make a game decision.

## 3. File ownership

### Agent A — ICE hot/cold split

Owns:

- `src/network/server-ice.cpp`
- `src/network/ice/ice-agent.cpp`
- `src/network/ice/ice-server.cpp`
- ICE policy extraction from `src/network/server.cpp`
- ICE cleanup in `src/network/net_common.cpp`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/hotreload/hotness-matrix.md`
- ICE rows in `config/source-owners.json`
- ICE dependency audit and boundary evidence

Deliverables:

- ICE negotiation policy hot: STUN/TURN, SDP exchange, candidate filtering,
  gather/connect retry and timeout, state interpretation.
- ICE listener policy hot: pending peers, admission, expiry, peer mapping,
  listener publication, room/session publication.
- Resident libjuice/socket/callback-thread primitive only.
- Reattachment by stable connection/session ID after a generation swap.
- No `juice_destroy` because the game DLL unloaded.
- Evidence that a resident ICE session survives a game DLL swap.

### Agent B — reconnect, health, events, config, trace, client continuity

Owns:

- `src/network/connection-health.*`
- Reconnect and health functions in `src/network/multiplayer-packets.cpp`
- Reconnect/token event fields in `src/network/server-packets.cpp`
- `src/config/networking-config.*`
- `config/networkingconfig.json`
- New `src/network/network-events.*` helper
- `devscripts/network_trace.py`
- `docs/specs/networking/network-event-contract.md`
- `docs/regressions/networking/` records created by this work
- Client resident transport core integration in
  `src/host/host-transport-adapter.*` and `src/host/host-runtime.*`

Deliverables:

- All reconnect/health timing from config, with no hardcoded values.
- Correct attempt cadence, grace, and terminal-failure classification.
- Snapshot-starvation and hot-swap suppression/rebase.
- Full event chain and `events.jsonl` trace script.
- Client session survives a swap with queued packets and no reconnect.
- Server log-bridge crash regression record and networking index update.

### Shared files, sequential only

- `docs/specs/networking/networking.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/hotreload/hotness-matrix.md`
- `src/host/abi/transport-capability.h`
- `src/host/host-runtime.*`
- `src/network/server.cpp`
- `config/source-owners.json`

Order: Agent A lands the boundary and ICE ownership first. Agent B reviews and
appends the event lifecycle and exhaustive contract. Do not both edit these
files in the same window.

## 4. Event contract

### Two tiers

Resident raw events use a fixed generic envelope and must not contain MiMITA
policy:

- `host.transport.*`
- `host.generation.*`
- `host.swap.*`

Hot semantic events use `network.*` and are emitted only from game DLL code.

### Common hot envelope

Every semantic network event carries:

- `correlation_id`: `conn:<connection_attempt_id>`,
  `reconnect:<attempt_id>`, `snap:<server_tick>`, or `swap:<generation>`.
- `connection_id`: stable across reconnect and swap.
- `attempt_id`: one increment per reconnect attempt, never per frame.
- `stage`: `requested`, `started`, `validated`, `sent`, `received`,
  `accepted`, `rejected`, `applied`, `expired`, `failed`, `skipped`.
- `result`: `ok`, `retry`, `degraded`, `terminal`, `intentional_swap`.
- `reason_code`: stable machine name.
- `role`, `transport`, `local_endpoint`, `remote_endpoint`.
- `player_id`, `server_slot_state`, `token_fingerprint` (never raw token).
- `generation`, `old_generation`, `build_id`, `config_revision`.
- `client_tick`, `server_tick`, `snapshot_tick`, `sequence`.
- `duration_ms`, `queue_depth`, `bytes`, `chunks`.
- `last_heard_age_ms`, `snapshot_age_ms`, `stale_threshold_ms`,
  `hard_timeout_ms`, `grace_remaining_ms`.
- `attempt`, `max_attempts`, `interval_ms`, `backoff_ms`.
- `next_expected_event`.

StructuredLogger already adds `run_id`, `pid`, `instance_id`, `event_id`,
`wall_time`, `source`, `line`, and `func`. Network events must not use `line 0`
or anonymous `printf` as their only evidence.

### Event ownership

Agent A emits from hot ICE policy:

- `network.client.ice.state`
- `network.ice.path`
- `network.server.ice.listener.*`
- `network.server.ice.peer.*`
- `network.server.ice.candidate.*`
- `network.server.room.publish.*`
- `network.server.browser.*` when browser policy is touched

Agent B emits:

- `network.config.loaded`, `network.config.reloaded`
- `network.client.connect.requested`, `network.client.connect.preflight`,
  `network.client.connect.cancelled`
- `network.client.transport.open`, `.ready`, `.failed`, `.closed`
- `network.server.transport.open`, `.ready`, `.closed`
- `network.client.hello.sent`
- `network.client.join.request_sent`
- `network.server.join.received`, `.validated`, `.accepted`, `.rejected`
- `network.server.player.registered`
- `network.client.join.accepted`, `.rejected`
- `network.client.map.sync.start`, `.loaded`, `.load_failed`
- `network.client.map.ready_sent`
- `network.server.map.ready_received`
- `network.server.spawn.assigned`
- `network.client.spawn.applied`
- `network.server.snapshot.build`, `network.server.snapshot.send`
- `network.client.snapshot.received`, `.applied`, `.gap`, `.recovery`
- `network.metrics.window`
- `network.session.health` (existing name retained; add `event_kind`)
- `network.connection-state.changed` (notification/UI only)
- `network.client.health.waiting`
- `network.server.peer.health`
- `network.client.reconnect.scheduled`, `.attempt`, `.accepted`, `.rejected`,
  `.terminal`
- `network.server.reconnect.received`, `.lookup`, `.accepted`, `.rejected`,
  `.expired`
- `network.client.disconnect.requested`, `.sent`
- `network.server.disconnect.received`, `network.server.player.removed`
- `network.server.timeout.stale`, `.lost`, `.expired`
- `game.generation.swap_begin`, `.candidate_loaded`, `.old_shutdown`,
  `.new_init`, `.first_tick`, `.first_snapshot`, `.swap_end`
- `network.hotswap.suspend`, `.resume`, `.health_rebased`, `.queue_drained`

`packet.route` remains the one-shot packet-route census. It is not per-packet
history and must not be described as full packet coverage.

### Cardinality

- Always emit transitions, errors, accepts, rejects, terminal states, and swap
  phases.
- Per-packet events only for gaps, duplicates, reorders, rejects, send failures,
  and sampled packets.
- One `network.metrics.window` per second per role.
- All rates are config-driven. No hardcoded sample or event limits.

## 5. Config contract

`config/networkingconfig.json` is the single source for networking tunables.
Existing `retries` and `network_timeouts` keys stay compatible.

Add or confirm these keys:

```json
"retries": {
  "reconnect_initial_backoff_ms": 1000,
  "reconnect_max_attempts": 10,
  "reconnect_max_backoff_ms": 15000,
  "reconnect_grace_ms": 60000,
  "reconnect_interval_ms": 6000,
  "reconnect_send_immediate_first_attempt": true,
  "reconnect_terminal_requires_grace": true
}
```

Agent B owns the JSON edit. Agent A supplies the ICE values as a checklist; B
merges them in one edit so the file is never concurrently written. Reserved ICE
keys:

```json
"ice": {
  "gather_timeout_ms": 5000,
  "connect_timeout_ms": 10000,
  "retry_initial_ms": 1000,
  "retry_max_ms": 15000,
  "retry_max_attempts": 10,
  "listener_pending_ttl_ms": 30000,
  "listener_max_pending": 64,
  "listener_peer_expiry_ms": 60000,
  "publish_room_on_listener_ready": true
}
```

Agent B adds observability and swap policy:

```json
"network_observability": {
  "enabled": true,
  "heartbeat_hz": 1,
  "snapshot_sample_hz": 2,
  "packet_sample_hz": 5,
  "max_events_per_second": 200,
  "log_every_reconnect_attempt": true,
  "log_every_rejection": true,
  "log_queue_depth": true,
  "log_rtt": true,
  "token_fingerprint_chars": 8
},
"hot_swap": {
  "suspend_network_health": true,
  "rebase_after_swap": true,
  "require_fresh_snapshot_after_swap": true,
  "maintenance_interval_ms": 1000,
  "max_suspend_ms": 30000
}
```

Rules:

- JSON values override C++ defaults.
- Missing or invalid keys keep the previous committed value and emit
  `network.config.reloaded` with `result=failed` and the bad key.
- Every reload emits the active revision/hash and changed keys.
- No networking retry, timeout, grace, sample, or event-rate constant may remain
  in hot or resident networking source.

## 6. Resident transport core API

Proposed shape, subject to one ABI review with Agent A:

- `NetCoreSessionHandle` and `NetCorePeerHandle`: opaque `uint64_t`.
- `NetCoreSessionStateV1`: role, connected, peer count, player ID, local/remote
  endpoint, transport kind, RTT, last receive/send, queue depth, bytes in/out.
- `NetCoreSwapStateV1`: active generation, old generation, `swap_in_progress`,
  swap start, queue depth, overflow/drop counts.
- `NetCoreMessageV1`: session, peer, channel, typeId, flags, sequence, bytes,
  receivedAtMs. Payload is valid only for the call; hot code copies if needed.
- `NetCoreEventV1`: peer joined/left, timeout, reconnect-required, overflow,
  transport error, swap suspend/resume.
- `NetCorePreviewV1`: owner, peer, channel, sorted snapshot/event preview keyed
  by tick/sequence; no game state.
- `NetCoreCapabilityV1` methods:
  - `open_session`, `close_session`, `attach_session`
  - `send`, `broadcast`, `send_to_player`, `send_to_endpoint`
  - `poll_inbound`, `poll_events`
  - `query_session`, `query_metrics`, `query_swap_state`
  - `register_maintenance_payload`, `clear_maintenance_payload`

Invariants:

- No hot-DLL pointer, callback, STL type, socket, or `sockaddr` is retained in
  the resident core.
- No resident thread calls into the game DLL. It only queues bytes/events.
- The pump keeps running during quiesce, candidate load, old shutdown, and new
  generation initialization.
- On swap, the session handle and ICE agent stay alive. The new generation
  attaches, drains the queue, and resumes normal tick processing.
- The maintenance payload is copied into resident memory and sent at the
  configured interval while the game generation is absent. Server-side hot
  policy recognizes it and holds the slot.
- The new generation must not call `mpInit`, rotate the token, or reset the
  session just because it attached.
- If the queue overflows, emit a raw `host.transport.overflow` event and bound
  the loss; never call hot code.

## 7. Swap/reattach invariant

During an intentional swap:

1. Host records `swap_begin`, keeps the pump alive, starts maintenance payload.
2. Old generation is quiesced and shut down; no socket/ICE destruction.
3. New generation loads and initializes.
4. New generation attaches to the stable session handle.
5. Host records `swap_end`; game drains queued packets.
6. Health timers are rebased, not expired.
7. First fresh authoritative snapshot arrives.
8. Only then may the state become `Connected` again.

No reconnect is emitted for this path. `network.hotswap.suspend`,
`network.hotswap.resume`, and `network.hotswap.health_rebased` must show it.

## 8. Shared-file protocol

1. Agent A writes the hot/cold boundary and ICE ownership first. Stop for
   review.
2. Agent B writes the event contract, config key list, and resident-core API
   proposal. Stop for review.
3. Merge `networking.md`, `hotreload.md`, and `hotness-matrix.md` sequentially:
   A's boundary first, B's event lifecycle second.
4. Agent B owns `networkingconfig.json`; Agent A submits ICE keys as a checklist.
5. Agent A owns `server-ice.cpp` and ICE primitive files. B requests event hooks
   through the shared event helper; A adds them from hot ICE policy.
6. Agent B owns reconnect/health functions in `multiplayer-packets.cpp`. If ICE
   client policy needs the same file, split by function and hand off; never
   co-edit.
7. Agent B drafts the resident-core ABI and client pump; Agent A reviews the ICE
   handle/reattach fields. One co-authored ABI patch.
8. Only one agent runs the dev-loop test at a time. Save source and let the root
   dev-loop build.

## 9. Validation gate

Required before either side claims success:

- Client joins by room code over ICE.
- A second client joins.
- One game DLL swap while both are connected.
- Journals show hot ICE policy path, `network.hotswap.suspend/resume`, no
  reconnect, no server slot removal, and the first fresh snapshot after swap.
- Forced real drop shows attempts at the configured interval, attempt count
  within the configured max, and terminal failure only after grace or an
  explicit terminal rejection.
- Server survives NPC player death; no new log-bridge crash report.
- `devscripts/network_trace.py --run <run>` passes chain assertions and reports
  the first divergence with source file/line/function.
- Build, runtime, and human evidence are reported separately.

## 10. Immediate next actions

### Agent A

1. Write the hot/cold boundary section for ICE negotiation and ICE listener.
2. Audit `server-ice.cpp`, `server.cpp`, and `ice-agent.cpp` into policy vs
   primitive, with exact function names and line ranges.
3. Provide the ICE config key checklist.
4. Do not edit event semantics or reconnect health.

### Agent B

1. Write the event contract and exhaustive field table.
2. Write the config key contract and the hardcoded-value audit.
3. Propose the resident transport core ABI and maintenance/reattach flow.
4. Do not edit ICE policy files or the ICE boundary sections.

### Checkpoint after both drafts

Review together:
- exact event names and ownership;
- exact config keys;
- resident-core ABI fields;
- shared-file edit order;
- proof gate.

Only then begin code. The first code change is Agent A's ICE policy extraction;
Agent B's reconnect/config/event work follows without touching the same files.

### Copy-paste start message

```text
Work in C:\mimita-v9 on the current shared checkout. Read AGENTS.md,
docs/ROUTER.md, docs/specs/hotreload/hotreload.md,
docs/specs/networking/networking.md, and
docs/handoffs/networking-hot-cold-contract-20261117.md first.

This is the approved networking split: ICE policy hot with resident
libjuice/socket/agent; reconnect, health, events, config, trace, and client
transport continuity hot with a resident transport core.

Do not create branches, worktrees, lanes, or a second checkout. Do not run a
compiler or request a build; save source and let the root dev-loop build. Do not
edit a shared file while the other agent owns it.

Start with the checkpoint named in section 10: Agent A writes the ICE boundary
and audit; Agent B writes the event/config/resident-core contract. Stop for
review before code. Report changed files, exact event/config/API decisions, and
remaining conflicts.
```

## Related documents

- `docs/specs/networking/networking.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/hotreload/hotness-matrix.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/handoffs/hot-reload-agent-focus.md`
- `docs/operations/devscripts/cold-change-guard.md`
