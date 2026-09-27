# Chunked artifact wire transfer (partial)

- EST timestamp: 2026-09-15 16:02:09 EDT (UTC 2026-09-15T20:02:09Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--artifact-transfer-selftest` 13/13; full suite 33/33)

## Report
- SUBSYSTEM: distributed hot-generation artifact transfer.
- ARTIFACT WIRE TRANSFER:
  - implemented? **Protocol + stream/reassembly implemented; live socket wiring
    NOT yet.** New packets `ArtifactRequestPacket` (client→server),
    `ArtifactBeginPacket` (server→client header), `ArtifactChunkPacket`
    (server→client indexed payload). `ArtifactStreamer` chunks a verified
    artifact; `ArtifactReceiver` reassembles.
  - protocol/messages: `PACKET_ARTIFACT_REQUEST=78`, `PACKET_ARTIFACT_BEGIN=79`,
    `PACKET_ARTIFACT_CHUNK=80`.
  - chunking: bounded `ARTIFACT_CHUNK_BYTES=1000`, indexed + offset + exact size,
    total size + chunk count in BEGIN.
  - failure behavior: duplicate safe, out-of-order safe, wrong hash/size/range
    rejected, missing chunk never valid, disconnect fails cleanly, invalid begin
    (size/chunk mismatch) fails.
- CACHE:
  - cache-hit proof: `ArtifactReceiver`/`ArtifactAcquirer` reuse
    `ArtifactCache` (content-addressed); a cached hash skips transfer (verified in
    `--artifact-cache-selftest`).
  - immutable behavior: never overwrites an existing hash; re-verify stored bytes.
- VERIFY:
  - hash: enforced (`hashArtifactBytes`) on store and before commit.
  - ABI / capabilities / schema / dependencies: **MISSING** peer-side gate
    (candidate load validation exists locally via `HotReloadSystem`).
  - load validation: local; not yet invoked on an acquired artifact in the live
    client path.
- REAL CLIENT READY:
  - actual packet path: **MISSING** (no live REQUEST→BEGIN→CHUNK→READY exchange
    yet).
  - exact generation association: `GenerationDistribution`/`GenerationIdentityV1`
    key per-peer by logical generation; READY(G) cannot satisfy H.
- QUORUM:
  - required peer definition: server + all participating non-spectator clients
    (v1, per `quorumReady` required set).
  - disconnect behavior: `removePeer` updates quorum deterministically (proven).
- TICK MAPPING: **MISSING** — "local tick ≥ switchTick" not proven equal to
  authoritative server ticks; requires explicit mapping. Documented as required.
- ATOMIC SWITCH:
  - pre-T generation: F (existing switch semantics held until `switchAtTick`).
  - at-T generation: G via existing safe-tick activation in `HotReloadSystem`.
  - rollback behavior: last-good preserved inside `HotReloadSystem`.
- GENERATION PROVENANCE:
  - reconciliation: **real** (predicted=local active, authoritative=server).
  - interpolation: **placeholder 0** (needs per-sample field).
  - rewind: **placeholder 0** (needs per-sample field).
- LATE JOIN: **MISSING**.
- MULTIPLE CANDIDATES: `supersedes()`/cancel exist; transfer/READY cancellation
  for a superseded generation is not yet wired (no live transfer yet).
- SELFTESTS: `--artifact-transfer-selftest` 13/13, `--artifact-cache-selftest`
  13/13, `--generation-distribution-selftest` 10/10; full suite 33/33.
- REAL SERVER+CLIENT PROOF: **NO**.
- LIVE RAW-CPP EDIT PROOF: **NO**.
- BAD-EDIT LAST-GOOD PROOF: partial (local `HotReloadSystem` last-good only; not
  live two-process).
- SAME PID/SESSION/ENTITY-ID PROOF: **NO** (requires the live two-process run).
- DISTRIBUTED GENERATION MILESTONE COMPLETE? **NO**.
- REMAINING BLOCKERS:
  1. Wire the live client/server packet handlers (request/stream/receive + READY)
     in `multiplayer-tick.cpp` / `server-packet-handlers.cpp` / `server-packets.cpp`.
  2. Authoritative tick-domain mapping + proof.
  3. Per-sample generation provenance (interpolation/rewind sample structs).
  4. Peer-side ABI/capability/schema/dependency verify gate on acquired artifact.
  5. Late join + superseded-transfer cancellation.
  6. The live two-process proof itself.
- NEXT COLD OWNER: wire the live transfer + READY path and prove the tick mapping,
  then the two-process proof.

## Notes
- Discipline: implemented only the transfer protocol/reassembly + selftest and did
  not scope-shift into editor/CRDT/asset-upload/presentation/movement. The live
  handler wiring was intentionally not rushed because it is networking-critical and
  cannot be validated without a running server+client here.
- `begin()` now validates before committing state, so a rejected stream cannot
  clobber a valid one (found via the selftest).

## Files changed
`src/network/packets.h` (artifact packets),
`src/hot-reload/artifact-transfer.{h,cpp}` (new),
`src/hot-reload/artifact-transfer-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Wire `PACKET_ARTIFACT_REQUEST/BEGIN/CHUNK` + READY into the live client/server
path; define/prove the authoritative tick-domain mapping; then run the
two-process switch proof (same PID/session/EntityIds, F→G at T).
