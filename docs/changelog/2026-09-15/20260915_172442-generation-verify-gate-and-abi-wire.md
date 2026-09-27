# Generation verify gate + hot-ABI wire

- EST timestamp: 2026-09-15 17:24:42 EDT (UTC 2026-09-15T21:24:42Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--generation-verify-selftest` 8/8; full suite 36/36)

## Report
- SUBSYSTEM: distributed generation verify gate.
- VERIFY MANIFEST: `GenerationManifestV1` (logicalGenerationId,
  logicalBehaviorHash, platformArtifactHash/size, hotAbiVersion, bounded
  requiredCapabilities/Schemas/Dependencies arrays) + `GenerationLocalFactsV1`.
- ABI VERIFY: **implemented** — `verifyGeneration` returns `AbiMismatch` when the
  manifest hot ABI differs from the peer cold ABI; the ABI is now carried on the
  wire (`CodeGenerationPacket.hotAbiVersion`) and the client refuses to send
  READY when incompatible.
- CAPABILITY VERIFY: **implemented** (generic `hasCapability(id)` probes; no
  feature-specific tables). Schema verify: **implemented** (`hasSchema(hash)`).
  Dependency verify: **implemented** (`hasDependency(id)`).
- MIGRATION PREP: **not implemented** (failure reason enum exists:
  `MigrationFailed`).
- VERIFY FAILURE OVER TRANSPORT: hash-mismatch unit-proven; ABI mismatch enforced
  on the live READY path (compiled) but not yet exercised over the socket test.
- QUORUM REAL TRANSPORT: not socket-tested (bookkeeping unit-tested).
- DISCONNECT QUORUM BEHAVIOR: `removePeer` deterministic (unit-proven).
- LATE JOIN / CACHE-HIT LATE JOIN / DURING PENDING CANDIDATE / DURING COMMITTED
  SWITCH: **missing**.
- FULL GAME-LOOP TRANSACTION: **not done** (loopback packet transport proven in
  Round 51, not the full server/client loop).
- SAME SESSION / ENTITY ID PROOF: **no** (needs the full loop).
- REAL TWO-PROCESS PROOF: **NO**. RAW CPP ONE-FILE / MULTI-FILE REFACTOR / BAD
  BUILD LAST-GOOD / BAD VERIFY LAST-GOOD: **NO** (live).
- DISTRIBUTED CODE GENERATION COMPLETE ENOUGH? **Not yet** — the gate exists and
  ABI is enforced on the real READY path; schema/dependency requirements are not
  yet populated on the wire, migration prep and late join are missing, and no
  full-loop/live proof exists.
- NEXT CATEGORY: distributed resources.
- RESOURCE TRANSPORT REUSE PLAN: reuse `ArtifactCache` (content-addressed,
  immutable) + `ArtifactStreamer`/`ArtifactReceiver` (chunked) + hash verify +
  generation publication; generalize the identity to a `ContentArtifact
  { logicalResourceId, resourceKind, contentHash, byteSize }` rather than a
  second protocol.
- RESOURCE PUBLICATION SEMANTICS: simulation code must coordinate a shared tick;
  presentation resources may typically publish independently after verified
  acquisition (to be implemented next category).
- LIVE-PROOF DEBT: full-loop/two-process transaction not observed.

## Notes
- The gate is generic (capability/schema/dependency probes), so wiring the
  remaining requirement arrays onto the wire is additive and small.
- Concurrency: the build staging step hit a transient `PermissionError`
  (a concurrent process held a runtime DLL); the exe relinked and the suite
  passed 36/36.

## Files changed
`src/hot-reload/generation-verify.h` (new),
`src/hot-reload/generation-verify-selftest.{h,cpp}` (new),
`src/network/packets.h` (`CodeGenerationPacket.hotAbiVersion`),
`src/network/server.cpp` (announce ABI), `src/network/multiplayer-context.h` +
`multiplayer-tick.cpp` (store ABI, gate READY), `src/game/game-cli.cpp`;
docs + this changelog.

## Next (auto-selected)
Populate schema/dependency requirements on the announce and call
`verifyGeneration` in the client READY path; add migration-prep; late join; then
the full-loop/two-process proof; then distributed resource/asset generations.
