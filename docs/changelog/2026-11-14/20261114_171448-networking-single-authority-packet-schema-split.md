# Networking single authority and packet schema split

Date: 2026-11-14

## Before

- Server inbound had two hot paths: `dispatchInboundMessage` handled 12 packet
  types through capability bridges, while `processServerPacket` handled the
  normal server set.
- The generated packet ledger reported `client_dll = 12` from those 12
  compatibility overrides. That number was not normal client coverage.
- `src/network/packets.h` contained protocol constants, packet IDs, shared
  enums, the wire header, and every packet struct in one file.
- A second `isKnownPacketType` range check lived in resident `server.cpp`.

## After

- `src/game/server-command-dispatch-module.cpp`: `dispatchInboundMessage` no
  longer switches on packet types and always returns false. Server inbound has
  one authority: `MimitaNet::processServerPacket`. The 722
  `dispatchServerCommand` surface remains separate.
- `src/network/packets.h`: wire header and common limits/enums moved to
  `src/network/schema/packet-header.h` and
  `src/network/schema/packet-common.h`. Input, chat, avatar, session, snapshot,
  combat, player, gamemode, physical, and vehicle structs moved to their domain
  headers under `src/network/schema/`.
- `src/network/server.cpp`: removed the unused resident `isKnownPacketType`
  duplicate and the resident packet-handler fallback blocks. The hot
  `server-packets.cpp` check remains. `runServer` refuses to start without the
  hot server API.
- `src/network/server-ice.cpp`: removed the resident `processServerPacket`
  fallback; ICE packets use the DLL handler only.
- `src/network/net_common.cpp`: now contains only pure endpoint/time/map-id
  helpers. WinSock startup and socket-option ownership moved to resident
  `src/host/net-common-host.cpp`. Agent A must remove `src/network/net_common.cpp`
  from the resident owner list so it is compiled into the generation.
- `docs/specs/networking/networking.md`: appended the normative single-authority
  and packet-split section. It records that all MiMITA networking behavior is
  hot; only external libraries/OS mechanisms are cold.

## Evidence

- Packet/state codegen dry-run passes: 78 packet IDs, 8 state domains.
- Dev-loop artifact source 22 published successfully after the single-authority
  and schema-header changes; source 25 published after the avatar split.
- Runtime probe on generation 160: `server_hotness_probe.py --client --chat
  packet-split-smoke` returned `ok`; `players=1`, chat `path=dll`,
  `server_failures=0`.
- Repeat probes after the input/chat/avatar splits returned `ok` with
  `players=1`, chat `path=dll`, and zero server failures.
- `runServer` now refuses to start without the hot server API; the resident
  server fallback path is no longer a supported startup mode.

## Follow-up

- Move `PacketType` to generated `packet-ids.h` once Agent A updates the packet
  generator to read the schema directory.
- Split packet structs into domain headers.
- Remove the remaining unused compatibility handler functions.
- Remove resident fallback duplicates for server packet/simulation/publish
  paths after the hot-only server is accepted.
'
## Follow-up in the same session

- Split the remaining requested packet domains: session, snapshot, combat,
  player lifecycle, gamemode, physical, and vehicle.
- Added `src/game/server-authored-rebuild-module.cpp`: the DLL generation uses
  CAP_JOBS instead of a resident `std::thread`; the EXE keeps the resident
  thread implementation.
- Added `EntityDomainCapabilityV1` with generated `player_core` and
  `player_session` read/write, host adapter methods, host thunk, and a
  generation bridge module. No handler consumes it yet.
- Moved WinSock startup/socket-option ownership from `net_common.cpp` into
  resident `src/host/net-common-host.cpp`; pure helpers remain hot.
- EXE artifact `2026-11-14T23:13:13Z` published successfully.

## Blocked-item follow-up

- Retired the standalone `--client` path. `main.cpp` now reports it as retired,
  `runClient` was removed from `net_mode.h`, and `src/network/client.cpp` was
  deleted. The full client remains the supported client surface.
- `src/game/net-common-module.cpp` is the DLL-only copy of the pure endpoint
  helpers, so the generation does not depend on resident `net_common.cpp`.
- `src/game/server-runtime-module.cpp` now performs a periodic
  `player_core`/`player_session` parity read through the new domain bridge and
  emits `game.state.domain-parity`.
- The first authored-rebuild hot attempt was removed after discovering the
  existing `authored-rebuild-bridge-module.cpp` already delegates to the host
  worker; no duplicate worker remains.
- `server.cpp`, `server-ice.cpp`, and the ICE wrappers still contain the raw
  socket/thread mechanisms and were not moved.

- Game generation 163 published successfully after the packet-domain split,
  Option A server-shell cleanup, client retirement, domain parity consumer, and
  hot net-common copy.
