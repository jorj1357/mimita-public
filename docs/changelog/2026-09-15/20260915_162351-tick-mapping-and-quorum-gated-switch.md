# Tick-domain mapping + quorum-gated switch (compiled integration)

- EST timestamp: 2026-09-15 16:23:51 EDT (UTC 2026-09-15T20:23:51Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (cold build + full suite 34/34). LIVE PROOF: no.

## Report
- SUBSYSTEM: generation switch safety in authoritative tick space.
- TICK DOMAIN TABLE:
  | Name | Owner | Domain | Source | Used for | Same as server tick? |
  |---|---|---|---|---|---|
  | `tick` | server loop (`server.cpp`) | server authoritative fixed step | accumulated `SERVER_DT` | snapshots (`header.tick`), switch scheduling, rewind history, movement | baseline |
  | `header.tick` on packets | server/client | server tick at send | copied from `tick` | snapshot/announce timing | yes (server domain) |
  | `clientSimulationTick` | `MultiplayerContext` (client) | client-local prediction | client fixed step | client prediction + client `CodeGenerationPacket.header.tick` | **not proven equal** |
  | `latestLocalSnapshotTick` | client | server tick observed | snapshot header | interpolation/ack | yes (server domain) |
  | render/interpolation tick | client | smooth render clock | `globalRenderTick` | remote interpolation | no (render domain) |
- ARE SERVER/CLIENT TICKS IDENTICAL? Outcome **C — no reliable mapping existed**
  (no explicit offset in code; counters are separate). The switch was compared
  against `clientSimulationTick` directly (unsafe).
- EXPLICIT MAPPING IMPLEMENTED: yes —
  `MimitaRuntime::mapServerSwitchTickToClientLocal(latestServerTick,
  localSimTickAtReceipt, serverSwitchTick)` (delta-based). Wired in
  `multiplayer-tick.cpp`; the client schedules activation at the mapped local
  boundary and refuses a SWITCH for an unvalidated generation.
- GENERATION SWITCH CONTRACT: `SWITCH(G, T)` — G is authoritative beginning at
  **server** simulation tick T; T is always server-tick space; no wall clock.
- QUORUM-GATED SWITCH STATUS: **IMPLEMENTED (compiled)** — server announces a
  candidate (phase 0) and holds its own activation (far-future switch tick), then
  schedules `SWITCH(G, tick+30)` only when
  `GenerationDistribution::quorumReady(required, G)`.
- SERVER READY STATUS: server counts itself by holding activation until quorum;
  explicit self-ready flag not added.
- CLIENT READY STATUS: client sends READY after acquire/verify (existing).
- REQUIRED PEER SET: all `players` (v1: server + connected non-spectator clients);
  disconnect updates quorum deterministically (`removePeer`).
- PRE-T / AT-T / POST-T BEHAVIOR: pre-T F (server holds, client holds); at T both
  activate via safe-tick swap; last-good preserved.
- PREDICTION BOUNDARY HANDLING: reconciliation receives real predicted vs
  authoritative ids; a boundary forces the hot mismatch path (rebase/reset) —
  client prediction is not yet cleared explicitly at T.
- INTERPOLATION GENERATION PROVENANCE: **placeholder 0** (needs per-sample field).
- REWIND GENERATION PROVENANCE: **placeholder 0** (needs per-sample field).
- VERIFY GATE: hash yes; ABI/ capabilities/ schema/ dependencies **missing**
  peer-side.
- LATE JOIN: **MISSING**.
- SUPERSEDE / SCHEDULED TRANSACTION RULE: documented preferred rule = once a
  SWITCH tick is committed, do NOT supersede mid-transaction (let G switch, then
  H). Not yet enforced in the server block.
- FAILURE CASES: client never READY → no quorum → no SWITCH (server holds);
  bad hash → no READY; peer disconnect → quorum recomputed; stale READY(G) after
  H → ignored (setPhase checks candidate id); SWITCH for unvalidated generation →
  client ignores.
- ACTUAL SOCKET INTEGRATION TEST: **NO** — compile + unit only.
- REAL TWO-PROCESS PROOF: **NO**. LIVE RAW-CPP EDIT PROOF: **NO**.
- DISTRIBUTED GENERATION COMPLETE? **NO**.
- LIVE-PROOF DEBT: the whole distributed switch is compiled-integration only;
  no packet has been observed over a live socket.
- NEXT LARGEST COLD OWNER: socket-level integration test + per-sample generation
  provenance, then the two-process proof; then resource/asset distribution.

## Notes
- No new protocol phases were added; the mapping is a pure helper and the quorum
  gate reuses existing announce/READY/switch machinery.
- Concurrency: a concurrent agent's `modules/ui/actor-overlays.cpp` transiently
  broke the DLL build (stray backtick); the kernel build of this change was fine
  and the retry then succeeded.

## Files changed
`src/hot-reload/generation-switch-mapping.h` (new),
`src/hot-reload/generation-switch-mapping-selftest.{h,cpp}` (new),
`src/network/server.cpp` (quorum-gated two-phase announce/hold),
`src/network/multiplayer-tick.cpp` (mapped switch boundary + unvalidated reject),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Socket-level integration test (serialize→send→receive→decode→dispatch), then
per-sample generation provenance (interpolation/rewind), then the two-process
switch proof.
