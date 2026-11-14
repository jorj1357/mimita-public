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
  `src/network/schema/packet-common.h`. Input and chat structs moved to
  `src/network/schema/packet-input.h` and `packet-chat.h`. Packet IDs and the
  remaining structs stay in the umbrella until the generated ID migration lands.
- `src/network/server.cpp`: removed the unused resident `isKnownPacketType`
  duplicate and the resident packet-handler fallback blocks. The hot
  `server-packets.cpp` check remains.
- `src/network/server-ice.cpp`: removed the resident `processServerPacket`
  fallback; ICE packets use the DLL handler only.
- `docs/specs/networking/networking.md`: appended the normative single-authority
  and packet-split section. It records that all MiMITA networking behavior is
  hot; only external libraries/OS mechanisms are cold.

## Evidence

- Packet/state codegen dry-run passes: 78 packet IDs, 8 state domains.
- Dev-loop artifact source 22 published successfully after the single-authority
  and schema-header changes.
- Runtime probe on generation 160: `server_hotness_probe.py --client --chat
  packet-split-smoke` returned `ok`; `players=1`, chat `path=dll`,
  `server_failures=0`.
- A later full rebuild was in progress when this entry was written.

## Follow-up

- Move `PacketType` to generated `packet-ids.h` once Agent A updates the packet
  generator to read the schema directory.
- Split packet structs into domain headers.
- Remove the remaining unused compatibility handler functions.
- Remove resident fallback duplicates for server packet/simulation/publish
  paths after the hot-only server is accepted.
'