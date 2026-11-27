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
