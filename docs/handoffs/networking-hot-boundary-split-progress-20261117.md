# Networking hot-boundary split — progress handoff

Recorded: 2026-11-17 15:40 local (20:40 UTC)
Status: implementation landed through the cold ABI/resident-core batch and hot
policy decision split. Build linked. Runtime proof and the remaining mechanical
move are unfinished.
Authority: `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/networking/networking.md`,
`docs/specs/hotreload/hotreload.md`, `docs/specs/hotreload/hotness-matrix.md`,
and `docs/handoffs/networking-hot-boundary-split.md` (the original plan).

This document is a coordination handoff, not a specification. If it disagrees
with an authoritative document, the authoritative document wins. Do not use it
to claim build, runtime, or human acceptance.

---

## 1. What this session did

Implemented the first real cut of the Cutover-1 networking boundary:

1. Added the resident transport/ICE/session core and opaque handles.
2. Removed the game-owned `std::unique_ptr<IGameTransport>` from the server
   player/session path; game policy now uses opaque transport handles.
3. Made the server frame carry an opaque `net_session`.
4. Added a hot ICE decision owner and a hot room/session decision owner.
5. Made `IceAgent` non-owning across a generation unload.
6. Made `net_common` pure helpers inline and deleted the duplicate cold/game
   translation units.
7. Updated ownership and hotness documentation.

The ICE/listener and server-session **state machines still live in resident
`server-ice.cpp` / `server.cpp`**. Their bounded decisions (timeouts, admission,
expiry, relay, local-only, publication, room-file) are now game-owned and live
through `GameServerPolicyV1`. That is the main remaining gap versus the original
handoff.

---

## 2. Current build and runtime state (as recorded)

Source/build facts:

- Game generation **328** was published:
  `build/game/generation-328/game-g328.dll`
  (game build `current.json`: `status=ok`, `source_count=642`).
- The EXE linked successfully at **2026-11-17T20:38:12Z**:
  - `build/progress.json`: `"result": "success"`
  - `build/link-state-debug.json`: `"status": "linked"`, SHA-256
    `27be990772c8aa175433512ff483c6eeb6155dde911c7cb78e75c0da39f9e5ef`
  - `mimita.exe` was written at 15:38:12 local, size `20,408,330`.
- The dev-loop may still show `lifecycle=building` / `build=building` while it
  stages/publishes the linked EXE. Check for a new `latest_artifact_id` later
  than `2026-11-17T17:02:29.218920Z` before launching.
- The running server (PID 12272) and client (PID 5152) are still the old ABI-1
  process from the pre-change run. No runtime proof for this change exists yet.

Important: `.dev/build-output.log` and `.dev/link-output.log` contained stale
failures from an earlier build. The authoritative current result is
`build/progress.json` + `build/link-state-debug.json` above.

Do not report this as build acceptance until the dev-loop publishes the new
artifact and a fresh server/client run is observed.

---

## 3. Files changed by this session

### Host ABI

- `src/host/abi/transport-capability.h`
  - `kTransportCapabilityVersion = 5`.
  - Added `TransportSwapStateV1`.
  - Added opaque `send_to_session`, `close_session`, `open_session`,
    `attach_session`, `poll_inbound`, `poll_events`, `query_swap_state`,
    `set_maintenance_payload`, `clear_maintenance_payload`.
- `src/host/abi/ice-capability.h`
  - `kIceCapabilityVersion = 2`.
  - Added `attach(stableKey)` and `detach_view(handle)`. `destroy` remains the
    explicit shutdown operation.
- `src/host/abi/transport-session-capability.h`
  - `kTransportSessionCapabilityVersion = 2`.
  - Added `server_attach_generation`, `server_detach_generation`,
    `server_poll_inbound`, `server_poll_events`, `server_query_swap_state`,
    `server_set_maintenance_payload`.
  - `server_state` remains as a deprecated compatibility path; **it still
    crosses a `ListenServerState*` and must be removed**.
- `src/host/abi/game-server-abi.h`
  - `kGameServerAbiVersion = 2`, `ServerFrameV1::version = 2`.
  - Added `ServerFrameV1::net_session`.
  - `kGameServerPolicyAbiVersion = 2`.
  - Added `ServerIcePolicyV1`, `ServerSessionPolicyV1`, and policy callbacks
    `ice_decide` and `session_decide`.

### Resident core

- `src/host/net-core-primitive.h` (new)
- `src/host/net-core-primitive.cpp` (new)
  - Transport registry: `TransportHandle -> unique_ptr<IGameTransport>`.
  - Session registry: role, connection key, maintenance bytes.
  - Swap bookkeeping: generation attached/suspended, suspend/resume counters.
  - Resident ICE agent registry: stable key -> resident handle.
  - `closeAllTransports()` for server shutdown.
