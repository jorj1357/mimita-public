# Real ECS Tool Entity resource continuity (GLB A -> B -> D)

- EST timestamp: 2026-09-15 23:38:25 EDT (UTC 2026-09-16T03:38:25Z)
- Branch: `8292026stash`
- Base commit: `cfa44db`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--tool-entity-continuity-selftest` PASS; regressions PASS)
- Evidence class: HEADLESS / SELFTEST ONLY (no rendered frame observed)

## Task
Prove a REAL ECS Tool Entity keeps the same EntityId, actor ownership/equip
relationships, logical mesh id, and gameplay state while the logical resource
version changes A -> B (and B -> D after a code generation change) through the
canonical ContentArtifact -> PresentationResourceProvider path, observed on the
real production render path. Narrow scope; no new registry/packet/enum/GC.

## Report
- REAL ECS TOOL ENTITY: tool `E=2`, actor `P=1`, tool key
  `gameHash("tool.rocket-continuity")`, logical mesh `HOT_MESH_ROCKET`
  (`gameHash("mesh.rocket")`), owns relation `relationship.owns-tool`,
  equip relation `relationship.equips-item` (+ `relationship.contains-item` and
  `ToolRefState` via the real `actorStateEquipTool` API). Created with the real
  generic `ctx->entityCreate` capability path.
- BEFORE STATE: hash A `15701783783807986060`, handle A `1996155251760`,
  gameplay `ToolContinuityState.shotsFired=7`.
- A -> B PUBLICATION: canonical content path only (`ContentArtifact` +
  `ArtifactCache` bytes -> GLB kind validation -> `publishContentArtifact` ->
  `PresentationResourceProvider::apply`). hash B `6455222657130290700`, handle B
  distinct from handle A.
- PRODUCTION RENDER PATH: after each publish the real frame order runs
  (`GAME_DOMAIN_POST_MOVEMENT` -> `GAME_DOMAIN_RENDER`);
  `hot.presentation-mesh` -> `render.mesh` -> `submitMesh` resolves
  `handleOf(meshResourceId)`. `PresentationRender::entityMeshResourceId(E)`
  stays `HOT_MESH_ROCKET` while it resolves B. Core proof PASS.
- ENTITY CONTINUITY: E/P EntityIds unchanged; owns/contains/equips edges
  unchanged; `ToolRefState` key unchanged; `meshResourceId` unchanged; gameplay
  value unchanged. No recreation, respawn, re-equip, or inventory rebuild.
- MALFORMED C: rejected at validation; provider stays B, handle stays B, entity
  graph unchanged, render path still resolves B.
- RUNTIME PREP FAILURE: failing loader -> publication fails; B stays active;
  entity graph intact.
- RETIREMENT SAFETY: handle A observed retired exactly at the A -> B swap. The
  provider retires synchronously at the committed swap; the render path stores
  only a logical id per entity and resolves the handle at use, so no raw handle
  outlives the swap in the single-threaded main loop. No explicit GPU fence
  exists for a hypothetical multi-threaded renderer (documented gap; no GC
  system added).
- F -> G WITH B ACTIVE: real `HotReloadSystem` switch transaction (F=1 -> G=2)
  using `build/mimita-game.dll` bytes + manifest/verify/prepare/install/switch.
  E, P, equip edge, gameplay state, and B all preserved; render path for E still
  resolves B.
- D AFTER G: valid GLB D published on the same logical id; same E/equip;
  B -> D works.
- UNKNOWN LOGICAL RESOURCE: `mesh.user.test-object` published and resolved by a
  generic entity in the production render path; no cold enum/case.
- UNRESOLVED FALLBACK: `mesh.does.not.exist` is a safe skipped draw (submission
  counted, no handle, no crash).
- GLB CONSUMER COMPLETE ENOUGH? YES for the gate (real ECS tool entity, same
  EntityId/actor/owns/equips/ToolRef/mesh id/gameplay state, production render
  path resolves B, malformed + prep-failure last-good, retirement observed,
  F->G preserves B + identity, D after G, no rocket-specific reload path).
  Remaining product debt: PNG/WAV consumers and resource late join.
- DID ANY STEP REQUIRE REBUILDING/KILLING mimita.exe? No. One cold build;
  runtime resource/code steps did not restart the process.
- LIVE-PROOF DEBT: no rendered frame observed; no multi-thread GPU fence;
  PNG/WAV consumers; resource late-join current-state sync.

## Files changed
- `src/render/presentation-render.{h,cpp}`: test-only hooks `debugCreateMesh`,
  `debugRetireMesh`, `entityMeshResourceId` (no gameplay semantics).
- `src/hot-reload/tool-entity-continuity-selftest.{h,cpp}`: new proof.
- `src/game/game-cli.cpp`: `--tool-entity-continuity-selftest`.
- `docs/architecture/live-development/hot-cold-audit.md`,
  `docs/architecture/live-development/hot-kernel-next-steps.md`.

## Validation
- `python build_agent.py` -> `Status: SUCCESS` (tree already compiled/linked by
  the immediately preceding build at 23:36:47, which included the changed TUs;
  this run reported `Nothing changed`).
- `mimita.exe --tool-entity-continuity-selftest` -> PASS.
- Regression: `--glb-consumer-selftest` PASS, `--content-resource-selftest`
  PASS, `--production-loop-selftest` PASS, `--hot-combat-selftest` PASS.

## Pre-existing changes
The working tree contained unrelated concurrent-agent edits (audio, gui, npc,
hot UI modules, `live-behavior.cpp`, `hot-combat-selftest.cpp`,
`transport-generation-selftest.cpp`, etc.). They were preserved untouched. No
concurrent regression was observed in the selftests run this session.

## Next
Real PNG consumer (`ui.menu.logo`), real WAV consumer
(`audio.weapon.rocket.fire`), resource late-join current-state sync, then the
rocket multi-axis + runtime-unknown tool falsifications.
