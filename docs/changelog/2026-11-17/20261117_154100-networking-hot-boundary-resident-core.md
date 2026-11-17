# Networking hot boundary: resident core, opaque transport handles, hot policy decisions

- UTC timestamp: 2026-11-17T20:41:00Z
- EST timestamp: 2026-11-17 15:41:00 EST
- Scope: networking host ABI, resident core, server transport/ICE/session policy,
  source ownership, hotness matrix, handoff
- Build: EXE linked at 2026-11-17T20:38:12Z; game generation 328 published.
  Runtime acceptance not performed in this session.

## Summary

Implemented the first cut of the Cutover-1 networking boundary:

- transport capability v5 (opaque session open/attach/close, inbound/event
  polling, swap state, maintenance payload),
- ICE capability v2 (attach/detach without destroying a live resident agent),
- transport-session capability v2 (generation attach/poll/swap/maintenance),
- server ABI v2 with `ServerFrameV1.net_session`,
- resident transport/ICE/session registry in `src/host/net-core-primitive.*`,
- `ServerPlayer`/`PendingServerTransport` now carry opaque transport handles
  instead of game-owned `unique_ptr<IGameTransport>`,
- game-owned ICE policy decisions (`server-ice-policy.cpp`) and room/session
  policy decisions (`server-session-policy.cpp`) reached through
  `GameServerPolicyV1::ice_decide` / `session_decide`,
- non-owning game `IceAgent` views with explicit `destroyNow`,
- inline pure `net_common` helpers; deleted the duplicate cold/game translation
  units and updated the smoke-build tool,
- deletion-safe cold-change guard reporting, ownership overrides, and hotness
  matrix updates.

The ICE and server-session **state machines still run in resident
`server-ice.cpp` / `server.cpp`**. Only their bounded decisions are hot. The
remaining mechanical move, the removal of the cross-binary `ListenServerState*`
path, maintenance transmission during swap, and runtime proof are unfinished.

## Files

### Added

- `src/host/net-core-primitive.h`
- `src/host/net-core-primitive.cpp`
- `src/network/server-ice-policy.h`
- `src/network/server-ice-policy.cpp`
- `src/network/server-session-policy.h`
- `src/network/server-session-policy.cpp`
- `docs/handoffs/networking-hot-boundary-split-progress-20261117.md`

### Updated

- `src/host/abi/transport-capability.h`
- `src/host/abi/ice-capability.h`
- `src/host/abi/transport-session-capability.h`
- `src/host/abi/game-server-abi.h`
- `src/host/host-runtime.h`, `src/host/host-runtime.cpp`
- `src/host/host-server-transport-adapter.h/.cpp`
- `src/host/host-server-entity-adapter.cpp`
- `src/network/server.h`, `server.cpp`, `server-ice.cpp`,
  `server-packets.cpp`, `server-packet-chat.cpp`
- `src/network/ice/ice-agent.h/.cpp`, `src/network/ice-transport.h`
- `src/network/net_common.h`
- `src/game/ice-session-bridge-module.cpp`
- `src/game/transport-bridge-module.h/.cpp`
- `src/game/server-policy-module.cpp`
- `src/game/server-runtime-module.cpp`
- `config/source-owners.json`
- `devscripts/cold_change_guard.py`
- `docs/specs/hotreload/hotness-matrix.md`
- `docs/handoffs/networking-hot-boundary-split.md`

### Deleted

- `src/network/net_common.cpp`
- `src/game/net-common-module.cpp`

## Build evidence

- Game generation 328 published:
  `build/game/generation-328/game-g328.dll`.
- EXE link success recorded in `build/progress.json` (`result=success`) and
  `build/link-state-debug.json` (`status=linked`) at
  `2026-11-17T20:38:12Z`.
- Root `mimita.exe` written at 15:38:12 local, size `20,408,330`; linked SHA-256
  recorded as
  `27be990772c8aa175433512ff483c6eeb6155dde911c7cb78e75c0da39f9e5ef`.
- Dev-loop artifact publication and a new server/client launch are still
  pending.

## Runtime evidence

None for this change. The running server/client from the prior run are the old
ABI and were not used as proof.

## Remaining work

See `docs/handoffs/networking-hot-boundary-split-progress-20261117.md` section
5. Highest priority: publish and relaunch the new EXE + generation 328, prove
join/swap/live-edit/no-reconnect in the journals, then finish moving
`initServerIceListener` / `tickIceCoordinator` / `tickIcePeers` and the room
registration mechanics into game-owned policy behind resident primitives.

## Human review

Pending. No visual, multiplayer, or human acceptance is claimed.
