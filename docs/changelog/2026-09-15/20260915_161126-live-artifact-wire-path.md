# Live artifact wire path (compiled integration)

- EST timestamp: 2026-09-15 16:11:26 EDT (UTC 2026-09-15T20:11:26Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build + full suite 33/33). LIVE PROOF: no.

## Report
- SUBSYSTEM: distributed hot-generation artifact transfer (live path).
- LIVE REQUEST HANDLER STATUS: **IMPLEMENTED (compiled)** — `server-packets.cpp`
  handles `PACKET_ARTIFACT_REQUEST`: reads the ready candidate artifact via
  `HotReloadSystem::readCandidateArtifact`, matches the requested
  `platformArtifactHash`, and streams BEGIN + bounded CHUNKs. Unknown/mismatched
  hashes are ignored (no filesystem path from the client).
- LIVE BEGIN/CHUNK HANDLER STATUS: **IMPLEMENTED (compiled)** — client
  `multiplayer-tick.cpp` handles `PACKET_ARTIFACT_BEGIN`/`CHUNK` via
  `ArtifactReceiver` (duplicate/out-of-order-safe), and on completion commits
  (verify + immutable cache) then sends `READY(G)`.
- ACTUAL NETWORK PACKET PATH PROVEN? **NO** — compiled/unit-proven; not exercised
  over a live socket pair in this environment.
- CACHE HIT PATH: **IMPLEMENTED** — client checks `ArtifactCache::contains(hash)`
  and skips the request when present (no transfer; still verifies).
- TRANSFER CANCELLATION: **PARTIAL** — superseded announce resets the requested
  hash; explicit cancel of an in-flight G when H supersedes is not yet wired.
- SUPERSEDE HANDLING: bookkeeping exists (`GenerationDistribution::supersedes`),
  but transfer/READY cancellation for a superseded generation is not wired.
- VERIFY GATE:
  - hash: **yes** (receiver/acquirer re-hash; immutable store).
  - ABI: **partial** — `GenerationIdentityV1.abiVersion` advertised; no peer gate.
  - capabilities/schema/dependencies: **missing** peer-side gate.
  - load validation: local `HotReloadSystem` only; not invoked on the acquired
    artifact in the client path yet.
- REAL CLIENT READY PATH: **IMPLEMENTED (compiled)** — after a successful
  receive/verify the client sends `CodeGenerationPacket` phase=1 with the exact
  logical generation (plus the existing periodic phase=1/liveStatus.loaded report).
- REAL SERVER READY PATH: **IMPLEMENTED (compiled)** — server records READY into
  `GenerationDistribution` only when the peer's announced candidate generation
  matches (`READY(G)` cannot satisfy H).
- QUORUM STATUS: `quorumReady` + required-set logic exist; the live scheduling
  path still uses the existing `switchTick` announce rather than gating the switch
  on `quorumReady` (not yet wired to block the switch).
- TICK DOMAIN AUDIT: **UNRESOLVED / RISK** — the server schedules
  `switchTick = serverTick + 30` in SERVER tick space; the client calls
  `requestSwitchAtTick(switchTick)` and `pollAndAdvance(localTick)` compares
  against its LOCAL `clientSimulationTick`. It is **not proven** that
  `clientSimulationTick == serverTick`. This must be mapped explicitly before a
  live switch is safe.
- AUTHORITATIVE SWITCH MAPPING: **MISSING** (depends on the tick-domain audit).
- ATOMIC ACTIVATION STATUS: existing safe-tick activation in `HotReloadSystem`
  (F until `switchAtTick`, then G); last-good rollback preserved.
- GENERATION PROVENANCE:
  - reconciliation: real (predicted=local active, authoritative=server).
  - interpolation: **placeholder 0** (needs per-sample field).
  - rewind: **placeholder 0** (needs per-sample field).
- LATE JOIN: **MISSING**.
- FAILURE PATHS: unknown artifact request → ignored safely (session stays alive);
  transfer interrupted/missing chunk → never valid (unit-proven); bad hash → no
  READY (unit-proven); bad source build → no candidate (existing last-good).
- NETWORK INTEGRATION TEST: **partial** — the artifact-transfer selftest exercises
  the real packet structs end-to-end in-process; no socket-level integration test.
- REAL SERVER+CLIENT PROOF: **NO**. LIVE RAW-CPP EDIT PROOF: **NO**. BAD-EDIT
  LAST-GOOD PROOF: partial (local only). SAME PID/SESSION/ENTITY-ID PROOF: **NO**.
- DISTRIBUTED GENERATION MILESTONE COMPLETE? **NO**.
- REMAINING BLOCKERS:
  1. Explicit authoritative tick-domain mapping (server tick ↔ client tick) and
     switch gating on it.
  2. Gate the switch on `quorumReady` (server scheduling).
  3. Peer ABI/capability/schema/dependency verify gate.
  4. Per-sample generation provenance (interpolation/rewind).
  5. Late join + superseded-transfer cancellation.
  6. The live two-process proof.
- NEXT COLD OWNER: tick-domain mapping + quorum-gated switch, then the
  two-process proof.

## Notes
- This pass did exactly the requested wiring (no new protocol abstraction). The
  server stream, client acquisition/READY, and server READY bookkeeping are now
  in the real packet paths and compile into the shipping exe.
- Honesty: "IMPLEMENTED (compiled)" means the code path exists and builds/passes
  the suite; it does NOT mean a packet was observed over a live socket. The tick
  domain is flagged as unsafe until proven.

## Files changed
`src/hot-reload/hot-reload-system.{h,cpp}` (candidate artifact accessor),
`src/network/server.cpp` (real artifact hash + per-peer announce),
`src/network/server-packets.cpp` (artifact request stream + READY bookkeeping),
`src/network/multiplayer-tick.cpp` (client request/BEGIN/CHUNK/READY);
docs + this changelog.

## Next (auto-selected)
Explicit server↔client tick mapping + quorum-gated switch; then the live
two-process proof; then per-sample generation provenance.
