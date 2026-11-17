# Networking Agent A execution plan

Recorded: 2026-11-17  
Status: active execution handoff; confirmed by the user  
Owner: Agent A (ICE and server policy split) plus integration/verification  
Coordinator: Agent B keeps reconnect, health, events, config, trace, and client
transport continuity  
Authority: `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/hotreload/hotreload.md`,
and `docs/specs/networking/networking.md`

This is a coordination handoff, not a specification. If it disagrees with the
authoritative documents, those documents win. Do not use this file to claim
build, runtime, or human acceptance.

## Confirmed decisions

1. **Agent A is assigned to this agent.** I own the ICE hot/cold split, server
   session/pump policy split, network ABI co-ownership, source-owner cleanup,
   regression status, and final verification. Agent B keeps the reconnect/
   health/event/config/trace/client-core work already in progress.
2. **One combined cold boundary change is acceptable.** The ABI, resident
   network core, and server pump changes may be bundled into the smallest
   practical cold batch because they require an EXE build/restart. After that
   cold boundary, policy edits must return to the game-DLL path.
3. **`ice-server.cpp` and `udp-echo.cpp` are tools/diagnostics.** They are not
   production networking policy and are not Cutover-1 hotness requirements.
   This is now recorded in the networking specification and hotness matrix.
   They may remain cold/tool-owned. Do not use their coldness to justify
   resident production policy. If a tool path becomes normal gameplay, it needs
   a separate owner/spec and loses this exclusion.
4. **The `packet_registry_codegen.py` failure is cleared.** Verified with
   `python devscripts/packet_registry_codegen.py --check` and a generate pass:
   `[PACKET CODEGEN] ok packets=78`. The dev loop published generation 320.
   No generator change is currently needed.

## Current state at recording

- Agent B has uncommitted reconnect/health/event/config/trace work in hot files.
- Agent A has not changed `server-ice.cpp`, `ice-agent.cpp`, `server.cpp`, or
  `config/source-owners.json` yet.
- Cold guard still reports:
  - `src/network/server.cpp`
  - `src/network/server-ice.cpp`
  - `src/network/ice/ice-agent.cpp`
  - `src/network/ice/ice-server.cpp`
  - `src/network/net_common.cpp`
- Cutover-1 networking is therefore **not complete**.
- The dev loop passed packet codegen and published generation 320; no codegen
  fix is currently needed.

Regression record:
`docs/regressions/hotreload/cutover-claimed-complete-REG-2026-11-17.md`.

## Target boundary

### Resident cold mechanism

- libjuice agent, callbacks, callback thread.
- OS socket/WSA handles and syscalls.
- OS timers, server pump thread, sleep, bind/listen/send/receive.
- Raw receive/send queues, pending event queue, swap state.
- Opaque session/peer handles.
- Maintenance payload storage/transmission during a swap.
- Process/window/loader lifetime and transport ABI.

No resident join, reconnect, health, token, map, snapshot, room, peer-admission,
or player-slot decisions.

### Hot game DLL

- Packet IDs, schemas, encode/decode, handlers, validation, routing.
- HELLO/JOIN/WELCOME, map sync, spawn, session lifecycle.
- Reconnect, retry, grace, terminal failure, health, snapshot starvation.
- Server accept/reject, player slots, peer expiry, disconnect policy.
- ICE negotiation: STUN/TURN, SDP, candidates, gather/connect retry, timeouts,
  state interpretation.
- ICE listener: pending peers, admission, expiry, peer mapping, listener and
  room publication.
- Coordinator, server browser, room code, local-only status.
- Snapshot build/publish/apply policy, movement validation, lag compensation.
- All semantic `network.*` events and config-driven timings/rates.

### Swap invariant

1. Resident core keeps the socket/agent/queue alive.
2. Old generation unloads without destroying the agent.
3. New generation attaches by stable connection/session ID.
4. Queued events drain.
5. Health timers rebase; no reconnect is emitted.
6. The first fresh authoritative snapshot resumes normal play.

Required evidence: `network.hotswap.suspend`, `resume`, `health_rebased`,
`queue_drained`, generation activation in the same PID, and no swap-caused
`Reconnecting`.

## Coordination with Agent B

### File ownership

Agent A owns:

- `src/network/server-ice.cpp`
- `src/network/ice/ice-agent.cpp`
- `src/network/ice/ice-server.cpp` classification/tool handling
- ICE policy extraction from `src/network/server.cpp`
- ICE cleanup in `src/network/net_common.cpp`
- ICE/server rows in `config/source-owners.json`
- ICE boundary sections in `hotreload.md` / `hotness-matrix.md`
- server session/pump policy split

Agent B owns:

- `src/network/connection-health.*`
- reconnect/health functions in `src/network/multiplayer-packets.cpp`
- reconnect/token fields in `src/network/server-packets.cpp`
- `src/config/networking-config.*`
- `config/networkingconfig.json`
- `src/network/network-events.*`
- `devscripts/network_trace.py`
- `docs/specs/networking/network-event-contract.md`
- client transport continuity and resident-core client integration

Shared, sequential only:

- `src/network/server.cpp`
- `src/host/abi/transport-capability.h`
- `src/host/abi/transport-session-capability.h`
- `src/host/host-runtime.*`
- `config/source-owners.json`
- `docs/specs/networking/networking.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/hotreload/hotness-matrix.md`

### Rules

- One writer per file. Agent B's current files are not co-edited.
- Agent A lands the ICE boundary/ownership first.
- Agent B reviews and appends event lifecycle after A's boundary.
- Agent B owns `networkingconfig.json`; Agent A submits ICE key checklists.
- One co-authored ABI patch; no competing transport API.
- No agent runs a compiler, requests a build, or starts a second dev loop.
- One runtime test at a time.
- Preserve unrelated edits and do not claim another agent's evidence.

## Exact split work

### `server-ice.cpp`

Move hot:

- `initServerIceListener`
- `publishServerIceListener`
- `tickIceCoordinator`
- `tickIcePeers`
- pending-peer admission, expiry, player mapping
- TURN/ICE configuration policy
- listener/room publication
- `tickServerIceTransports` policy portions
- send-to-player and send-to-endpoint policy

Keep cold:

- raw socket send/receive
- libjuice agent tick/send/event queue
- connection-ID allocation mechanics
- pump timing and OS handling

### `ice-agent.cpp`

Move hot:

- remote SDP candidate filtering
- relay/direct selection policy
- ICE config/retry/timeout interpretation
- negotiation state policy

Keep cold:

- `juice_create`, gather, set-remote, send, destroy
- callbacks and callback thread
- bounded raw event queue
- selected-path logging mechanics

### `server.cpp`

Keep cold:

- socket setup
- outer pump
- listen-server thread
- sleep/timers/scheduler
- process lifetime

Move hot:

- session accept/reject
- player-slot and peer lifecycle policy
- join/reconnect/disconnect decisions
- listener publication calls
- send-routing policy
- outage/error policy

### `net_common.cpp`

Move pure parsing, formatting, map-id, and time helpers to a shared/hot owner.
Keep WSA startup and socket option syscalls in the resident host helper.

### `ice-server.cpp` and `udp-echo.cpp`

Record as tools/diagnostics per the specification. Do not use them as
production hotness evidence. They may remain cold.

## Initial function audit

Line numbers are a starting map, not stable API. Re-check before editing.

### `src/network/server-ice.cpp`

Hot policy candidates:

- `loadIceConfigForTurnCredentials` (62-74): TURN/ICE config policy.
- `CoordinatorPollWorker` (87-173): worker thread is a resident mechanism; the
  admission/publication decisions it feeds are hot.
- `waitForAgentState` (195-214): startup negotiation policy.
- `initServerIceListener` (226-330): listener setup/publication policy.
- `publishServerIceListener` (332-350): room/session publication policy.
- `tickIceCoordinator` (352-452): coordinator poll policy and peer admission.
- `tickIcePeers` (454-543): pending peer state, expiry, mapping, SDP exchange.
- `tickServerIceTransports` (545-765): packet dispatch and peer routing policy;
  split out the raw send/receive calls.
- `serverSendToPlayer` / `serverSendToEndpoint` (767-785): transport choice and
  send policy; raw `sendto` stays cold.

Resident primitive candidates:

- `residentAllocateIceConnectionId` (34-41) and
  `residentLegacyEndpoint` (43-60): raw handle/endpoint mechanics.
- libjuice tick/send/poll calls.
- raw socket send/receive.
- pending event queue and blocking primitives.

