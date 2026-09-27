# NPC rewind provenance + generation-provenance complete-enough

- EST timestamp: 2026-09-15 17:05:09 EDT (UTC 2026-09-15T21:05:09Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build + full suite 34/34)

## Report
- SUBSYSTEM: generation provenance (NPC rewind completion + declaration).
- NPC REWIND PROVENANCE: **done** — `ServerNpcPositionSample.logicalGenerationId`
  stamped at insertion in `pushNpcPositionHistory` from the active generation;
  both NPC historical-pose lookups clamp to the newer sample across an F/G
  boundary (identical semantics to player rewind).
- PLAYER/NPC CROSS-GENERATION BEHAVIOR: both clamp to the newer authoritative
  sample (no cross-generation lerp). `net.rewind` receives real ids.
- REMAINING PLACEHOLDER ZEROS: none in the player/NPC/snapshot/interpolation
  rewind or provenance paths. (Generation 0 can still occur for a not-yet-hot
  process, which is an explicit base sentinel.)
- GENERATION PROVENANCE COMPLETE? **Complete enough** — authoritative snapshots,
  snapshot wire, interpolation samples, player rewind history, NPC rewind history,
  and rewind-policy inputs carry the canonical logical generation id; cross-
  generation interpolation/rewind clamp/snap. Prediction has **no** stored
  per-tick history by design (reconcile snaps to the stamped authoritative
  snapshot); documented as intentional, not invented.
- ACTUAL TRANSPORT INTEGRATION: **NOT DONE** — encode/send/receive/decode for
  ANNOUNCE/REQUEST/BEGIN/CHUNK/READY/SWITCH and snapshot-generation-over-socket
  are compiled/unit-proven but not exercised over a real socket in this pass.
- TICK-MAPPING / QUORUM TRANSPORT TEST: unit-proven only.
- VERIFY GATE: hash yes; ABI/capabilities/schema/dependencies/migration missing.
- LATE JOIN / CACHE-HIT LATE JOIN: missing.
- PRE-COMMIT SUPERSEDE / POST-COMMIT CANDIDATE: rule-level only.
- FAILURE INTEGRATION: unit-level only.
- REAL TWO-PROCESS / MULTI-FILE HOT REFACTOR / BAD-EDIT LAST-GOOD: **NO** (live).
- DISTRIBUTED CODE GENERATION COMPLETE? **NO** — the remaining items are the
  transport integration test, verify gate, late join, and live proof.
- LIVE-PROOF DEBT: unchanged.
- NEXT LARGEST COLD OWNER: actual transport-level end-to-end generation test,
  then verify gate + late join + two-process proof; then distributed assets.

## Honesty
- This pass finished the last provenance gap (NPC rewind) and declares
  provenance complete-enough. It did **not** build the transport integration test
  or verify gate; those are the next concrete step, and no live socket transaction
  has been observed.

## Files changed
`src/network/server.h` (`ServerNpcPositionSample.logicalGenerationId`),
`src/network/server-npcs.cpp` (stamp + cross-generation clamp);
docs + this changelog.

## Next (auto-selected)
Transport-level integration test (encode→send→receive→decode→dispatch) for the
full generation transaction + snapshot generation; then ABI/capability/schema/
dependency verify gate; then late join; then the two-process proof.
