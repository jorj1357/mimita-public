# Generic constraint networking + entity inspector/editor (source + bootstrap)

- EST timestamp: 2026-09-13 18:08:07 EDT (UTC 2026-09-13T22:08:07Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + headless self-tests); two-client runtime proof pending

## Part A — Generic constraint replication

### Canonical identity
- `EntityDomain::Constraint = 6` (`src/ecs/entity-types.h`).
- `src/physics/constraints/constraint-store.h/.cpp` (new): one dedicated
  Constraint entity per serial, owner-scoped serial allocation
  (`(owner<<16)|counter`), release tombstones, active-set queries.
- `ragdoll-entities.cpp`: a grab now allocates a serial, maintains the
  dedicated constraint entity through the store, and clears it on release;
  `GrabComponent.constraintSerial` links hand -> constraint.

### Wire / lifecycle
- `packets.h`: `PACKET_CONSTRAINT_CREATE_REQUEST(73)`, `..._CREATE(74)`,
  `..._RELEASE(75)`, `..._SNAPSHOT(76)`; `ConstraintWire` +
  `ConstraintCreatePacket`/`ConstraintReleasePacket`/`ConstraintSnapshotPacket`
  (eventId/eventSessionId kept at the reliable-event offsets; snapshot capped to
  12).
- `src/network/constraint-codec.h/.cpp` (new): `ConstraintComponent` <->
  `ConstraintWire`.
- `src/network/server-constraints.h/.cpp` (new): validate create/release intent
  (auth owner, type, limb range, body ids), own the store, broadcast reliable
  create/release, and send the active-set snapshot to a joiner.
- `server-packets.cpp`: dispatch branches; `isKnownPacketType` bound bumped to
  `PACKET_CONSTRAINT_SNAPSHOT`; early-join active-constraint snapshot.
- Client (`multiplayer-tick.cpp`, `multiplayer-packets.cpp`,
  `multiplayer-context.h`): instant local prediction (grab creates the constraint
  immediately), create-request retry until confirmed, release on end, apply
  authoritative create/release/snapshot via the store.
- Ordering: reliable dedup (`mpAcceptReliableEventOnce`), release tombstones
  (release-before-create drops the create), release tick.

## Part B — Generic entity inspector / editor

- `src/editor/entity-inspector.h/.cpp` (new): `inspectEntity(EntityId)` returns
  `EntityInspection` (identity, transform, components, behaviors, resources,
  network, constraint, telemetry) with `toText()`/`toJson()`; the single
  inspector data owner.
- `src/editor/creation-mode.cpp`: `pick` now does a full nearest-hit raycast over
  world triangles **and** entities (players/NPCs/projectiles/ragdoll limbs via
  ray-sphere bounds); `describe` delegates to `inspectEntity`; `WorldObjectComponent`
  added for authored map-object identity.
- `src/terminal/creation-commands.cpp`: added `inspect`, `select`,
  `constraint_list` (terminal-first, same data for future GUI).
- `CreationMode` now records `Project::ChangeSet` entries per edit and
  `commit()` appends a `Project::ProjectVersion` (parent/tree/changeId/UTC) —
  reuses the project vocabulary; the source GLB is never rewritten.

### Telemetry
- Constraint store counts and receive/create flows are inspectable via
  `constraint_list` and `Telemetry::Registry::entityJson`.

## Evidence

- `-fsyntax-only` clean (mingw64 g++, `-std=c++17`, pch) for all changed TUs.
- `python build_agent.py` -> `BUILD SUCCESS`, mimita.exe relinked
  (`Compiled: 137, Skipped: 398`).
- `mimita.exe --ragdoll-slice-selftest` -> **PASS** (30 checks, including
  dedicated constraint serial/entity, codec round-trip, tombstone/out-of-order,
  late-join reconstruction).
- `--live-code-selftest`, `--hot-authoritative-selftest`,
  `--entity-slice-selftest`, `--project-selftest`, `--phase456-selftest`,
  `--telemetry-selftest`, `--creation-selftest` -> all **PASS**.

## Runtime proof (pending, not claimed)

- Two-client: same constraint serial on both peers, move while active, release
  removes it on both, create a second, late third client reconstructs the active
  set.
- Editor: `modecreate 1`, point at a limb, `inspect`, copy/paste/move/rotate/
  scale/delete, `create_patch` fork hash changes, source GLB unchanged.

## Known limitations

- Server owns constraint lifecycle only; owners remain the body physics
  authority (instant local prediction, server validation + broadcast).
- Cross-actor two-body solve still requires local simulation of both bodies;
  the wire model already supports bodyA/bodyB/limbs/anchors for it.
- Map-object identity is `sourceIndex`-based today; stable glTF node identity
  across reload/Blender round-trip is future work.
- Component copy policy categories are not yet extracted into a registry; edit
  ops currently apply to transforms/duplication only.

## Files

New: `src/physics/constraints/constraint-store.h/.cpp`,
`src/network/constraint-codec.h/.cpp`, `src/network/server-constraints.h/.cpp`,
`src/editor/entity-inspector.h/.cpp`.

Changed: `src/ecs/entity-types.h`, `src/ecs/components.h`,
`src/physics/constraints/constraint-components.h`,
`src/ragdoll/ragdoll-components.h`, `src/ragdoll/ragdoll-entities.cpp`,
`src/ragdoll/ragdoll-slice-selftest.cpp`, `src/network/packets.h`,
`src/network/multiplayer-context.h`, `src/network/multiplayer-packets.cpp`,
`src/network/multiplayer-tick.cpp`, `src/network/server-packets.cpp`,
`src/network/server.cpp`, `src/editor/creation-mode.h/.cpp`,
`src/terminal/creation-commands.cpp`.

## Pre-existing edits preserved

All unrelated working-tree changes were preserved.