- `src/host/host-runtime.h/.cpp`
  - Implemented all new capability thunks.
  - Added `serverPolicyIceDecide` and `serverPolicySessionDecide`.
  - Hooks `setGenerationAttached`, `noteSwapSuspend`, `noteSwapResume` on
    generation load/swap/refusal.
- `src/host/host-server-transport-adapter.h/.cpp`
  - Claimed transport is now a `ServerTransportHandle*`, not a game
    `unique_ptr<IGameTransport>*`.
- `src/host/host-server-entity-adapter.cpp`
  - Player removal closes the resident transport handle.

### Server policy / hot owners

- `src/network/server-ice-policy.h/.cpp` (new, game-owned)
  - Live-fixable constants/decisions: listener gather timeout, peer gather and
    connect timeouts, coordinator poll interval, pending-peer admission bound,
    max packets per ICE poll, relay-only decision.
- `src/network/server-session-policy.h/.cpp` (new, game-owned)
  - Live-fixable decisions: local-only, coordinator registration, publication,
    room-file writing, duplicate-room admission.
- `src/game/server-policy-module.cpp`
  - Wires `ice_decide` and `session_decide` into `GameServerPolicyV1`.
- `src/network/server-ice.cpp`
  - Calls `ice_decide` at listener config, coordinator poll, peer gather,
    peer connect, and transport-drain stages.
  - Registers the listener agent by `iceSessionId` for reattach.
  - Uses resident transport handles for pending/connected ICE transports.
  - Closes handles on deferred removal; unregisters listener agent on stop.
- `src/network/server.cpp`
  - Uses resident transport handles in probe/adapter sends.
  - Opens/closes the resident server session and fills `net_session`.
  - Calls `session_decide` for local-only/registration/publication/room-file
    policy.
  - Removed a resident read of game-owned `NetworkingConfig` from the metrics
    log (bounded resident default). This was required to link the strict cold
    EXE.
- `src/network/server.h`
  - `ServerTransportHandle`, `kNoServerTransport`.
  - `ServerPlayer::transportHandle` and `PendingServerTransport::transport`
    replace game-owned transport pointers.
  - `ListenServerState::netSession`.
  - Claimed-transport signatures now take `ServerTransportHandle*`.
- `src/network/server-packets.cpp`
  - Join/reconnect/send/source routing uses opaque transport handles.
  - Disconnect closes handles through `mimita::game::closeSession`.
- `src/network/server-packet-chat.cpp`
  - Timeout/disconnect path uses `transportHandle` and
    `mimita::game::closeSession` (concurrent edit; verify it matches the
    boundary).
- `src/game/server-runtime-module.cpp`
  - Casts `frame.claimedTransport` to `MimitaNet::ServerTransportHandle*`.
- `src/game/transport-bridge-module.h/.cpp`
  - Added `sendToSession` and `closeSession`.
  - `serverSendToPlayer` prefers the player's opaque handle.

### ICE agent lifetime

