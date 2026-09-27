# Generic relationship replication on the runtime-state substrate

Date: 2026-09-14 19:15 EST (UTC 2026-09-14T23:15:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## Scope

This pass extends the already-landed generic dynamic-component replication
(`src/network/dynamic-replication.*`) to relationships, fixes two correctness
gaps in the per-client sender, wires the listen/host server tick, and adds
focused self-test coverage. It does not redesign networking, prediction,
interpolation, inventory, or `GenericRuntime`.

## What changed

- `src/ecs/relationship-store.{h,cpp}`: added the generic change marker
  (`RelationshipEdge::changeVersion`, bumped on add/update/remove), a
  deterministic `edgesOfType` enumerate, `typeIds`, `changeVersionOf`,
  `addVersioned` (adopts the sender's changeVersion and rejects stale records),
  and per-type `networkPolicy`/`setNetworkPolicy`. A relationship type becomes
  replicable (`GAME_NET_ALL`) the first time an edge is added, so a type unknown
  to the EXE at startup replicates with no cold registration.
- `src/network/dynamic-replication.{h,cpp}`: the envelope now carries
  `RelationshipRecord` (op 3 add/update, op 4 remove) in addition to component
  records and schema descriptors. Decode gained a relationship output; apply
  now takes the `RelationshipStore` and applies edges with no type switch.
- `serverReplicateDynamicComponents`: per-client diff of components and
  relationships, removal detection by diffing the previously-sent set,
  `GAME_NET_OWNER` filtered by source entity, and bounded multi-packet batching
  so truncation no longer drops (and wrongly marks sent) changed records.
- `src/network/server.cpp`: the listen/host server tick now calls
  `serverReplicateDynamicComponents`, matching the dedicated server tick.
- `src/network/multiplayer-tick.cpp`: client applies relationship records into
  `RelationshipStore::instance()`.
- `src/network/dynamic-replication-selftest.cpp`: new checks for a relationship
  type unknown at startup (collected, decoded, applied), relationship update,
  relationship remove, and component removal detection.

## Files changed

`src/ecs/relationship-store.h`, `src/ecs/relationship-store.cpp`,
`src/network/dynamic-replication.h`, `src/network/dynamic-replication.cpp`,
`src/network/multiplayer-tick.cpp`, `src/network/server.cpp`,
`src/network/dynamic-replication-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS` (canonical `mimita.exe`).
- `mimita.exe --dynamic-replication-selftest` -> **PASS**, incl.
  "new runtime relationship type is collected without registration",
  "client received the new relationship edge", "relationship update
  propagates", "relationship remove propagates", plus all component checks and
  v1->v2 migration with last-good preservation.
- `--hot-combat-selftest`, `--dynamic-lifecycle-selftest`, `--live-code-selftest`,
  `--capability-selftest`, `--gamemode-hot-selftest`,
  `--movement-parity-selftest`, `--hot-authoritative-selftest` -> **PASS**.

## Honest limitations

- Arbitrary runtime entities are not yet network-visible as entities. Only their
  dynamic component and relationship records replicate, and the client applies
  them into its stores; generic `ENTITY_CREATE`/`ENTITY_DESTROY` replication is
  the next slice. Documented in `hot-cold-audit.md`.
- Component records are not yet stale-rejected on the client (relationships
  are). Reliable-ordered delivery makes this low risk for now.
- No live two-client network/visual acceptance was performed; the proof is the
  headless self-test plus the canonical build. `HotProjectileStateV1` is
  `GAME_NET_ALL` and proven to flow through the generic path, but client-side
  rendering of replicated projectile state is not wired.
- This session overlapped a concurrent session editing the same worktree; the
  shared `relationship-store.*` files were reconstructed as one coherent version
  after an overlapping-edit corruption. Pre-existing concurrent edits elsewhere
  were preserved and are not claimed here.
