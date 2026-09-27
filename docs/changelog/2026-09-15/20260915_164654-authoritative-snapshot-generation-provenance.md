# Authoritative snapshot generation provenance (implemented)

- EST timestamp: 2026-09-15 16:46:54 EDT (UTC 2026-09-15T20:46:54Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build; suite 34/34 minus one unrelated concurrent UI failure)

## Report
- SUBSYSTEM: generation provenance end-to-end (snapshot wire + interpolation).
- AUTHORITATIVE SNAPSHOT STAMPING: **implemented** — the server stamps
  `SnapshotChunkPacket.logicalGenerationId` from
  `HotReloadSystem::status().activeGeneration`, which is the generation that
  actually executed the tick (activation is at the safe boundary before
  simulation), so T<switch → F and T>=switch → G. Not the announced candidate,
  not receive-time inference.
- SNAPSHOT WIRE GENERATION FIELD: **added** — `uint32_t logicalGenerationId` in
  `SnapshotChunkPacket` (wire size 1124→1128; static_assert updated);
  `buildSnapshotChunks` takes it; `reassembleSnapshotChunks` verifies chunk
  agreement (`mixed-generations` rejected) and returns it.
- CLIENT SAMPLE POPULATION: **implemented** — the decoded generation is threaded
  through `processSnapshotEntities` → `pushInterpolationTarget`, stamping
  `SnapshotTransform.logicalGenerationId`.
- PREDICTION PROVENANCE: partial (reconciliation uses local active vs
  authoritative); per-predicted-state generation not stored.
- INTERPOLATION PROVENANCE: **real** — `net.interpolate` receives
  a/b/current generation from same-generation samples; a cross-boundary pair
  snaps/resets via the existing hot policy. No placeholder 0 remains in this path.
- REWIND PROVENANCE: **placeholder 0** (history sample struct not tagged).
- F→G BOUNDARY TEST: policy-level test exists (`--interpolation-policy-selftest`
  generation-boundary/mismatch → snap); the wire round-trip of the generation is
  now covered by the snapshot-chunk path (not yet a dedicated end-to-end test).
- PLACEHOLDER ZERO AUDIT: interpolation path cleared (real ids); rewind history
  and predicted-state-per-sample remain 0 (documented, not silent).
- ACTUAL SOCKET INTEGRATION TEST: **NO**.
- SNAPSHOT PROVENANCE OVER SOCKET: **NO** (not yet a live test).
- VERIFY GATE: hash yes; ABI/capabilities/schema/dependencies/migration missing.
- LATE JOIN / CACHE-HIT LATE JOIN: **MISSING**.
- COMMITTED SWITCH + H TEST: rule enforced; not integration-tested.
- FAILURE INTEGRATION TESTS: unit-level only.
- REAL TWO-PROCESS / MULTI-FILE RAW CPP / BAD-EDIT LAST-GOOD PROOF: **NO** (live).
- DISTRIBUTED GENERATION COMPLETE? **NO**.
- LIVE-PROOF DEBT: the artifact transfer + switch + snapshot provenance are
  compiled/unit-proven but no live socket transaction was observed.
- NEXT LARGEST COLD OWNER: rewind-history + predicted-state provenance, then the
  socket integration test and two-process proof; then distributed assets.

## Concurrent (unrelated)
`--hot-combat-selftest` fails "hot ui.frame fails safe without match HUD state"
(UI/HUD agent's active work). Recorded; not touched per the concurrency rule.

## Files changed
`src/network/packets.h` (snapshot chunk generation field),
`src/network/snapshot-chunks.{h,cpp}` (build/reassemble generation),
`src/network/server-packets.cpp` (stamp active generation),
`src/network/multiplayer-tick.cpp` + `multiplayer-context.h` +
`multiplayer-interpolation.cpp` (thread + stamp samples);
docs + this changelog.

## Next (auto-selected)
Tag rewind history and predicted-state samples with the canonical generation;
then the socket-level integration test (encode/send/receive/decode) and the
two-process proof; then distributed resource/asset generations.
