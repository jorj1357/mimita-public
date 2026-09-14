# Generic dynamic entity/component lifecycle (ABI v6)

- EST timestamp: 2026-09-14 11:47:38 EDT (UTC 2026-09-14T15:47:38Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + all self-tests + live DLL execution);
  live multi-generation add/rename/syntax proof is inherited from the prior
  session and not re-run here

## Migration standard

1. **Old cold path.** A hot package could only store dynamic component bytes for
   a schema id it declared; there was no generic way to create/destroy a package
   entity, remove/enumerate/inspect a dynamic component, express relationships,
   or execute schema migrations. `GenericRuntime::activate` also registered
   dynamic schemas mid-staging and keyed migrations by a truncated 32-bit id.
2. **New hot path.** `GameplayContextV1` (ABI v6) exposes operation-generic
   capabilities; the dynamic store owns schema versions, 64-bit migrations, and
   an atomic activation-time migration; a hot package declares schemas/migrations
   and drives them through those capabilities.
3. **Kernel mechanisms retained.** `EntityRegistry` identity/allocation and raw
   component storage; `DynamicComponentStore` bytes; `RelationshipStore` edges;
   the `GenericRuntime` package registry; the generation loader.
4. **New generic capabilities.** `entity.create`, `entity.destroy`,
   `component.remove`, `component.enumerate`, `component.typesOnEntity`,
   `component.schema`, `relationship.add/remove/query` (all registered as kernel
   capability ids). Plus `DynamicComponentStore::serializeType`/`worldHash`.
5. **Why general, not feature-specific.** Every operation is generic over a
   64-bit schema/relationship id declared at runtime; none is per-component-type,
   and no new EXE enum or per-feature callback was added.
6. **State ownership.** `DynamicComponentStore` owns dynamic bytes, schema
   versions, migrations, and deterministic serialization;
   `Project::StateSchemaRegistry` keeps typed POD state (unchanged, 32-bit); the
   entity registry owns lifetime and purges both stores on destroy.
7. **Migration / rollback.** Migrations run inside `activate` before any runtime
   registration commits. A failed migration rejects the candidate and the loader
   retires it (`hot-reload-system.cpp:446-456`), so old code and old bytes both
   survive. Migrations are forward-only; rollback across a forward schema
   migration needs a reverse migration (multiplayer state agreement is future).
8. **Files changed.** `src/hot-reload/game-api.h`,
   `src/hot-reload/generic-runtime.cpp`, `src/live-code/live-behavior.cpp`,
   `src/ecs/dynamic-components.{h,cpp}`, `src/ecs/entity-registry.{h,cpp}`,
   new `src/ecs/relationship-store.{h,cpp}`,
   new `src/ecs/dynamic-lifecycle-selftest.{h,cpp}`, `src/game/game-cli.cpp`,
   `src/hot-reload/modules/generic-demo-package.cpp`,
   new `src/hot-reload/modules/banana-component.cpp`.
9. **Evidence.** See below.
10. **Remaining cold pieces.** Dynamic-component replication from `networkPolicy`,
    editor edit/copy-paste UI for dynamic components, typed legacy structs.
11. **Next cold call site.** Projectile policy in
    `src/network/server-projectiles.cpp` (Priority 3), then gamemode scoring.

## What changed

- `GameplayContextV1` ABI `MIMITA_GAME_API_VERSION` 5 -> 6 with append-only
  generic lifecycle capabilities; `kernelProvidesCapability` lists their ids.
- `DynamicComponentStore`: schema `version`, version-stamped blobs,
  `registerMigration` keyed by 64-bit type id, atomic `applySchemaUpdate`
  (migrates existing blobs or fails without mutating), deterministic
  `enumerate`/`componentsOnEntity`/`serializeType`/`worldHash`.
- `EntityRegistry::createGeneric` allocates package entity ids in
  `EntityDomain::None` with a kernel monotonic id; `destroy` purges dynamic
  components and relationship edges.
- `GenericRuntime::activate` stages dynamic schemas and routes migrations to the
  dynamic store (64-bit) or the project registry (32-bit), then applies the
  schema update before committing; no mid-staging schema mutation.
- New `RelationshipStore`: typed directed edges keyed by relationship hash.
- `--dynamic-lifecycle-selftest` (new) and the live hot probe
  `banana-component.cpp`.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS` (compiled 7 dependents via the
  `game-api.h` change, linked `mimita.exe`, 15.2s).
- `mimita.exe --dynamic-lifecycle-selftest` -> **PASS (28/28)**, including
  migration v1->v2 preserving data, a failing migration rejecting the candidate
  while schema/bytes stay at v2, deterministic serialization, and destroy purge.
- All prior self-tests PASS: movement, movement-parity, entity-slice,
  hot-authoritative, live-code, project, phase456, telemetry, creation,
  ragdoll-slice.
- Live DLL proof: `--movement-parity-selftest` loads the real package and runs
  `GAME_DOMAIN_GAMEPLAY`; output shows
  `[SYSTEM_REGISTERED] system=banana.lifecycle`, `[EVENT_REGISTERED]
  event=banana.component.changed`, and the system creating entity 1 and
  attaching/reading/modifying/enumerating the live `BananaComponent`.
- `-fsyntax-only` clean for every changed cold source (real build flags) and for
  the changed hot sources with `-DMIMITA_GAME_DLL`.
- No regressions recorded.

## Honest limitations

- The full single-process add/rename/delete/syntax-error proof was run in the
  2026-09-13 session for systems/events/capabilities; this pass did not re-run it.
- Runtime human proof is still required for a live schema-version edit across
  generations and for any visual result.
- Dynamic components are not replicated, not editor-editable, and have no
  copy/paste integration yet.
- `Project::ComponentLayout` still truncates 64-bit schema ids to 32 bits for the
  project-layer tooling view only; the authoritative dynamic migration path does
  not.

## Next

- Priority 3: migrate projectile/weapon policy out of `server-projectiles.cpp`
  onto the same component/relationship/capability substrate.
- Then gamemodes as packages; then per-entity behavior bindings.
