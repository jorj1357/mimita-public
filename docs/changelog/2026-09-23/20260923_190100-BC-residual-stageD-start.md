# Stage B/C residual finish + Stage D start: ground clamp, broadcast interp, join policy, packet registry

Date (UTC): 2026-09-23T19:01:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Finish the remaining small policy residuals of Stage B/C, then start Stage D
(server packets) with the packet-type registry and join-acceptance policy.

## Changes

### B/C residual
- **`net.npc-ground-clamp`** (new `hot-npc-ground-clamp.h` +
  `modules/npc-ground-clamp-policy.cpp`): the NPC floor-pin rest height and clamp
  decision are hot; the cold path still supplies the floor query and body state.
- **`net.broadcast-interp`** (new `hot-broadcast-interp.h` +
  `modules/broadcast-interp-policy.cpp`): the server broadcast smoothing-enabled
  decision and per-tick movement cap are hot; the sample buffer and lerp
  mechanism stay cold.

### Stage D start
- **Explicit packet-type registry** (`server-packets.cpp` `isKnownPacketType`):
  replaced the numeric-range TODO with an explicit `switch` listing every
  `PACKET_*` id (1-90). A future packet id must be added here, or arrive via
  `PACKET_HOT_CODEC` (whose schema is validated by the hot codec).
- **`net.join-policy`** (new `hot-join-policy.h` + `modules/join-policy.cpp`):
  join acceptance (full / token / local-vs-coordinator / password) is hot. The
  cold path gathers the facts (token presence, `coordinatorIceValidateJoin`
  result, password comparison) and emits the reject packet.

### Manifest + selftests
- Added `hot-npc-ground-clamp.h`, `hot-broadcast-interp.h`, `hot-join-policy.h`
  to `headers`; added checks for each in `live-code-selftest.cpp`.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T185934.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED]` for all new providers and their checks;
  `--packet-codec-selftest` -> PASS; `--udp-echo` boots and binds normally.
- Runtime / human acceptance: pending.

## Stage D remaining

- `server-packets.cpp`: map-ready handshake, reconnect-token rotation, reload
  cache, spawn-generation/ack gates; move remaining decode/validation policy to
  hot codecs/handlers.
- `packets.h`: become a schema declaration layer with per-schema hot codecs
  registered through `net.packet-codecs`; keep old + new versions accepted
  during a transition.
