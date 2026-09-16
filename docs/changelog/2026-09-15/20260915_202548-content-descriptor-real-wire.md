# Content descriptor on the real wire (reusing the artifact path)

- EST timestamp: 2026-09-15 20:25:48 EDT (UTC 2026-09-16T00:25:48Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--transport-generation-selftest` content section + full subset 13/13)

## Report
- SUBSYSTEM: generic content artifact — descriptor wire + client routing.

- CONTENT DESCRIPTOR WIRE
  - real socket? YES — `ContentArtifactPacket` (type 82) encoded/sent/received/
    decoded over a real OS loopback UDP socket.
  - cache miss? YES — bytes transferred through the EXISTING
    ArtifactBegin/Chunk path, hash-verified, GLB-validated, then published
    (`resolve == hashA`).
  - cache hit? YES — descriptor announces with bytes cached and publishes with
    zero chunk bytes.
  - stale safety? YES — C announced after B; late B publish rejected; C publishes.
  - identity binding: descriptor binds logical id + kind + hash + size; a publish
    whose hash is not the advertised candidate is rejected ("not the advertised
    candidate").

- REAL GLB CONSUMER: **not done this pass** — no in-world Tool Entity swap yet.
  Logical id `mesh.tool.rocket`, old/new hashes, same Tool EntityId, same equip
  relationship, renderer re-resolution, malformed last-good: NOT PROVEN in-world.
- REAL PNG CONSUMER: **not done this pass** (no live UI path).
- REAL WAV CONSUMER: **not done this pass** (no live audio consumer).

- RESOURCE / CODE GENERATION SAFETY: the registry is a cold singleton keyed by
  logical id + content hash; it holds NO pointer into a hot DLL, so a code
  generation swap cannot invalidate it. Not exercised against an in-world consumer
  yet.
- RESOURCE LATE JOIN: not implemented (no current-mapping manifest/snapshot);
  unresolved-resource fallback: not implemented.

- CONTENT PIPELINE COMPLETE ENOUGH? **No** — one descriptor model, one artifact
  path, real-socket cache miss/hit, and supersede safety are proven; real in-world
  consumers, renderer re-resolution, late-join mapping, and fallback remain.

- ROCKET MULTI-AXIS FALSIFICATION: not run. WEAPON ENUM CLASSIFICATION: not run
  (still closed-world candidate). UNKNOWN TOOL PROOF: not run. NPC GOAL
  CLASSIFICATION / UNKNOWN NPC BEHAVIOR: not run.
- CHANGESET STATUS / PEER AUTHORING STATUS / WORLD-MAP EDIT STATUS /
  PERSISTENCE STATUS: not started.

- COLD-REBUILD MATRIX (this round): PNG/GLB/WAV CHANGE **NO** at the wire/registry
  level.
- DID ANY TEST REQUIRE REBUILDING/KILLING mimita.exe? **No** (build only; one
  transient DLL-lock PermissionError during a concurrent build, retried).
- NEXT LARGEST REAL COLD BOUNDARY: real in-world resource consumers (renderer
  per-frame logical re-resolution; audio next-playback resolution; UI image
  resolution) — i.e. proving no consumer caches a raw handle across publications.

- LIVE-PROOF DEBT: in-world PNG/GLB/WAV swap, renderer re-resolution, late-join
  resource mapping, fallback, rocket/unknown-tool falsifications, weapon/NPC enum
  classification, ChangeSet, world edits, persistence, peer authoring.

## Files changed
- `src/network/packets.h`: `PACKET_CONTENT_ARTIFACT`, `ContentArtifactPacket`.
- `src/hot-reload/content-artifact.h`: `pendingLogicalIdForHash`, hash-keyed
  pending map.
- `src/network/multiplayer-tick.cpp`: descriptor handler + content-vs-code routing
  on artifact completion.
- `src/network/transport-generation-selftest.cpp`: real-socket content wire tests.
- docs.

## Next (auto-selected)
Route an existing Tool Entity's mesh through `ResourceRegistry::resolve` with
per-frame re-resolution, then publish a real GLB swap over the socket and assert
the same Tool EntityId/equip; then PNG (UI) and WAV (audio) consumers; then the
rocket multi-axis and unknown-tool falsifications.
