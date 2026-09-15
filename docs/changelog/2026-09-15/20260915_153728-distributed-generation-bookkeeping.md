# Distributed hot-generation bookkeeping (partial)

- EST timestamp: 2026-09-15 15:37:28 EDT (UTC 2026-09-15T19:37:28Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--generation-distribution-selftest` 10/10; full suite 31/31)

## Report
- SUBSYSTEM: distributed hot-generation delivery (bookkeeping/handshake).
- EXISTING DISTRIBUTED HOT-RELOAD AUDIT:
  | Stage | Owner | Data | Local/Networked | Missing |
  |---|---|---|---|---|
  | source change detect | `HotReloadSystem`/`ProjectWatcher` | file hashes | local | — |
  | build candidate | `HotReloadSystem` worker | sourceHash/codeHash | local | — |
  | validate | `HotReloadSystem` API/ABI + selftests | package descriptor | local | — |
  | activate/rollback | `HotReloadSystem::pollAndAdvance` safe tick | generation | local | — |
  | announce | `server.cpp` `CodeGenerationPacket` | generation/phase/switchTick/logicalCodeHash/platformPackageHash | networked | artifact manifest |
  | peer acquire | none | — | — | **missing** |
  | peer verify | partial (load locally) | codeHash | local | hash/ABI/schema gate |
  | READY/quorum | none | — | — | **added this pass** |
  | shared switch tick | `requestSwitchAtTick` + `CodeGenerationPacket` | switchTick | networked | tick-mapping audit |
  | retire old | `HotReloadSystem` | generation record | local | — |
- LOGICAL GENERATION IDENTITY: `logicalCodeHash` (platform-independent) exists in
  `CodeGenerationPacket`; `GenerationIdentityV1.logicalGenerationId` /
  `logicalBehaviorHash` added for host bookkeeping.
- PLATFORM ARTIFACT IDENTITY: `platformPackageHash` exists in the packet;
  `GenerationIdentityV1.platformArtifactHash` added. Not conflated with logical.
- GENERATION MANIFEST: minimal per-peer identity added; a full multi-platform
  artifact-descriptor manifest is not built yet.
- ANNOUNCE FLOW: exists (`server.cpp`) — host builds+validates a candidate, then
  announces SWITCH at a tick.
- ACQUIRE FLOW: **not implemented** (no network artifact transfer / content cache).
- VERIFY FLOW: local load validation exists; peer-side hash/ABI/schema gate
  against an acquired artifact is not implemented.
- READY FLOW: **added** — `GenerationDistribution` tracks per-peer phases keyed by
  logical generation; `quorumReady` gates the switch; Active counts as Ready.
- SHARED SWITCH-TICK FLOW: host-only scheduling added (`scheduleSwitch`), with
  supersede-on-newer and cancel; the wire announce already carries `switchTick`.
- TICK-MAPPING MODEL: existing path activates when the local fixed tick reaches
  `switchTick`. **Not audited/proven** that server and client tick domains are
  identical — flagged as required follow-up before live proof.
- STATE MIGRATION MODEL: existing schema version/migration machinery in the dynamic
  component store; manifest does not yet expose migration requirements for
  pre-READY validation (follow-up).
- PREDICTION GENERATION HANDLING: **wired** — reconciliation now receives real
  `predictedGeneration` (local active) vs `authoritativeGeneration`
  (`ctx.serverCodeGeneration`), so a real cross-generation boundary is detected.
- INTERPOLATION GENERATION HANDLING: policy supports it; ids still 0 (needs
  per-sample generation storage — follow-up).
- REWIND GENERATION HANDLING: policy supports a conservative reject; ids still 0
  (needs per-sample generation storage — follow-up).
- LATE-JOIN HANDLING: not implemented (must learn active generation + acquire).
- MULTIPLE-CANDIDATE HANDLING: `supersedes()` (newer generation supersedes the
  not-yet-active scheduled one; older candidates rejected).
- FAILURE / ROLLBACK BEHAVIOR: last-good preserved structurally by
  `HotReloadSystem` (bad build never becomes active); peer VERIFY failure paths
  depend on the not-yet-built ACQUIRE/VERIFY flow.
- SELFTEST PROVEN: announce/phase, quorum (incl. Active-counts-as-ready and
  different-candidate breaks), schedule switch, supersede, peer removal, cancel,
  determinism.
- REAL SERVER/CLIENT PROOF: **no** — no live two-process run performed. This is
  compiled integration + unit proof only; labelled honestly.
- LIVE HOT-EDIT PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected (new files only; reconcile touched
  additively; movement/presentation untouched).
- WOULD THIS STILL REQUIRE A COLD RESTART? For the generation-switch protocol
  itself: no (it is hot-reload infrastructure). Artifact acquisition/loader
  mechanism and any cold-source change: yes.
- DISTRIBUTED GENERATION MILESTONE COMPLETE? **No** — logical/platform identity,
  announce, READY/quorum, and switch scheduling exist; **artifact acquisition,
  peer verify, per-sample generation tagging, late-join, and the live
  server/client proof are missing.**
- NEXT COLD OWNER: implement ACQUIRE/VERIFY (content-addressed artifact cache +
  transfer) and wire READY into the real client path; audit the tick mapping;
  then run the two-process proof. Optional later: generic
  `historicalState(EntityId,T)` + explosion rewind.

## Files changed
`src/hot-reload/generation-distribution.{h,cpp}` (new),
`src/hot-reload/generation-distribution-selftest.{h,cpp}` (new),
`src/network/multiplayer-reconcile.cpp` (real generation ids),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Artifact acquisition (content-addressed; immutable candidates; verify before
READY) + READY wire-up; tick-mapping audit; then the real server+client switch
proof. Do NOT scope-shift into generic historical query/explosion rewind.
