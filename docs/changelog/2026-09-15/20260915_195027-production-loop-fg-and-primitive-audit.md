# Production-loop F->G proof + primitive cold-boundary audit (first pass)

- EST timestamp: 2026-09-15 19:50:27 EDT (UTC 2026-09-15T23:50:27Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--production-loop-selftest` PASS; targeted suite 18/19; the one failure is concurrent GUI work)

## Report
- SUBSYSTEM: distributed hot-code — production-loop proof + primitive audit.

- FULL PRODUCTION CODE LOOP
  - passed? YES in-process: real `HotReloadSystem::startup` (F active, real hot
    package), real artifact bytes (`build/mimita-game.dll`), real manifest +
    `verifyGeneration`, real `prepareMigration`, real `installCandidateArtifact`,
    real `requestSwitchAtTick` + `tryActivateCandidate`. F=1 -> G=2.
  - real loader? YES (same `loadCandidateFromFile` path).
  - real switch? YES (coordinated switch branch, `lastSwitchRejection == None`).
  - real snapshot? Snapshot generation provenance proven earlier; not re-run here.
  - Caveat: no two-process interactive run; process/session/connection ids not
    captured live.

- SESSION CONTINUITY: persistent dynamic component on its EntityId survived the
  F->G change (F=1 -> G=2). Full process/connection/match capture remains debt.
- RAW CPP PROOF: none this pass (no live two-process run).

- PRIMITIVE AUDIT (first pass, ranked)
  - GENERATION: generic (identity, requirements, candidate, verification,
    migration readiness, publish, last-good). No weapon/UI/mode assumptions found.
  - ARTIFACT: `ArtifactCache`/transport carry opaque immutable hash-addressed
    bytes; no code-specific transport assumptions observed — kind-agnostic, ready
    for PNG/GLB/WAV.
  - RESOURCE: no `LogicalResourceId -> content artifact` primitive exists yet
    (ContentArtifactV1 is next). Code/artifact identity exists; generic logical
    resource identity does not.
  - ENTITY/COMPONENT: dynamic components support runtime-new schemas, versioning,
    migration, and replication (proven in earlier rounds); no cold EntityType enum
    needed.
  - RELATIONSHIP: generic relationship primitive exists (earlier rounds).
  - CAPABILITIES: 20, mechanism-shaped. Feature leaks NOT found
    (no `rocket.fire`/`bomb.defuse`/`zombie.spawn`/`tdm.score`).
  - CLOSED ENUM / SWITCH AUDIT (cold paths, excluding hot modules):
    - `combat/weapon-types.h` (`WeaponBehaviorType`, `WeaponExecutionType`,
      `WeaponFireMode`) used by cold `weapon-system.cpp` (27),
      `server-physical-contact.cpp` (12), `server-attack.cpp` (11),
      `weapon-data.cpp` (10), `npc-combat.cpp` (9), `npc.cpp` (5).
    - `npc/npc-goal.h` (`NpcGoalKind`) used by cold `npc-navigator.cpp` (11),
      `npc.cpp` (8).
    - `combat/projectile-simulation.h` (`ProjectileCollisionType`).
    Classified as CLOSED-WORLD CANDIDATES pending a trace of whether the cold
    paths are still authoritative or legacy projection. This is the largest likely
    cold owner.
  - COLD-BOUNDARY FAILURES FOUND: weapon/NPC enum branches in cold code (above);
    no resource/logical-id primitive yet.

- ContentArtifactV1: not implemented. PNG LIVE PROOF: not done. PNG LAST-GOOD: not
  done. GLB STATUS / WAV STATUS: blocked on ContentArtifactV1.
- ROCKET-LAUNCHER FALSIFICATION STATUS: not run. RUNTIME-UNKNOWN TOOL STATUS: not
  run. CHANGESET STATUS / WORLD/MAP EDIT STATUS / PEER AUTHORING STATUS: not
  started.

- COLD-REBUILD REGRESSION MATRIX (this round): EDIT HOT CPP **NO** (proven in
  production loop); the rest are unmeasured pending ContentArtifactV1/world work.

- DID ANY TEST REQUIRE REBUILDING/KILLING mimita.exe? **No** for the code path —
  the F->G production loop ran in one process with no restart.

- NEXT LARGEST REAL COLD BOUNDARY: the cold weapon/NPC enums (`WeaponBehaviorType`,
  `NpcGoalKind`) if still authoritative; otherwise the missing generic logical
  resource primitive (ContentArtifactV1).

- LIVE-PROOF DEBT: two-process/raw-cpp run, live session/entity capture, all
  resource-generation proofs, ChangeSet/world-edit/persistence.

- CONCURRENT / UNRELATED: `--hot-combat-selftest` fails "hot pause menu ..." (GUI
  agent in-progress); not caused by this change.

## Files changed
- `src/hot-reload/production-loop-selftest.{h,cpp}`: production-loop F->G proof.
- `src/game/game-cli.cpp`: `--production-loop-selftest`.
- docs.

## Next (auto-selected)
`ContentArtifactV1 { logicalResourceId, resourceKind, contentHash, byteSize }` over
the existing cache/streamer/receiver + hashing; PNG live publication with malformed
last-good ("ui.menu.logo"); then GLB/WAV; then trace the weapon/NPC cold enums.
