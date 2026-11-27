# Authored block edits now reach authoritative player + NPC collision in about one second

## Scope

- Fixed the map-editor divergence where a placed-then-deleted authored block kept
  colliding on the authoritative server long after the editor stopped drawing it.
- No change to `src/world/world.h` or `src/network/server.h`; the only new header
  is `src/map/authored-block-rebuild.h`, included by two translation units.
- Runtime evidence is from dev-loop isolated run `20261127_021425` (generation
  755, launch mode 45, map `dust2cyberiav4`, 1 NPC) and the earlier shared runs
  used for the before numbers.

## Diagnosis

Before the change the editor mutated `MapConfigRegistry` and rebuilt only the
client `World` immediately. The authoritative `HeadlessWorld` was updated only by
`AuthoredWorldRebuildWorker`, which performs a full authored apply plus a full
Recast navmesh bake in one single-slot request. Every newer edit superseded the
in-flight candidate and its completion was discarded, so the server could trail
the editor for tens of seconds.

Measured before:

- `logs/2026-11-27/20261127_001718/client-01/events.jsonl`: `block_1` deleted at
  00:20:17, authoritative headless apply at 00:20:31 (about 14 s).
- `logs/2026-11-27/20261127_000428/client-01/events.jsonl`: edit at 00:19:06,
  `map.geometry.reload.applied` at 00:20:54 (about 108 s), `worker_ms=29727`,
  `npc_collision_ms=22331`, with several `map.geometry.rebuild.discarded` rows.

Two additional defects were found and fixed:

- `server-gamemode.cpp` marked a **rejected** authored rebuild signature as
  applied, so a transient reject could leave the server permanently stale.
- `clearAuthoredMapGeometry()` cleared `collisionMesh` but left
  `collisionChunks`/`collisionTree` referencing it. Presentation-only editor
  rebuilds (material/color/opacity) therefore wiped or corrupted the client's
  collision view on every such edit. Collision is now left intact for
  presentation-only rebuilds; full rebuilds clear it themselves.

## Change

- New owner `src/map/authored-block-rebuild.{h,cpp}`:
  - `authoredNonBlockGeometrySignature()` distinguishes block-only edits from
    structural (chunk/GLB/zone/spawn) edits.
  - Applied-block/parity bookkeeping and a 100 ms coalescing window for drags.
- `map-authored-geometry.cpp`:
  - Headless authored apply gained a `finalizeCollision` switch. A block-only
    edit re-applies the pristine raw base plus the authored tail without
    decimate/union/face-removal, so the authoritative triangle set matches the
    editor instead of taking the multi-second LOD pass.
  - Per-object GLB override events are suppressed on the fast path (491 events
    per apply were a measurable part of the stall).
  - `clearAuthoredMapGeometry()` no longer clears collision.
- `server-gamemode.cpp`:
  - Signature changes that are block-only, on a static map, with no active
    destructible cut and no in-flight structural rebuild are applied in place to
    the live `HeadlessWorld` and its NPC mirror on the server thread, then
    `geometryRevision` is bumped and the normal config snapshot is broadcast.
  - Structural changes keep the existing async worker.
  - Reject/failure now records a 2 s retry backoff instead of claiming the
    signature applied.
- `server-world.cpp`: headless chunk broadphase can be rebuilt without the 4^3
  projectile sub-grid. `gatherHeadlessTrianglesForAABB` already falls back to
  chunk-level queries when the sub-grid is absent.
- `map-editor.cpp`: shape/profile/size-parameter edits mark collision dirty so
  `flushCollisionNow()` repairs the editor world on exit.
- Regenerated the stale `config/hot-reload/packet-ledger.json` because the build
  refused to start (`[PACKET CODEGEN] ERROR stale ...`). This unblocked the build;
  it is not a packet-definition change by this task.

## Build evidence

- Generation 755 published after `map-authored-geometry.cpp`,
  `server-gamemode.cpp`, `server-world.cpp` compiled cleanly (failed first attempt
  was a declaration-namespace mistake in the new header, fixed and rebuilt).
- Build notes: `bc4ce0787943`, `5f5bce431fba`, `86f5fa4ef79d`, `1bbd4c99d35c`,
  `e62108524b95`.

## Runtime evidence

Run `20261127_021425`, game generation 755, mode 45, `dust2cyberiav4`, 1 NPC.
Commands were sent through the isolated test-run terminal queue.

- `mapeditor add block paritytest` (client autosave revision 2):
  `map.geometry.block-delta.applied` at 02:15:06,
  `authored_ms=299.97`, `npc_ms=428.68`, `duration_ms=728.68`,
  authoritative triangles 9484, `finalize_collision=false`.
