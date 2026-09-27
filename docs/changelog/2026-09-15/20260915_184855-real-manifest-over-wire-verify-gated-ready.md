# Real generation manifest over the wire + verify-gated client READY

- EST timestamp: 2026-09-15 18:48:55 EDT (UTC 2026-09-15T22:48:55Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--transport-generation-selftest` 19 checks; `--generation-verify-selftest` 12/12)

## Report
- SUBSYSTEM: distributed hot-generation — manifest carriage + client READY gate.

- MANIFEST WIRE
  - requirement arrays: **real** — the server builds `GenerationManifestV1` via
    `HotReloadSystem::buildCandidateManifest` (identity + ABI + the package's
    declared capability requirements and registered schemas) and sends it as a
    bounded `GenerationManifestPacket` (type 81): scalar identity + capped
    `requiredCapabilities[8]` / `requiredSchemas[8]` / `requiredDependencies[8]`.
  - identity binding: manifest carries `logicalGenerationId` +
    `logicalBehaviorHash` + `platformArtifactHash` + size + `hotAbiVersion`; a
    manifest for G fails verify against H's artifact (reflected in
    `verifyGeneration` HashMismatch and by binding storage to the generation).
  - supersede safety: client stores the manifest keyed by exact
    generation and only binds it when it matches the most recently announced
    `serverCodeGeneration`; a late G manifest cannot validate H. Cache entries
    stay immutable.

- CLIENT READY VERIFY
  - real local facts: yes — `artifactHash`/`artifactSize` from the committed
    receiver; `coldAbiVersion = MIMITA_GAME_API_VERSION`; capability probe =
    `GenericRuntime::hasCapability`; schema probe = `DynamicComponentStore::schema`;
    dependency probe = trivially satisfied (v1).
  - ABI: enforced via `manifest.hotAbiVersion` in `verifyGeneration`.
  - capabilities: verified against the live registry.
  - schemas: verified against the live registry.
  - dependencies: serialized / validated / participate in verifyGeneration; v1 empty.
  - **No READY from hash alone**: READY requires `hash ok && verify == None`.

- VERIFY FAILURE OVER SOCKET
  - ABI: proven cross-socket (artifact valid, ABI off-by-one -> AbiMismatch, no READY).
  - capability: proven cross-socket (unknown required capability -> MissingCapability, no READY).
  - schema: proven cross-socket (unknown required schema -> SchemaMismatch, no READY).
  - also: identity mismatch -> HashMismatch; oversized requirement count rejected.

- MIGRATION PREP: **not implemented**.
- MIGRATION COMMIT: not implemented.
- MIGRATION ROLLBACK/FAILURE: not implemented (reason enum exists).

- MULTI-PEER QUORUM TRANSPORT: not done (rules/unit level only).
- DISCONNECT BEFORE COMMIT: not done.
- DISCONNECT AFTER COMMIT: not done.

- LATE JOIN: **missing** (cache miss / cache hit / pending H / committed switch).

- FULL GAME-LOOP TRANSACTION: not done (packet transport proven).
- SAME SESSION / ENTITY IDS: not asserted.

- RAW CPP LIVE PROOF: no.
- MULTI-FILE HOT PROOF: no.
- BAD BUILD LAST-GOOD: no (live).
- BAD VERIFY LAST-GOOD: unit/propedeutic only (no live process).

- DISTRIBUTED CODE GENERATION COMPLETE ENOUGH? **No** — manifest is now real and
  carried over the wire, and client READY depends on real `verifyGeneration` with
  real facts; but migration prep/commit, multi-peer quorum over transport, late
  join, the full-loop transaction, and live proofs remain.

- NEXT: DISTRIBUTED RESOURCES
  - FIRST RESOURCE TYPE: start with GLB or PNG (already presentation-supported).
  - CONTENT DESCRIPTOR: `ContentArtifactV1 { logicalResourceId, resourceKind,
    contentHash, byteSize }`.
  - TRANSPORT REUSE: `ArtifactCache` + `ArtifactStreamer`/`ArtifactReceiver`,
    same hashes; no new protocol.
  - LAST-GOOD RESOURCE PROOF: malformed replacement keeps the logical id on the
    previous good content.

- CONCURRENT / UNRELATED: `--hot-combat-selftest` fails "hot navigation state is
  migratable component state" — a UI agent's in-progress hot-navigation feature in
  the working tree, not caused by this change. My touched selftests pass.

- LIVE-PROOF DEBT: full-loop / two-process transaction not observed.

## Files changed
- `src/network/packets.h`: `PACKET_GENERATION_MANIFEST = 81`,
  `GenerationManifestPacket`, `GENERATION_MANIFEST_VERSION`,
  `GENERATION_MANIFEST_MAX_REQUIREMENTS`.
- `src/hot-reload/hot-reload-system.{h,cpp}`: `buildCandidateManifest`.
- `src/hot-reload/generic-runtime.h`: per-entry requirement/schema accessors.
- `src/network/server.cpp`: build + send the bounded manifest packet on announce.
- `src/network/multiplayer-context.h`: pending manifest keyed by generation +
  `pendingVerifyFailure`.
- `src/network/multiplayer-tick.cpp`: manifest handler (bounded decode, supersede
  safety) + verify-gated READY with real local facts.
- `src/hot-reload/artifact-transfer.h`: `ArtifactReceiver::totalSize()`.
- `src/network/transport-generation-selftest.cpp`: manifest wire + verify-gate tests.
- `src/hot-reload/generation-verify-selftest.cpp`: runtime-derived manifest tests.
- docs.

## Next (auto-selected)
Migration preparation (prepare-not-commit; schema/version-aware; `MigrationFailed`
gates READY; invalidate stale F→G plans), then multi-peer quorum over real
transport, disconnect semantics, late join, full-loop transaction; then distributed
resource/asset generations.