### `src/network/ice/ice-agent.cpp`

Hot policy candidates:

- `filterRelayCandidates` (373-386): relay/direct candidate policy.
- state/candidate interpretation in `handleStateChanged` (254-279) and
  `handleCandidate` (281-342), except raw event transport.
- config/retry/timeout interpretation in `initialize` (27-93) and
  `setRemoteDescription` (116-130).

Resident primitive candidates:

- construction/destruction (20-25, 185-201).
- `juice_create`, gather, set-remote, send, get-state, selected-path calls.
- `onStateChanged`, `onCandidate`, `onGatheringDone`, `onRecv` callbacks
  (230-252).
- bounded raw event queue (`pollEvents`, 220-228) and callback thread.

### `src/network/server.cpp`

Resident pump/mechanism:

- `runServer` (715-1767): socket setup and outer loop; several policy blocks are
  mixed in and need extraction.
- `startListenServer` (1775-2086): thread/socket creation plus room/session
  policy; split before moving.
- `simulateOneServerTick` (2143-2339): scheduler and containers; hot ABI calls
  remain, cold policy must leave.
- `listenServerThreadFunc` (2341-2423): pump thread.
- `tickListenServer` (2425-2447): tick entry.

Hot policy candidates mixed into those ranges:

- room registration/publication and local-only status;
- server host/player-slot assignment;
- ICE listener/peer orchestration;
- send-routing and outage/error policy;
- registration/room file policy.

## ABI checkpoint proposal

Decision: **extend the existing capability families; do not add a second
parallel “NetCore” API** unless review proves an existing family cannot express
the boundary.

### `TransportCapabilityV1` version 4 → 5

Keep the existing send/broadcast/state/peer/bind calls. Append:

- `open_session(role, connectionId, out SessionHandle)`
- `attach_session(connectionId, out SessionHandle)` — reattach after swap
- `close_session(SessionHandle)` — explicit shutdown only
- `poll_inbound(session, TransportMessageV1* out, capacity, out count)`
- `poll_events(session, TransportEventV1* out, capacity, out count)`
- `query_swap_state(TransportSwapStateV1* out)`
- `set_maintenance_payload(session, bytes, size, intervalMs)`
- `clear_maintenance_payload(session)`

Rules:

- Inbound payload pointers are valid only until the next poll call; hot code
  copies if it needs to retain them.
- Resident scratch buffers own all returned bytes.
- `attach_session` returns the existing resident session; it does not reset
  tokens, player slot, or transport.
- `close_session` is the only intentional destroy path. A DLL unload must not
  call it.

### `IceCapabilityV1` → V2

Keep create/initialize/gather/local-SDP/remote-SDP/send/tick/state/poll/log.

Append:

- `attach(connectionId, out handle)` — return an existing resident agent.
- `detach_view(handle)` — drop the game-side view without destroying the agent.
- Keep `destroy(handle)` as an explicit shutdown operation only.

The game `IceAgent` proxy becomes non-owning. Its destructor detaches its view;
it never calls `juice_destroy` merely because a generation is unloading.

### `TransportSessionCapabilityV1` → V2

Append or replace the raw `server_state` boundary with:

- `server_attach_generation(out session)`
- `server_detach_generation(session)`
- `server_poll_inbound(session, ...)`
- `server_poll_events(session, ...)`
- `server_query_swap_state(...)`
- `server_set_maintenance_payload(...)`

Do not pass a `ListenServerState*` across binaries. Return an opaque session
handle; hot policy keeps its own game-owned state.

### Server ABI

- Append `net_session` to `ServerFrameV1` and bump the server ABI/frame version
  (`kGameServerAbiVersion` / `ServerFrameV1::version` 1 → 2) in the same cold
  patch.
- The pump fills `net_session`; hot policy uses `TransportCapability` and
  `IceCapability` to poll/decide/send.
- Remove raw `SOCKET`, `sockaddr`, and game state pointers from hot policy
  signatures.
- Keep `GameServerPolicyV1` startup/configure/ID functions; add transport
  attach/poll only where needed.

### ABI invariants

- Opaque session/peer/agent handles only.
- Fixed-width session, peer, swap, message, and event records.
- No STL, vtable, `SOCKET`, `sockaddr`, game pointer, callback, or old-generation
  pointer retained in resident memory.
