# Generic dynamic-component + relationship replication

- EST timestamp: 2026-09-14 19:12:53 EDT (UTC 2026-09-14T23:12:53Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--dynamic-replication-selftest` 18/18 +
  full suite 14/14)

## What this proves
A component type invented after startup can be network-visible to clients
without the EXE knowing its type. One opaque envelope carries schema
descriptors, component upserts, removals, and relationship edges for **any**
dynamic type, selected by the schema/relationship `networkPolicy`. A real
gameplay component (`HotProjectileState`) uses the same path.

## Design
- **One envelope, no per-component packet/codec.** `network/dynamic-replication.{h,cpp}`
  defines a single `DynamicComponentRecord` (+ `RelationshipRecord`) wire format:
  `PacketHeader + eventId/session + records`, where each record carries
  `entity/typeId/schemaHash/schemaVersion/changeVersion/payloadSize/payload`.
  Carried over the existing reliable gameplay-event channel.
- **One framing type.** `PACKET_DYNAMIC_COMPONENT` is a single generic envelope
  type. The reliable channel needs a routable `PacketHeader.type`, and there was
  no opaque general message type, so exactly one generic type was added. There is
  no per-component packet, struct, encoder, or decoder.
- **Schema distribution.** Schema descriptors (`op = 2`) are sent once per client
  until known; clients cache by typeId and register unknown schemas generically.
- **Network policy from metadata.** `GAME_NET_NONE/ALL/OWNER/SERVER_ONLY`;
  replication eligibility is schema/relationship data, never a name switch.
- **Generic change tracking.** `DynamicComponentStore` tracks a per-blob
  `changeVersion` and a `RelationshipStore` a per-edge `changeVersion`; the
  server sends only changes (plus removals detected by per-client diffing) and
  no component-specific "mark dirty" call is required.
- **Generic client apply.** `dynamicReplicationApply` validates schema
  identity/version/size, registers unknown schemas, migrates on version change
  through registered migrations, attaches/updates/removes components and applies
  relationship edges. Mismatched payload sizes and incompatible schemas are
  rejected without reinterpreting bytes.
- **Relationships too.** The same envelope replicates `contains-item`,
  `equips-item`, and any relationship type (default policy ALL on first use).

## Evidence (`--dynamic-replication-selftest`, real DLL, 18/18)
- new runtime component collected; real gameplay component (`HotProjectileState`)
  on the generic path; new relationship type collected with no registration;
- envelope decodes; record count round-trips;
- client applies records: receives schema + state, receives the real gameplay
  component, receives the new relationship edge;
- component update / remove / re-add propagate; relationship update / remove
  propagate;
- mismatched payload rejected and state preserved;
- v2 schema migration applied on the client, compatible state preserved;
- a version with no migration is rejected and last-good state remains.
- Full suite PASS 14/14.

## Files changed
`src/ecs/dynamic-components.{h,cpp}` (changeVersion + removal log + accessors),
`src/ecs/relationship-store.{h,cpp}` (edge views, typeIds, changeVersion,
policy, addVersioned), `src/hot-reload/game-api.h` (`GAME_NET_*` policy
constants), `src/network/dynamic-replication.{h,cpp}` (new),
`src/network/dynamic-replication-selftest.{h,cpp}` (new),
`src/network/packets.h` (`PACKET_DYNAMIC_COMPONENT`),
`src/network/multiplayer-tick.cpp` (client apply branch),
`src/network/server.cpp` (replicate each fixed step),
`src/hot-reload/modules/tools/hot-projectiles.cpp`
(`HotProjectileState` networkPolicy ALL), `src/game/game-cli.cpp`; docs + this
changelog.

## Honest limitations
- The single generic packet type is the one transport framing addition; it is
  not per-component.
- OWNER policy is implemented for player-domain entities; relevance heuristics
  and per-type policy breadth are minimal.
- Entity lifecycle (generic entity create/destroy replication) is not added;
  components replicate on existing entities.
- The live server + 2-client falsification was not run (no multiplayer harness);
  the headless test exercises the real encode/decode/apply path and the server
  integration compiles/runs each fixed step.
- Tree co-edited by the gameplay agent; relationship replication in this
  envelope was added concurrently and is integrated and green.

## Next
Generic entity lifecycle replication; move hot projectile/tool state across the
network generically; remove `runtimeToolId`/`equippedToolEntity` bridges; move
registered-weapon ammo/cooldown/reload into components; then hot
snapshot/relevance and prediction/interpolation/reconciliation policy.
