# Generic logical-resource / content-artifact primitive (PNG/GLB/WAV)

- EST timestamp: 2026-09-15 20:09:00 EDT (UTC 2026-09-16T00:09:00Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--content-resource-selftest` 17/17)

## Report
- SUBSYSTEM: generic content artifact / logical resource primitive.

- CONTENT ARTIFACT PRIMITIVE
  - descriptor: `ContentArtifactV1 { logicalResourceId, resourceKind, contentHash,
    byteSize }`.
  - artifact reuse: YES — bytes live in the EXISTING immutable `ArtifactCache`;
    the existing `ArtifactRequest/Begin/Chunk` packets already carry an opaque
    content hash, so no new transfer protocol is introduced (logical id rides
    `logicalGenerationId`).
  - logical identity: `resourceIdFromLogicalName("ui.menu.logo")` (hash of the
    stable logical name); hot code/entities reference only this.
  - version identity: content hash (no shared integer with code generations).
  - publication: kind validator (Code/PNG/GLB/WAV structural) then atomic mapping
    swap; unknown kinds rejected.

- RESOURCE REGISTRY (`ResourceRegistry`)
  - active / candidate / last-good: `ResourceVersionState { activeHash,
    candidateHash, lastGoodHash }`.
  - stale candidate behavior: a late publish for a superseded candidate hash is
    rejected ("not the advertised candidate"); the newer candidate publishes.
  - non-destructive: A stays active while B is validated; only then the mapping
    swaps; `lastGoodHash` retains A.

- PNG LIVE
  - cache miss: publish A->B with bytes passed directly; last-good A. PASS.
  - cache hit: `publishCandidateFromCache` with zero chunks; resolve B. PASS.
  - valid publish: resolve("ui.menu.logo") == B. PASS.
  - malformed last-good: hash-valid but non-PNG bytes rejected; resolve stays B. PASS.
  - same session/process: not run live (no in-world menu swap this pass).

- GLB LIVE: publish A->B, cache hit, malformed last-good all PASS at the registry
  level. Same Tool EntityId / equip relationship: NOT yet wired to an in-world
  entity (next).
- WAV LIVE: publish A->B, cache hit, malformed last-good PASS at the registry
  level. Same logical id: yes.

- CONTENT PIPELINE COMPLETE ENOUGH? **Partially** — one descriptor model, one
  artifact path, stable logical identity, PNG/GLB/WAV publish, cache hit,
  malformed last-good, and supersede safety are proven at the primitive level.
  Still missing: descriptor over the real socket, in-world consumption (menu logo,
  Tool mesh), and the raw-pointer-across-generations check in a real consumer.

- ROCKET FALSIFICATION: not run this pass. UNKNOWN TOOL PROOF: not run.
- WEAPON ENUM TRACE: not run this pass (still classified as a closed-world
  candidate). NPC GOAL TRACE / UNKNOWN NPC BEHAVIOR PROOF: not run.
- CLOSED-WORLD AUDIT RESULT: unchanged from Round 59 (weapon/NPC cold enums
  unresolved; not rewritten because no falsification yet proves they block).
- CHANGESET / WORLD-MAP / PERSISTENCE / PEER AUTHORING: not started.

- COLD-REBUILD MATRIX (this round): PNG CHANGE **NO**, GLB CHANGE **NO**, WAV
  CHANGE **NO** at the primitive level (no rebuild, no restart).
- DID ANY TEST REQUIRE REBUILDING/KILLING mimita.exe? **No** (in-process; build
  only, no restart).
- NEXT LARGEST REAL COLD BOUNDARY: wiring resources into real in-world consumers
  (menu logo + Tool mesh) and the descriptor wire; then the cold weapon/NPC enums
  if the unknown-tool falsification shows they still gate new tools.

- LIVE-PROOF DEBT: descriptor over the socket, in-world PNG/GLB/WAV swap, rocket
  multi-axis falsification, unknown-tool proof, NPC goal falsification, ChangeSet,
  world edits, persistence, peer authoring.

## Files changed
- `src/hot-reload/content-artifact.h`: descriptor, version state, validators,
  `ResourceRegistry`.
- `src/hot-reload/content-resource-selftest.{h,cpp}`: selftest suite.
- `src/game/game-cli.cpp`: `--content-resource-selftest`.
- docs.

## Next (auto-selected)
Carry the content descriptor over the real socket reusing the artifact request/
chunk path; publish a real in-world GLB swap on the existing Tool Entity (same
EntityId/equip); then the rocket multi-axis + runtime-unknown-tool falsifications;
then classify the cold weapon/NPC enums with real data flow.