- `src/network/ice/ice-agent.h/.cpp`
  - Added `AttachedTag` construction, `attachView(stableKey)`, `destroyNow()`.
  - Resident `destroyNow()` is a no-op (owner destruction is the owner's job).
- `src/game/ice-session-bridge-module.cpp`
  - Destructor now calls `detach_view`; it does not destroy a live agent.
  - `destroyNow()` performs explicit destroy.
  - `attachView()` reattaches to an existing resident agent.
- `src/network/ice-transport.h`
  - `close()` calls `destroyNow()` after `shutdown()` for intentional close.

### Pure helpers / ownership

- `src/network/net_common.h`
  - Pure helpers are inline (`parseAddress`, `addressToString`, `nowMs`,
    `normalizeMapId`, `mapIdsReferToSameMap`).
- `src/network/net_common.cpp` — deleted.
- `src/game/net-common-module.cpp` — deleted.
- `tools/network_smoke_build.py` — removed the deleted source from its input
  list.
- `config/source-owners.json`
  - Removed `src/network/net_common.cpp` from resident inputs.
  - Added explicit overrides for `server-ice-policy.cpp` (game),
    `server-session-policy.cpp` (game), and `net-core-primitive.cpp` (resident).
- `devscripts/cold_change_guard.py`
  - Deleted files are no longer classified as `UNKNOWN` (deletions cannot
    introduce cold behavior; the build/link gates still prove the removal).
- `docs/specs/hotreload/hotness-matrix.md`
  - ICE and room rows now name the hot policy decision owners and the resident
    primitive exceptions.
- `docs/handoffs/networking-hot-boundary-split.md`
  - Appended an implementation-progress section.

### Concurrent edits to preserve

Another session/agent was active in this checkout during the work. These files
were modified outside this session’s boundary work and must be preserved:

- `build.py`, `devscripts/dev-loop.py`, `devscripts/network_trace.py`,
  `devscripts/scenarios/melee_start_v1.json`, `config/analytics.json`
- `src/config/networking-config.*`
- `src/engine/engine-tick-net.cpp`, `engine-tick-setup.cpp`,
  `engine-tick-state.cpp`, `whole-game-entry.cpp`
- `src/network/multiplayer-packets.cpp`, `multiplayer-tick.cpp`,
  `network-events.h`, `server-npcs.cpp`
- `src/npc/npc.cpp`, `npc.h`, `npc-navigator.cpp`
- `src/physics/physics-mini.cpp`, `physics-mini.h`
- `src/host/host-server-ice-fallback.cpp`
- `docs/changelog/...`, `docs/features/...`, `docs/specs/open-gameplay-issues.md`

Do not revert those edits. In particular, `networking-config.*` is another
owner’s in-progress work.

---

## 4. Build issues already diagnosed and fixed

The first EXE link attempts failed for three separate reasons:

1. **Stale manifest/object link.** The EXE build started before the source
   manifest was regenerated, so it linked game-owned objects
   (`network_coordinator-client.o`, `debug_debug-log.o`) against the resident
   weak fallbacks (`host-coordinator-fallback.o`, `host-debug-facade.o`) and
   reported multiple definitions.
   - Fixed by regenerating `build/source-manifest.json` (`total=808`,
     `game=643`, `resident=42`) and letting the next build use it.
   - `build/link.rsp` now excludes those game objects.
2. **Stale `host_host-runtime.o`.** It predated a concurrent
   `inputMouseButtonCallback` definition and produced an undefined reference.
   Recompiling `host-runtime.cpp` resolves it.
3. **Resident `server.cpp` read game-owned `NetworkingConfig`.** Lines around
   the metrics log referenced `NetworkingConfig::instance()`, which is not in
   the strict cold EXE. Replaced with a bounded resident default and
   `config_revision=0`; a future hot policy can supply the real value.

The final link at `20:38:12Z` succeeded. If a later build regresses, check the
current `build/source-manifest.json` owners first, then stale objects, then new
resident references to game-owned symbols.

---

## 5. What is NOT done

These are required to finish the original handoff:

1. **Runtime proof.** No fresh server/client run has exercised the new EXE +
   generation 328.
2. **Full ICE state-machine move.** `initServerIceListener`,
   `tickIceCoordinator`, `tickIcePeers`, and the transport-drain loop still run
   in resident `server-ice.cpp`. Only their bounded decisions are hot.
3. **Full server session move.** Room registration thread, listener
   publication mechanics, and the dedicated/listen pump still run in resident
   `server.cpp`. Only the local-only/registration/publication/room-file decision
   is hot.
4. **Remove the `ListenServerState*` cross-binary path.**
   `TransportSessionCapabilityV1::server_state`,
   `src/game/transport-session-bridge-module.cpp`, and
   `src/gui/gui-main.cpp` still use it. Version 2 adds opaque attach/poll, but
   the old path is still live.
5. **Maintenance transmission during a swap.** The resident session stores the
   payload and interval, but no resident worker sends it while no generation is
   attached.
6. **Async coordinator transport.** `CoordinatorPollWorker` still lives in
   resident `server-ice.cpp`. A hot state-machine move needs a resident async
   HTTP/text job or an equivalent bounded coordinator transport.
7. **Peer reattach after swap.** The listener agent is registered by
   `iceSessionId`, and `IceCapabilityV2::attachView` exists, but no hot policy
   currently reattaches and resumes. Pending peers are discarded on swap.
8. **Documents/regression/changelog.** Hotness proof column, networking
   implementation note, regression entry (if applicable), and the final
   changelog for this session (see note at the end).
9. **Human acceptance.** None yet.

---

## 6. Next steps (recommended order)

### Step 1 — Confirm publication and restart

1. `python devscripts/dev-loop.py --status`
   - Wait for a `latest_artifact_id` / `latest_build_id` later than
     `2026-11-17T17:02:29.218920Z`.
   - The linked EXE hash from the successful link is
     `27be990772c8aa175433512ff483c6eeb6155dde911c7cb78e75c0da39f9e5ef`.
2. Stop the old server/client (`--request stop-server`, `--request stop-client`
   or the dashboard) only after the new artifact is published.
3. Launch mode 57 or a networking mode:
   `python devscripts/dev-loop.py --request launch --launch-mode 57`.
4. Verify the new process journal shows `game.generation.activated` for
   generation 328 and a server ABI-2 start.

### Step 2 — Runtime acceptance

In the new server and client `events.jsonl`:

- Server starts and room registers (`network.room.registered`).
- Client joins by room code over ICE; `network.connect.stage` reaches
  `Connected`; `network.connection-state.changed` ends at `Connected`.
- Second client joins.
- Swap the game DLL while both are connected (save a hot game file), then check:
  - `network.hotswap.suspend`, `network.hotswap.resume`,
  - `game.generation.activated` in the same PID,
  - no `Reconnecting` caused only by the swap,
  - both clients keep playing.
- Live-edit an ICE policy value in `src/network/server-ice-policy.cpp` (for
  example `kIcePeerConnectTimeoutMs`) and verify the new value is used after a
  generation swap.
- Live-edit `src/network/server-session-policy.cpp` local-only/publication and
  verify the room behavior changes in the same PID.
- Force a real drop and verify the configured interval / attempts / grace.
- Run `python devscripts/network_trace.py --run <run>` and record chain
  assertions.

Record build evidence, runtime evidence, and human acceptance separately. A
generation activation alone is not behavior proof.

### Step 3 — Finish the mechanical move (Phases 2/3 of the original handoff)

To satisfy the original handoff literally:

1. Add resident primitive entry points for:
   - create/start listener agent,
   - coordinator poll request/response (async text/bytes, no DLL call from the
     resident worker),
   - create peer agent, apply remote SDP, promote to resident transport,
   - peer expiry/close.
2. Move `initServerIceListener`, `publishServerIceListener`,
   `tickIceCoordinator`, and `tickIcePeers` into a game-owned
   `server-ice-policy.cpp` (or equivalent) that calls those primitives by opaque
   handles. Keep only libjuice/socket/thread mechanics resident.
3. Move room registration/publication/local-only orchestration out of
   `server.cpp` into `server-session-policy.cpp`.
4. Delete `TransportSessionCapabilityV1::server_state`; replace GUI/bridge reads
   with `server_attach_generation` + a fixed `ServerSessionViewV1`, and replace
   `ListenServerState*` passing with the opaque `net_session`.
5. Implement maintenance send during swap and peer reattach by stable key.
6. Re-run cold guard: `src/network/server-ice-policy.cpp` and
   `src/network/server-session-policy.cpp` must resolve `game`; no production
   policy may remain in a cold file. Review hot-to-cold edges.

### Step 4 — Documentation and completion

- Update `hotness-matrix.md` “Current proof” for ICE and room rows.
- Update `networking.md` only if behavior/spec alignment changed; do not weaken
  the hot/cold target to match the implementation.
- Add a regression entry only for a confirmed regression, not for this work
  history.
- Regenerate `build/source-manifest.json` after any owner changes.
- Run the cold guard on the exact hot policy files and the dependency graph.
- Create one final changelog for the session.

---

## 7. Verification commands (do not run builds directly)

Allowed observation commands:

```text
python devscripts/dev-loop.py --status
python devscripts/dev-loop.py --list-instances
python devscripts/dev-loop.py --inspect
python devscripts/dev-loop.py --ensure-running
python devscripts/source_manifest.py --check
python devscripts/cold_change_guard.py --report-only
python devscripts/packet_registry_codegen.py --check
```

Do not run `build.py`, `build_agent.py`, `buildv2.py`,
`module-build-worker.py`, a compiler, `ccache`, `dev-loop.py --request build`,
`--request game-dll`, or `--request cold-link-trial`. Saving a compiler input is
the only normal build trigger.

One runtime test at a time. The old server/client must not be used as proof for
the new ABI.

---

## 8. Risks

- Concurrent edits by another session can invalidate builds and overwrite
  shared files. Re-read a file immediately before editing it.
- `ServerFrameV1` still carries game-owned containers (`players`, `npcs`,
  `world`, and similar). This session fixed only the transport/session boundary;
  it did not remove the container ABI.
- `hostedRoomSession()` is still an inline per-binary static. The resident
  server writes its EXE copy; game/terminal code may write a DLL copy. This is
  pre-existing and must be moved to resident session identity before claiming
  room-state parity across binaries.
- `server-ice.cpp` still owns the coordinator poll worker thread. A DLL swap
  while that worker is alive is only safe because the worker remains resident.
- The policy decision structs are ABI v2. Any mismatch between the EXE and the
  game DLL will reject the generation; always restart both together.
- Do not use `ice-server.cpp` or `udp-echo.cpp` as production hotness evidence;
  they remain diagnostic tools.

---

## 9. Session changelog note

This session’s changelog must be created as one file under
`docs/changelog/2026-11-17/` (the session was interrupted before the final
changelog was written). It must list:

- source changes above;
- build evidence: generation 328 published, EXE link success at
  `2026-11-17T20:38:12Z`, hash/size;
- runtime evidence: none yet;
- human review: pending;
- the remaining work in section 5.
