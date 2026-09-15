# Content-addressed artifact acquisition + always-live creation audit

- EST timestamp: 2026-09-15 15:49:33 EDT (UTC 2026-09-15T19:49:33Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--artifact-cache-selftest` 13/13; full suite 32/32)

## SECTION 1 — DISTRIBUTED GENERATION IMPLEMENTATION
- ACQUIRE STATUS: **PARTIAL** — local acquisition state machine exists
  (`ArtifactAcquirer`: Requesting/Receiving/Verifying/Complete/Failed); the
  network transfer/chunking over the wire is **not** implemented.
- CACHE STATUS: **EXISTS** — `ArtifactCache` content-addressed, immutable
  (`<root>/<hash>.bin`), deduplicated by hash, never overwrites an existing key,
  re-verifies stored bytes.
- VERIFY STATUS: **PARTIAL** — artifact hash verification + load-local validation
  exist; peer-side ABI/generic-capability/schema-version/dependency gates against
  an acquired artifact are not yet wired.
- REAL READY STATUS: **MISSING** — `GenerationDistribution` tracks per-peer
  phases/quorum, but the real client path does not yet ACQUIRE→VERIFY→READY.
- TICK MAPPING STATUS: **MISSING** — existing "local tick >= switchTick"; not
  proven that client tick N == authoritative server tick N. Requires an explicit
  mapping before live switch.
- SHARED SWITCH STATUS: **PARTIAL** — host scheduling + `CodeGenerationPacket`
  SWITCH/switchTick exist; gated by quorum (added last pass).
- GENERATION PROVENANCE STATUS: **PARTIAL** — reconciliation receives real
  `predictedGeneration` (local active) vs `authoritativeGeneration`
  (`ctx.serverCodeGeneration`). Interpolation/rewind still pass 0 (need per-sample
  generation storage on the sample structs).
- LATE JOIN STATUS: **MISSING**.
- FAILURE/ROLLBACK STATUS: **PARTIAL** — last-good preserved inside
  `HotReloadSystem`; peer VERIFY failure paths depend on the missing wire-up.
- REAL SERVER+CLIENT PROOF: **NO** (no live two-process run). Compiled + unit
  proof only.
- LIVE HOT-EDIT PROOF: **NO**.
- DISTRIBUTED MILESTONE COMPLETE? **NO** — cache/verify core exists; wire
  transfer, READY wire-up, tick mapping, per-sample provenance, late join, and the
  live proof remain.

## SECTION 2 — ALWAYS-LIVE CREATION AUDIT
- RAW CPP ADD/DELETE/RENAME LIVE? **EXISTS** (for hot sources) — the manifest
  globs are re-resolved each build (`hot-reload-system.cpp`: "Re-resolve globs so
  live-added/removed/renamed hot files change"); new/deleted/renamed `*.cpp` under
  `src/hot-reload/modules/**` (globs) are picked up without an exe rebuild.
- MULTI-FILE REFACTOR LIVE? **PARTIAL** — inside globbed hot sources yes; edits to
  cold sources or to `game-api.h`/kernel ABI require a cold rebuild.
- MOVEMENT REWRITE LIVE? **EXISTS** — movement policies are hot (`movement-air/
  ground/gravity/speed-policy/jump/dash/freeze`, `movement.system`).
- ROCKET VISUAL CPP FIX LIVE? **PARTIAL** — presentation modules + `render.mesh`/
  `effect.spawn` are hot; not re-audited end-to-end in this pass.
- COUNTERSTRIKE UI CPP FIX LIVE? **PARTIAL** — `ui.frame` + `render.ui` +
  `modules/ui/hud.cpp` exist; CS-specific UI composition ownership not re-audited.
- TEMP HOT COMMANDS STATUS: **EXISTS** — `MimitaHotPackage::CommandRegistrar`
  used by banana/editor/movement/demo modules; commands are added/removed with the
  hot package. Cross-generation command safety not stress-tested.
- ENTITY EDITOR STATUS: **EXISTS/PARTIAL** — `modecreate`/`editor-behavior` +
  generic EntityId + dynamic components + relationships + fork ops
  (`EDITOR_FORK_*`).
- WORLD EDITING STATUS: **PARTIAL** — same world + generic entity replication;
  in-world create/move/duplicate exists; "save as ChangeSet" persistence partial.
- ASSET HOT-UPLOAD STATUS: **MISSING** — no content-addressed resource
  acquisition/publish; `render.mesh` is hot but resource introduction is not.
- SHARED SOURCE EDITING STATUS: **MISSING** (single-author local only).
- MULTI-USER COLLAB STATUS: **MISSING** (design/audit only, as scoped).
- CHANGESET STATUS: **PARTIAL** — editor fork/undo/redo exists; a general
  ChangeSet (source + resource + world edits) published as a candidate generation
  is not built.
- ZOMBIE TOWER SCENARIO RESULTS:
  - tower geometry/entities: POSSIBLE NOW (entity + transform + mesh + collision)
  - zombie behavior hot C++: POSSIBLE NOW (actor components + hot systems, npc.*)
  - temp `spawnzombie` command: POSSIBLE NOW (`CommandRegistrar`)
  - wave/spawner behavior: POSSIBLE NOW (hot system + events)
  - weapon/tool: POSSIBLE NOW (tool entity + BehaviorBindings + hot behavior)
  - attach model/effects/audio: PARTIAL (hot presentation/effects; asset
    introduction MISSING)
  - health/wave UI: POSSIBLE NOW (`ui.frame` + `render.ui`)
  - XP/gold/progression reuse: PARTIAL (audit not completed)
  - win/loss rules: POSSIBLE NOW (hot gamemode systems + match capabilities)
  - live testing while server stays live: PARTIAL (local live build works; remote
    distribution MISSING)
  - remove temp commands: POSSIBLE NOW
  - publish generation to peers: MISSING (ACQUIRE/READY wire-up)
- TOP 5 REMAINING COLD BLOCKERS:
  1. Distributed wire-up: artifact transfer + client ACQUIRE→VERIFY→READY + tick
     mapping (`multiplayer-tick.cpp`, `server.cpp`, `HotReloadSystem`).
  2. Cold-source edits / kernel ABI changes (`src/hot-reload/game-api.h`,
     non-globbed sources, `mimita.exe`) still require a cold rebuild.
  3. Per-sample generation provenance in interpolation/rewind (sample structs in
     `multiplayer-interpolation.cpp` / `server-players.cpp` history).
  4. Asset/resource hot-upload + content-addressed resource acquisition (no
     owner yet; `render.mesh` hot but resources are cold-introduced).
  5. Shared/multi-user source + general ChangeSet model (design only).

## Notes
- Discipline: this pass implemented only the immediate-priority ACQUIRE/CACHE/
  VERIFY core (unit-tested) and audited the rest; it did not build a CRDT editor,
  Git replacement, sandbox, or asset marketplace. No movement/presentation files
  touched.
- The distributed milestone is **not** declared complete; Section 2 items marked
  POSSIBLE NOW are compositional claims from primitives, not live-proven.

## Files changed
`src/hot-reload/artifact-cache.{h,cpp}` (new),
`src/hot-reload/artifact-cache-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Wire ACQUIRE/VERIFY/READY into the real client path + authoritative tick mapping,
then the two-process live proof; then per-sample generation provenance; then
asset/resource hot-upload and the ChangeSet model.
