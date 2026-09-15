# Prediction + rewind generation provenance (implemented)

- EST timestamp: 2026-09-15 16:59:26 EDT (UTC 2026-09-15T20:59:26Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build; suite 34/34 minus one unrelated concurrent UI failure)

## Report
- SUBSYSTEM: generation provenance for prediction + rewind.
- PREDICTION HISTORY PROVENANCE:
  - storage field: **none exists** — the client does not keep a per-tick
    predicted-state history (reconciliation snaps to the authoritative snapshot
    rather than replaying inputs). No parallel debug history was invented.
  - source stamping: n/a (no stored predicted state to stamp).
  - reconciliation consumption: uses the authoritative snapshot's stamped
    generation vs the local active generation.
  - F/G reset behavior: authoritative G vs predicted-active F triggers the hot
    mismatch path (conservative rebase/snap).
- REWIND HISTORY PROVENANCE:
  - player: **tagged** — `PositionHistoryEntry.logicalGenerationId`, stamped at
    insertion in `pushPositionHistory` from the active generation.
  - NPC: **not tagged** (`getNpcPoseAtTick` storage) — remaining.
  - cross-generation query behavior: `getPositionAtTick`/`getPlayerPoseAtTick`
    clamp to the newer sample when bracketing samples straddle an F/G boundary
    (no cross-generation lerp). `net.rewind` receives real attacker/target/current
    generation ids instead of zeros.
  - placeholder zeros remaining: NPC rewind history; no player-path zero remains.
- PROVENANCE COMPLETE? **Mostly** — snapshot + interpolation + player rewind are
  real; prediction has no stored history; NPC rewind remains.
- ACTUAL SOCKET INTEGRATION: **NO** (encode/send/receive/decode not exercised;
  artifact bytes/READY/SWITCH/snapshot generation are unit/compiled only).
- QUORUM INTEGRATION / TICK-MAPPING INTEGRATION: unit-proven only.
- VERIFY GATE: hash yes; ABI/capabilities/schema/dependencies/migration missing.
- LATE JOIN / CACHE-HIT LATE JOIN: missing.
- SUPERSEDE INTEGRATION / FAILURE INTEGRATION: unit/rule-level only.
- REAL TWO-PROCESS / MULTI-FILE RAW CPP / BAD-EDIT LAST-GOOD: **NO** (live).
- DISTRIBUTED CODE GENERATION COMPLETE? **NO**.
- LIVE-PROOF DEBT: unchanged — compiled/unit-proven, no live socket transaction.
- NEXT LARGEST COLD OWNER: NPC rewind provenance, then the socket integration
  test; then distributed resource/asset generations.

## Concurrent (unrelated)
`--hot-combat-selftest` fails "hot ui.frame fails safe without match HUD state"
(UI/HUD agent). Recorded; not touched.

## Files changed
`src/network/server.h` (`PositionHistoryEntry.logicalGenerationId`),
`src/network/server-players.cpp` (stamp history, cross-generation clamp in
`getPositionAtTick`/`getPlayerPoseAtTick`, real ids into `net.rewind`);
docs + this changelog.

## Next (auto-selected)
NPC rewind-history provenance, then the socket-level integration test
(encode/send/receive/decode) and two-process proof; then distributed assets.
