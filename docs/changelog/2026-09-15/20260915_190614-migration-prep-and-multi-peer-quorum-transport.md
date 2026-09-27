# Migration preparation gating READY + real multi-peer quorum transport

- EST timestamp: 2026-09-15 19:06:14 EDT (UTC 2026-09-15T23:06:14Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--migration-prep-selftest` 14/14; `--transport-generation-selftest` 21 checks; `--generation-verify-selftest` 12/12)

## Report
- SUBSYSTEM: distributed hot-generation — migration preparation + multi-peer quorum.

- MIGRATABLE STATE AUDIT
  - requires migration: dynamic component blobs whose stored version differs from
    the candidate's declared schema version (`DynamicComponentStore`), i.e. real
    persisted package-defined state.
  - does NOT require migration (by design): hot code statics, per-frame render/UI
    commands, transient stack state, and any schema whose version is unchanged.
    Every day-to-day edit (behavior, UI composition, movement constants, weapon
    logic, rendering policy) with unchanged schema yields `NoMigrationRequired` —
    no world copy.

- MIGRATION PLAN
  - identity: `MigrationPlanV1 { fromGeneration, toGeneration, entryCount,
    entries[schemaId, fromVersion, toVersion] }`, bounded to
    `kMaxMigrationEntries = kMaxVerifyRequirements`.
  - storage: internal only; NOT serialized over the network. Each peer prepares
    its own plan against its own live state.
  - function lifetime safety: the plan stores LOGICAL identities only, never
    function pointers. Commit resolves the path against the still-registered
    migrations at the switch boundary via the existing atomic `applySchemaUpdate`.

- NO-OP MIGRATION: identical schema version -> `NoMigrationRequired`, `valid`,
  0 entries, plan matches its exact F->G. PASS.

- REAL SCHEMA MIGRATION
  - prepare: `TestStateV1{value}` v1 stored on a real entity; candidate v2 +
    registered `migrateV1toV2` -> `Prepared`, entry 1->2. Preparation did NOT
    mutate live F (version still 1, value preserved).
  - commit: `applySchemaUpdate({v2})` -> v2, `value` preserved, `bonus` = 42.
  - EntityId preservation: read back on the SAME EntityId. PASS.

- MISSING MIGRATION FAILURE: version change with no registered path ->
  `Failed / MissingMigration`, plan invalid, READY blocked. PASS.

- STALE PLAN INVALIDATION: `migrationPlanMatches` false for X->G and F->H;
  `prepareMigration(from==to)` -> `Failed / StaleSource`. PASS.

- SUPERSEDE BEHAVIOR: the F->G plan does not validate an F->H transition;
  a committed G is not invalidated by later H (existing GenerationDistribution
  rule unchanged). PASS (plan-level).

- MIGRATION FAILURE OVER SOCKET: artifact valid + manifest valid + verify passes,
  but migration preparation fails -> no READY. Proven in the transport harness.
  PASS.

- READY SEMANTICS
  - verify complete? yes (`verifyGeneration`: hash/size/logical gen/ABI/
    capabilities/schemas/dependencies).
  - migration prepared? yes (`prepareMigration` against active F, not Failed).
  - READY allowed? only when both succeed. Server also prepares locally before it
    schedules a switch.

- REAL MULTI-PEER QUORUM TRANSPORT: server + A + B over real loopback UDP. A
  READY, B not -> no quorum, no SWITCH. B READY -> quorum true -> SWITCH
  scheduled. PASS.
- DISCONNECT BEFORE COMMIT: removing B recomputes quorum over survivors (A alone
  satisfies). PASS.
- DISCONNECT AFTER COMMIT: committed G is not cancelled. PASS.

- LATE JOIN: **missing** (cache miss / hit / pending H / committed switch).
- FULL GAME-LOOP TRANSACTION: not done.
- SAME SESSION / ENTITY IDS: not asserted.
- RAW CPP LIVE PROOF / BAD BUILD / BAD VERIFY / BAD MIGRATION LAST-GOOD: none live.

- DISTRIBUTED CODE GENERATION COMPLETE ENOUGH? **No** — migration prep + commit
  substrate, real manifest wire/verify, and real multi-peer quorum + disconnect
  semantics are done; late join, the full game-loop transaction, and live proofs
  remain.

- NEXT: DISTRIBUTED RESOURCES — `ContentArtifactV1 { logicalResourceId,
  resourceKind, contentHash, byteSize }`; reuse `ArtifactCache` +
  `ArtifactStreamer`/`ArtifactReceiver`; first type GLB or PNG; last-good on
  malformed content.

- CONCURRENT / UNRELATED: `--hot-combat-selftest` still fails its hot-navigation
  checks (UI agent in-progress work); not caused by this change.

- LIVE-PROOF DEBT: late join, full-loop transaction, two-process/raw-cpp not
  observed.

## Files changed
- `src/hot-reload/migration-prep.h`: preparation model.
- `src/hot-reload/migration-prep-selftest.{h,cpp}`: selftest suite.
- `src/ecs/dynamic-components.{h,cpp}`: `hasMigration`, `maxStoredVersion`.
- `src/hot-reload/generic-runtime.{h,cpp}`: schema version carried + `schemaAt`
  version out param.
- `src/hot-reload/generation-verify.h`: `requiredSchemaVersions`.
- `src/hot-reload/hot-reload-system.cpp`: manifest populates schema versions.
- `src/network/packets.h`: manifest packet carries `requiredSchemaVersions`.
- `src/network/server.cpp`: sends schema versions in the manifest.
- `src/network/multiplayer-context.h` + `multiplayer-tick.cpp`: prepare migration
  against active F before READY.
- `src/network/transport-generation-selftest.cpp`: migration-failure + real
  multi-peer quorum/disconnect cases.
- `src/game/game-cli.cpp`: `--migration-prep-selftest`.
- docs.

## Next (auto-selected)
Wire the prepared plan's commit into the atomic switch transaction, then late
join (cache miss/hit, pending H, committed switch) and the full game-loop
transaction; then distributed resource/asset generations.
