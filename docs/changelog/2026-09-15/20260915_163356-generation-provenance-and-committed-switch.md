# Generation provenance scaffolding + committed-switch supersede rule (partial)

- EST timestamp: 2026-09-15 16:33:56 EDT (UTC 2026-09-15T20:33:56Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build + full suite 34/34). LIVE PROOF: no.

## Report
- SUBSYSTEM: generation provenance across the switch + committed-switch rule.
- CANONICAL GENERATION PROVENANCE: **one id source** — `SnapshotTransform`
  gained `logicalGenerationId`; the interpolation policy is fed sample A/B/current
  generation from the samples (no subsystem-specific counters introduced).
- SNAPSHOT GENERATION TAGGING: **NOT POPULATED** — no per-snapshot wire
  `generation` field yet, so samples default to 0 and receive-time guessing was
  deliberately avoided (would mis-tag F snapshots as the announced candidate).
  This is the concrete next step.
- PREDICTION GENERATION TAGGING: partial — reconciliation already receives real
  `predictedGeneration` (local active) vs `authoritativeGeneration`
  (`ctx.serverCodeGeneration`); per-predicted-state provenance not stored.
- INTERPOLATION GENERATION TAGGING: **wired, not populated** — `net.interpolate`
  now receives `aGeneration`/`bGeneration`/`currentGeneration` from the samples;
  with real stamped samples a cross-boundary pair will snap instead of lerp.
- REWIND GENERATION TAGGING: **placeholder 0** (history sample struct not tagged).
- F→G BOUNDARY BEHAVIOR: policy is snap/reset on mismatch (existing, unit-proven);
  conservative v1.
- COMMITTED SWITCH SUPERSEDE RULE: **ENFORCED** — pre-commit a newer candidate may
  replace the announced one and cancel/ignore its acquisition; once a SWITCH is
  committed, H cannot cancel G (G activates, then H is the next candidate).
  Flags reset on activation.
- PRE-COMMIT SUPERSEDE RULE: enforced (H replaces G while holding for quorum).
- VERIFY GATE: hash yes; ABI/capabilities/schema/dependencies/migration
  **missing**.
- ACTUAL SOCKET INTEGRATION TEST: **NO** (real encode/send/receive/decode not
  exercised; artifact bytes/READY/SWITCH only unit-tested).
- QUORUM TEST: bookkeeping unit-tested (no-late-switch-before-quorum); no socket
  integration test.
- TICK-MAPPING FALSIFICATION TEST: **PASS** — `--generation-switch-mapping-selftest`
  proves server tick 1000 / client local 1004 / SWITCH T=1030 maps to client 1034
  (not blindly 1030), falsifying the old bug.
- LATE JOIN / CACHE-HIT LATE JOIN: **MISSING**.
- FAILURE INTEGRATION TESTS: partial (unit only) — bad hash/missing chunk/stale
  READY/unvalidated SWITCH covered at unit level in the artifact/generation
  selftests; not over a socket.
- REAL TWO-PROCESS PROOF / LIVE RAW-CPP EDIT PROOF / MULTI-FILE REFACTOR PROOF /
  BAD-EDIT LAST-GOOD PROOF: **NO** (live).
- DISTRIBUTED GENERATION COMPLETE? **NO**.
- LIVE-PROOF DEBT: entire distributed switch remains compiled-integration only.
- NEXT LARGEST COLD OWNER: per-snapshot generation field (stamp snapshots at
  source), rewind/predicted provenance, then the socket integration test and
  two-process proof; then distributed resource/asset generations.

## Honesty
- The provenance **field + policy plumbing** is in place, but samples are not yet
  stamped from the authoritative active generation, so cross-generation detection
  is not yet real end to end. I did not add a speculative wire field without a
  verified send/receive path; that is the next concrete step.
- No new protocol phases were added; the committed-switch rule reuses existing
  flags.

## Files changed
`src/network/multiplayer-context.h` (`SnapshotTransform.logicalGenerationId`),
`src/network/multiplayer-interpolation.cpp` (feed a/b/current generation),
`src/network/server.cpp` (committed-switch supersede enforcement);
docs + this changelog.

## Next (auto-selected)
Add the per-snapshot generation field and stamp samples on receipt; tag rewind
history + predicted state; then the socket integration test (encode/send/receive/
decode) and the two-process proof.
