# Late-join generation bootstrap

- EST timestamp: 2026-09-15 19:28:53 EDT (UTC 2026-09-15T23:28:53Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--generation-bootstrap-selftest` 16/16; transport late-join PASS; full suite 39/39)

## Report
- SUBSYSTEM: distributed hot-generation — late-join bootstrap.

- LATE JOIN STATE MACHINE: `GenerationBootstrapV1`
  (Idle/AwaitingMetadata/Acquiring/Verifying/Ready/Failed) in
  `hot-reload/generation-bootstrap.h`. Bootstrap != READY: it means become locally
  ACTIVE on the generation the server already runs; no fake SWITCH.

- ACTIVE GENERATION BOOTSTRAP DATA: server sends, on join-accept,
  `CodeGenerationPacket.phase = 3` (ACTIVE) + `GenerationManifestPacket` for
  `HotReloadSystem::status().activeGeneration` (via `buildActiveManifest`).

- CACHE-MISS LATE JOIN: transport-proven — advertise → request → BEGIN/CHUNKS →
  reconstruct → hash-commit → verify → Ready → world allowed. Bytes > 0.
- CACHE-HIT LATE JOIN: transport-proven — artifact cached → Verifying directly,
  zero chunk bytes transferred, still hash + manifest verified → Ready.

- SNAPSHOT/WORLD-SYNC GATING: `processSnapshotEntities` returns early while
  bootstrap is active/failed (safe v1: discard). World sync requires
  `Ready && localActive == serverActive`.
- INPUT GATING: input packet send requires `worldParticipationAllowed`; no movement/
  fire/gameplay actions during bootstrap (control/artifact/manifest traffic only).

- ACTIVE G + PENDING H: v1 rule documented — a bootstrapping peer is NOT in the
  active required set; it bootstraps to ACTIVE G first, then joins H's candidate
  transaction according to existing required-peer rules. Not separately transport-
  tested.
- COMMITTED SWITCH + NEW JOIN: documented v1 — the joining peer stays in
  bootstrap outside the frozen committed quorum, then bootstraps directly into the
  post-switch active generation. Not separately tested.
- GENERATION CHANGE DURING BOOTSTRAP: proven — `onServerActiveChanged` re-targets;
  a late completion for the old target cannot activate; cached artifact stays.
- STALE BOOTSTRAP PACKET SAFETY: proven — metadata/artifact/completion tagged with
  another generation are ignored.
- BOOTSTRAP FAILURE BEHAVIOR: proven for ABI mismatch (Failed, no gameplay) and
  code-not-loaded; no fallback to base generation; explicit status via
  `bootstrapStateName`.
- PARTICIPANT/QUORUM MEMBERSHIP: a peer in bootstrap does not count as a required
  gameplay participant (gating + rule documented).
- EXISTING SESSION / ENTITY ID PRESERVATION: bootstrap only prepares the joining
  peer (server sends info; no world/match mutation). Not asserted in a live run.

- LATE JOIN COMPLETE ENOUGH? **Not fully** — the state machine, server
  advertisement, gating, and cache miss/hit transport are real; the client's
  artifact-install-into-loader step and the production-loop end-to-end remain.

- FULL PRODUCTION-LOOP F->G: not done.
- SAME SESSION / ENTITY IDS: not asserted.
- RAW CPP LIVE PROOF / MULTI-FILE LIVE PROOF: none.
- BAD BUILD / BAD VERIFY / BAD MIGRATION / STALE PLAN LAST-GOOD: unit-covered; no
  live process.

- DISTRIBUTED CODE GENERATION COMPLETE ENOUGH? **No** — late-join gates are in
  place; artifact install + full-loop + live proofs remain.

- NEXT: DISTRIBUTED RESOURCES — `ContentArtifactV1 { logicalResourceId,
  resourceKind, contentHash, byteSize }`; reuse `ArtifactCache` +
  `ArtifactStreamer`/`ArtifactReceiver`; first resource PNG/GLB; malformed content
  keeps last-good.

- LIVE-PROOF DEBT: artifact install into the loader, full production-loop
  transaction, two-process/raw-cpp proof.

## Files changed
- `src/hot-reload/generation-bootstrap.h`: state machine.
- `src/hot-reload/generation-bootstrap-selftest.{h,cpp}`: selftest suite.
- `src/hot-reload/hot-reload-system.{h,cpp}`: `buildActiveManifest`.
- `src/network/multiplayer-context.h`: `generationBootstrap`.
- `src/network/multiplayer-tick.cpp`: phase-3 handling, manifest/artifact hooks,
  input + snapshot gating.
- `src/network/packets.h`: `CODE_GENERATION_PHASE_ACTIVE_BOOTSTRAP`.
- `src/network/server-packets.cpp`: active-generation bootstrap on join.
- `src/network/transport-generation-selftest.cpp`: late-join cache miss/hit tests.
- `src/game/game-cli.cpp`: `--generation-bootstrap-selftest`.
- docs.

## Next (auto-selected)
Install the downloaded artifact into the hot loader (`HotReloadSystem`) so a late
joiner can complete bootstrap end-to-end; then run the full production-loop F->G
transaction with same-session/entity assertions; then distributed resources.