- `mapeditor select paritytest` + `mapeditor delete` (client
  `map-editor.object.deleted`, autosave revision 3 at 02:15:14):
  `map.geometry.block-delta.applied` at 02:15:15,
  `authored_ms=101.46`, `npc_ms=899.25`, `duration_ms=1000.72`,
  authoritative triangles 9472 (the block's 12 triangles are gone),
  `geometry_revision=2`, no `map.geometry.rebuild.discarded` rows.

Client-to-authority delete latency is therefore about one second (250 ms file
poll + up to 100 ms coalescing + the apply), down from 14-108 s.

## Remaining work

The apply still blocks the fixed tick for roughly 0.7-1.0 s, dominated by
rebuilding the NPC collision mirror (`buildNpcWorldCollisionDetached`, 430-900 ms
in the samples). That is a large improvement over the previous multi-second to
multi-minute stall but is not yet "one tick". Follow-up: patch the live NPC
collision mesh (append the block's triangles, mark removed block triangles
hidden, rebuild only the AABB tree and touched chunk cells) instead of copying
and re-finalizing the whole mesh. Also schedule a deferred full-LOD rebuild so the
fast path's un-decimated triangle set is eventually optimized again.

## Claims

- Source: changed as listed; `world.h` and `server.h` unchanged.
- Build: generation 755 compiled and published (build evidence only).
- Runtime: authoritative collision triangle set changed in the same apply as the
  editor delete, with measured timings; verified through the server journal.
- Human/visual acceptance: not performed (no manual walk-through in this
  session); the proof is journal-level authoritative state, not a human report.

## Measurement tooling (same session)

The before/after numbers above were originally extracted by hand from the run
journals. That was re-invented per task and is now automated:

- Added `devscripts/measure-events.py`:
  - resolves a run id, run directory, or `events.jsonl`;
  - prints the pinned identity from `run.json` (run id, game generation, DLL,
    mode/map/profile, journal paths);
  - summarizes event counts and numeric fields with n/min/mean/p50/p95/max/sum;
  - measures A->B latency on same-host wall time, preferring correlation ids;
  - supports `--compare` for a before/after table;
  - ships presets (`authored-block-parity`, `server-tick`, `frame`);
  - supports `--json`, `--out`, and `--self-test` (synthetic round-trip passes).
- `docs/workflows/measurement-hypothesis-driven-improvement.md` gained
  section 4a "Automatic extraction (preferred)" and a checklist item.
- `docs/ROUTER.md` routes measurement work to the extractor.
- `AGENTS.md` now asks agents to persist the extractor report with `--out`.

Observed during this work: every journal older than run
`20261127_082112_controller-proof` had been pruned (only `run.json` remained),
so the raw paths cited above no longer resolve. The quoted numbers survive in
this changelog and issue, and `--out` is the durable path going forward. This is
a retention/storage observation, not a change in the measured result.

Live extractor check: `python devscripts/measure-events.py
20261127_082112_controller-proof --preset server-tick` reported generation 794,
`arena2`, 153 `performance.server-tick-window` rows, and
`total_avg_ms` mean 2.562 ms / p95 3.120 ms. Self-test: PASS.

## Automatic build approval (same session)

Agents no longer wait for human approval before a shared game-DLL build.

- `agent-coord.py build request --auto --why ... --expected-seconds ... --paths
  ... --issue ...` creates the request and applies the peer/resource policy.
- Policy (`build policy` / `auto-approve`):
  - active peer = `status='active'` with a heartbeat within **5 minutes**;
  - idle peers (no unexpired lease, no live runtime, not building/editing/
    testing/debugging/review) do not block → `auto:peers_idle`;
  - no active peers → `auto:no_peers`;
  - working peers or a requested-path lease → `ask`; the requester messages up
    to three peers, one non-requester approval approves;
  - `approved`/`queued`/`building` request → `wait` (`build_in_progress`);
  - CPU > 90% or available RAM < 1024 MB → `wait` (`resource_pressure`);
  - halt/freeze → `reject`.
- `build approve`/`build reject` now require a registered actor, reject
  requester self-approval, and append an audit row to `build_approvals`
  (`auto:*`, `peer`, `owner`, `objection`) with reason and evidence.
- `build_update` emits `coord.build.finished` for terminal statuses so waiters
  can resume from events without polling.
- Docs: AGENTS gate 0 step 2-5, `docs/specs/dev-loop/agent-build-queue.md`
  (policy matrix, 5-minute TTL, resource reserve, audit), and
  `agent-coordination.md` (policy pointer).
- Tests: `build policy --simulate no_peers|peers_idle|peer_active|path_lease|
  pressure|build_in_progress`; a real `auto-approve` produced
  `actor=auto:no_peers, status=approved`; a peer approval produced
  `actor=map-authored-parity, status=approved`; requester self-approval was
  refused; two recorded asks with no objection approve as
  `auto:no_response`. Self-test requests were rejected afterward so they do not
  block the one-build slot.
- `python -m unittest devscripts.quality.test_agent_coord -v`: 47 tests OK,
  including six new tests for alone/idle-peer/working-peer/path-lease/
  active-build/no-response decisions.
- No C++/DLL input changed for this policy work, so no build approval was
  consumed.

## Incremental NPC collision patch (same session)

Replaced the full NPC mirror rebuild in the block-delta fast path with an
in-place patch.

- `src/map/authored-block-rebuild.{h,cpp}`:
  - captures authored block triangle ranges once after a full non-finalized
    apply (`authoredNpcBlockStateCapture`);
  - on a later edit, hides removed/changed block ranges (`collisionMesh.hidden`
    + `kRemovedCollisionSolidId`), appends added/changed block triangles copied
    from the headless block tail, fills AABB/OBB caches, inserts the new indices
    into `collisionChunks`, rebuilds `collisionTree`, refreshes solid bounds, and
    bumps `renderRevision`;
  - validates the whole plan before mutating; falls back to the previous full
    build and re-captures when ranges cannot be trusted.
- `src/map/map-authored-geometry.cpp` records the headless block tail
  (thread-local) and exposes `authoredBlockTriangleCount` /
  `authoredHeadlessBlockTail`.
- `src/network/server-gamemode.cpp` uses the patch first and logs
  `npc_full_rebuild` and `npc_patch_failure` in `map.geometry.block-delta.applied`.
- Smooth seam topology is cleared on a patch (the movement code already
  disables smoothing when the arrays do not match), restoring on the next full
  rebuild. A Recast full bake is still triggered off-tick by the content hash;
  targeted dirty tiles remain a follow-up.
- Builds: generation 798 (diagnostics) and 799 (zero-tail fix) compiled and
  published. Generation 798 runtime test showed both edits still fell back
  (`npc_full_rebuild=true`, `npc_ms=755/1290` then `1584/1810`) because deleting
  the last block produced an empty tail that `authoredHeadlessBlockTail`
  treated as unavailable. Generation 799 fixes that by separating tail validity
  from count.
- **Generation 799 runtime proof** (`run 20261127_175851`, mode 45,
  `dust2cyberiav4`, DLL `af19584b51e7`): the server poll path was driven by
  editing the map JSON directly because the dev-loop command router still
  reported a live client dead. Four block changes were applied:
  - first add (state not yet captured): `npc_full_rebuild=true`,
    `authored_ms=248.6`, `npc_ms=871.4`, `duration_ms=1120.0`;
  - delete: `npc_full_rebuild=false`, `authored_ms=88.6`, `npc_ms=21.1`,
    `duration_ms` about 100 ms;
  - add: `npc_full_rebuild=false`, `authored_ms=169.9`, `npc_ms=21.8`;
  - delete: `npc_full_rebuild=false`, `authored_ms=78.0`, `npc_ms=21.7`.
  The incremental NPC phase dropped from 755-1810 ms to about 21 ms. The report
  is persisted at `.dev/metrics-npc-patch-799.md`; the test map was restored to
  `blocks: []`.
- The proof used the authoritative server's production map-file poll, not the
  map-editor UI command path, because `dev-loop.py --command` reported the live
  test client dead. Visual/editor acceptance remains unverified.

## Incremental navmesh tiles and NPC compaction (same session)

- `RecastNavigationBackend::markDirtyTiles(boundsMin,boundsMax)` maps a changed
  block AABB to the 32 m navmesh tiles. `prepare()` and `query()` now accept
  explicit dirty tiles even when the collision content changed, instead of
  marking every tile and falling back to a full bake.
- `RecastNavMeshCandidate::build` now builds a **tiled** navmesh and `adopt`
  copies `tileRefs`/`tileOrigin`/`tileCount`, so the incremental tile path is
  active. Previously the adopted candidate was monolithic, so `markDirtyTiles`
  was inert.
- An explicit authored tile is allowed one bounded tile rebuild even when the
  navigation policy defers rebuilds inside the fixed tick; whole-map bakes stay
  forbidden.
- Hidden authored NPC triangles are counted; at `>=1024` or `>25%` of the NPC
  mesh, the block-delta path performs one full NPC rebuild and recaptures ranges
  (compaction), restoring smooth topology too.
- Runtime evidence (gen 804, run `20261127_183715`, mode 45, dust2cyberiav4):
  deleting the block emitted `npc.navmesh.tiles.marked` with `marked=1`,
  `dirty_total=1` (one 32 m tile), no full `npc.navmesh.bake-success` after the
  edit, and the NPC patch stayed incremental (`FULL=false`, `npc_ms=103.8`).
  The tiled candidate makes the first full NPC build slower (1571 ms) but it is
  off-tick/prewarm.
- Gen 805 adds the bounded in-tick tile rebuild; it compiled and published. The
  final `navmesh_version` increment check was blocked by isolated test clients
  failing to connect, so the tile rebuild is build-proven but not
  runtime-verified end-to-end.
- Compaction was not runtime-triggered (it needs roughly 86 block edits to cross
  1024 hidden triangles).
- `config/debuglogger.json` (temporarily verbose for the tile event) and the
  test map (`blocks: []`) were restored.

- Known follow-ups: Recast targeted dirty tiles, periodic compaction of hidden
  NPC triangles, and a verified gen-799 add/delete run.



