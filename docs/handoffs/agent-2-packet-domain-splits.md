# Agent 2 handoff — packet and wire-domain splits

Snapshot: 2026-11-11
Repository: `C:\mimita-v9`
Role: Agent 2 owns packet/wire-domain splits.
Agent 1 owns server timing/performance and combat authority; see
`agent-1-server-perf-authority.md`.

## Goal

Split the remaining packet domains out of `src/network/packets.h` into
domain-owned headers under `src/network/wire/` without changing a single byte
of the wire layout, protocol version, reliable-event field order, or enum
values. Keep `packets.h` as the umbrella include so no consumer file changes.

## Boundary

Owns only:

- `src/network/packets.h`
- new headers under `src/network/wire/`
- `src/network/packets.cpp` only if the include path needs no layout change

Must not edit:

- `src/network/wire/protocol.h`, `src/network/wire/join.h`
- `src/network/*.cpp` and other `src/network/*.h`
- `src/modules/api/**`, `config/modules/modules.json`, `src/hot-reload/**`
- any module source
- server tick/authority, NPC, physics, client performance files
- Agent 1's server/authority files

Packet structs are host-only. No module source or module API header includes
`packets.h`, so this split does not affect module code hashes, shared-module
agreement, or module generation.

## Current wire risks

- `packets.h` has a packed region from about line 297 to 1544. A nested
  `pack(1)` push/pop at about 518/573 returns to pack(1), not default. Preserve
  the effective packing of every struct, not the exact pragma sequence.
- Structs after that region are naturally aligned. Accidentally packing them
  changes the wire layout. `ClaimedHitPacket`, `AttackRequestPacket`,
  `AttackResultPacket`, `ReloadRequestPacket`, `ReloadResultPacket`, respawn/
  spawn structs, bomb-tag/disaster, and join structs are natural-aligned.
- Many existing asserts are loose `<=` caps. Packed and natural layouts can both
  pass. Add exact `sizeof` asserts before moving a domain.
- `DuelStatePacket` has no exact assert and is already larger than the 1200-byte
  safe datagram budget; it is sent as a whole reliable event. Do not add fields,
  reorder, or change its packing.
- Reliable gameplay events require `eventId`/`eventSessionId` to remain the
  first two `uint32` values after `PacketHeader` for shot, chat, pellet, kill,
  and NPC damage events.
- `offsetof` dependencies exist for `SnapshotChunkPacket::entities` and
  `MapConfigSnapshotPacket::data`. Preserve field order.
- `MAX_PLAYERS=32`, `MAX_SNAPSHOT_ENTITIES=90`, and `PROTOCOL_VERSION=52` must
  not change unless a deliberate wire change is approved separately.
- `MimitaNet::EntityType` must remain namespaced; a global `EntityType` exists
  elsewhere.

## Domain order

One domain per build. Wait for the lane build and runtime validation before
starting the next domain.

1. `wire/packet-enums.h` — enums/constants above the structs
2. `wire/attack.h` — claimed hit, attack request/result, reload request/result
3. `wire/avatar.h`
4. `wire/input.h`
5. `wire/snapshot.h`
6. `wire/shot.h` — shot/projectile/fire-result structs
7. `wire/combat.h` — reliable ack, damage confirmed, melee hit
8. `wire/pellet.h`
9. `wire/spyknife.h`, `wire/godball.h`
10. `wire/ragdoll.h`
11. `wire/physics.h`
12. `wire/vehicle.h`
13. `wire/chat.h`
14. `wire/lifecycle.h` — mixed packed and natural sections
15. `wire/duel.h` — last, largest and most coupled

`packets.h` becomes an umbrella including protocol, join, packet-enums, and
every domain header. It must have no file-level `#pragma pack`. Each domain
header includes only `<cstdint>`, `protocol.h`, `packet-enums.h`, and any
intra-domain headers.

## Exact guards to add before moving

At minimum:

- `ClaimedHitPacket == 28`
- `AttackRequestPacket == 336`
- `AttackResultPacket == 72`
- `ReloadRequestPacket == 40`
- `ReloadResultPacket == 64`
- avatar manifest/request/chunk `== 1088/60/1196`
- `InputPacket == 244`, redundancy slot `== 50`
- `SnapshotEntity == 194`, `SnapshotPacket == 17492`
- `CompactEntityData == 179`, `SnapshotChunkPacket == 1106`
- `DamageConfirmedEventPacket == 113`, `MeleeHitEventPacket == 87`
- `RespawnRequestPacket == 28`, `PlayerRespawnedPacket == 584`
- `SpawnAckPacket == 28`, `SpawnActivatedPacket == 32`
- `DuelStatePacket ==` the pre-move value confirmed in the lane build

If a size differs from the pre-move build, stop. Do not adjust the assert to
match the new layout.

## Validation

Compile-time asserts are the primary guard.

Runtime/serialization checks:

- snapshot chunk self-test for snapshot/compact round-trip and chunking
- destruction replication self-test for physical entity packets
- live Payload session: join, map-ready, snapshot, movement, hitscan, melee,
  respawn, projectile, chat, duel/TDM if the domain is touched

Report build, serialization test, and live runtime evidence separately.

Do not rely on the stale network smoke tooling: it reads
`PROTOCOL_VERSION` from the wrong header and references a missing test file.
Fix that as a separate tooling task if needed; do not move the constant back.

## Coordination with Agent 1

- Agent 1 must not edit packet domains or wire headers.
- Agent 2 must not edit module/host, server tick/authority, or Agent 1 files.
- `modules.json`, `module-abi.h`, `module-host.*`, public API headers, and
  `packets.h` are serialized: only one agent may edit each at a time.
- Agent 1's request-scoped NPC damage correlation needs a wire field
  (`causeSerial`) and must wait until Agent 2 finishes the relevant domain.
- Auto-landing: a packet/wire lane is a host lane. It requires the one host
  boundary build and human approval before landing. It is not a green
  module-only auto-land.

## Do not do next time

- Do not copy `packets.h` wholesale between branches.
- Do not bump `PROTOCOL_VERSION` for a byte-identical refactor.
- Do not remove or relocate `pack` pragmas without proving every struct size.
- Do not trust loose `<=` asserts as layout proof.
- Do not delete apparently unused wire structs during the move; they may be
  compatibility placeholders.
- Do not mix packet moves with Agent 1's module/host edits in one build window.
- Do not change `MAX_PLAYERS` or snapshot limits as part of this split.