- Resident threads queue bytes/events only; they never call the game DLL.
- Maintenance payload is copied into resident memory and sent while no game
  generation is active.
- Server pump and client core use the same transport boundary.
- Remove the current cross-binary `ListenServerState` layout hazard; game and
  resident must not cast different layouts of the same object.

### ABI review checklist for Agent B

- Confirm client reconnect state can attach to an existing session without
  resetting the token or player slot.
- Confirm `poll_inbound` and `poll_events` can serve both client and server.
- Confirm maintenance payload is connection-level, not a second snapshot
  protocol.
- Confirm no new capability ID is needed; version-bump existing families.
- Confirm the client core uses the same attach/detach semantics as the server.
- Confirm one cold ABI patch updates every version check and bridge together.

## Phases

### Phase 0 — Stabilize

- Freeze shared-file writes with Agent B.
- Record baseline source manifest, cold guard, dependency graph.
- Produce the function/line matrix for `server-ice`, `server`, and `ice-agent`.
- Confirm packet codegen stays green; do not edit the generator without a new
  failure.

### Phase 1 — Resident core and cold ABI

- Add opaque handles, attach/detach, event queue, maintenance payload, swap
  state, agent/socket retention.
- Keep current behavior working.
- Bundle cold ABI/server-pump changes into one boundary where practical.
- Gate: old join/server behavior unchanged.

### Phase 2 — ICE policy hot

- Extract negotiation/listener policy into game-owned code.
- Make the game ICE proxy non-owning; close only intentionally.
- Add reattach by stable connection ID.
- Classify `ice-server`/`udp-echo` as tools.
- Gate: ICE policy resolves `HOT`; room join works; no ICE policy cold.

### Phase 3 — Server session policy hot

- Move peer/player/session/send/outage policy out of the pump.
- Pump becomes socket/thread/timer/agent only.
- Remove raw `SOCKET`/`sockaddr` and STL from hot policy interfaces.
- Gate: server/room/session edits activate in the same PID.

### Phase 4 — Cleanup

- Remove resident duplicate policy and fallbacks.
- Share pure helpers.
- Update source owners, specs, matrix, regression status.
- Verify no old-generation callback/job/pointer survives unload.

### Phase 5 — Proof

- Client joins by room code over ICE.
- Second client joins.
- Repeated DLL swaps while both stay connected; no swap-only reconnect.
- Live edit ICE timeout/retry/candidate/listener policy.
- Live edit server session/room policy plus rollback.
- Forced real drop uses configured interval/attempts/grace.
- `network_trace.py --run <run>` passes chain assertions.
- Cold guard and dependency graph show no cold policy files.
- Human review confirms the session stayed usable.
- Build, runtime, and human evidence reported separately.

## Risks

- Current `ListenServerState` layout mismatch is the highest-risk defect.
- Moving code without retaining the resident agent will drop connections.
- Shared-file conflicts with Agent B are likely if ownership is not enforced.
- Packet codegen is green now; runtime proof remains blocked on the ABI/split.
- A cold boundary build is required at least once for the ABI/pump split.
- `ice-server`/`udp-echo` scope creep must be rejected.

## Next immediate action

The function matrix and ABI checkpoint proposal are recorded above. Agent A's
next step is the ABI review with Agent B:

- Agent B confirms the client attach/detach and maintenance behavior.
- Agent B confirms `poll_inbound`/`poll_events` cover client and server.
- Both agree on one cold ABI patch and its version bumps.
- Then implement the smallest cold boundary change.

After ABI review, the sequence is:

1. One cold patch: capability versions, opaque session handles, attach/detach,
   maintenance payload, swap state, `ServerFrameV1.net_session`, resident
   session registry, and removal of the cross-binary `ListenServerState` cast.
2. Build/restart once, preserve current join behavior, and prove no policy moved
   only for the ABI.
3. Move ICE negotiation/listener policy hot and retain the resident agent.
4. Move server session/peer/send policy hot; leave only the pump.
5. Clean up `net_common`, classify tools, update source owners and matrix.
6. Run the swap/reconnect/live-edit proof gate.

No source edits until shared-file ownership and the ABI review are confirmed.
The codegen check is currently green; runtime proof remains blocked on the ABI/split landing.
