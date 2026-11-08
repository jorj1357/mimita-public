# Live map geometry authority and remote snapshot

## Changed

- Added `authoredGeometrySignature()` over authored blocks, chunk parts/connectors/spawns, generation zones, GLB object overrides, and authored spawnpoints.
- The server now captures a pristine GLB-only `HeadlessWorld` base before applying authored geometry.
- The server polls the host map JSON on its normal cadence, detects a geometry signature change, and rebuilds authoritative collision at the next fixed-tick boundary from the pristine base.
- Rebuilds preserve active destructible state only when stable `HeadlessGLBObject.id` and triangle ranges still match. Incompatible active ranges fail closed: the old authoritative world is kept and the rejected reason is journaled.
- GLB node position/material overrides are re-applied from the pristine base, so repeated edits never accumulate duplicate authored geometry.
- Added a reliable chunked `PACKET_MAP_CONFIG_SNAPSHOT` carrying map id, config revision, geometry revision, geometry signature, content hash, and JSON bytes.
- Remote clients assemble the snapshot, validate size/hash, apply it through `MapConfigRegistry::loadFromJsonString()`, and rebuild authored render/collision geometry on their next safe frame boundary.
- The host/local editor is excluded from remote snapshots and keeps local file authority.
- Map-ready now sends the current config snapshot to non-host clients, covering late joins and post-map-change clients.
- Remote-config registry mode disables local file polling and rejects `save()` so a remote snapshot cannot silently overwrite the host file.
- Added correction diagnostics fields `geometry_revision` and `geometry_transition` to `movement.position-correction.applied`.
- Added live-rebuild timing fields: `authored_apply_ms`, `npc_collision_build_ms`, `duration_ms`, and resulting world bounds.
- Documented current host-authority behavior and future authorized non-host editing in `docs/specs/networking/networking.md`. Non-host edit submission is intentionally not implemented.

## Build unblock fixes in pre-existing working-tree changes

These files already contained unrelated uncommitted edits that prevented any build. The smallest compile fixes were applied without changing their intended behavior:

- `src/npc/npc.cpp`: replaced an out-of-scope `uctx` with the persisted `npc.utilityContext`.
- `src/network/server.cpp`: corrected `npcWorld.triangles` to `npcWorld.collisionMesh.triangles` in a startup diagnostic.
- `src/network/server-packets.cpp`: replaced legacy `gServerCoordinatorCode` / `gServerMapId` names with the declared getters.

## Build and runtime evidence

- Build: dev-loop artifact `2026-11-08T05:52:33.420610Z`.
- Build SHA-256: `2f650a9378a562c9b722aaa7aabeae449244125a6bb36cd75590fe0f65b6f3c5`.
- Runtime run: `logs/2026-11-08/20261108_055243` (mode 47, server + two clients).
- Initial map-ready snapshot to the non-host client: `map.geometry.snapshot.sent` bytes=1981 chunks=2 config_revision=2 geometry_revision=0, then `map.geometry.snapshot.applied` on the remote client.
- Live edit (`node:310` Z +10):
  - Server `map.geometry.signature` `16315895121062991499` -> `16843339634790437260`.
  - Server `map.geometry.reload.applied` geometry_revision=1, authored_apply_ms=433.354, npc_collision_build_ms=2228.3155, duration_ms=2708.7444.
  - Host client `map.geometry.client-applied` with signature `16843339634790437260`.
  - Remote client `map.geometry.snapshot.received` and `snapshot.applied` with config_revision=5, geometry_revision=1, same signature, duration_ms=404.3048.
- Restore edit (map file returned to tracked content):
  - Server geometry_revision=2, signature back to `16315895121062991499`, duration_ms=3126.8113.
  - Remote client applied config_revision=6, geometry_revision=2, same signature.
- No confirmed movement regression was observed in this bounded run. Visual confirmation of the moved GLB node is not claimed.
- Performance gap: the full rebuild blocks the server tick for 2.7-3.1 s on `dust2cyberiav4`, dominated by the Recast/NPC collision-world bake (2.2-2.6 s). This exceeds the 60 Hz budget. Follow-up should make authored collision updates incremental or move NPC/navmesh rebuild work off the fixed tick.

## Raw-base GLB authority fix — 2026-11-08T15:27Z

The earlier live rebuild still applied GLB overrides against post-LOD triangle ranges. `loadHeadlessWorld()` records object triangle ranges, then decimation/static-union/face-removal compact the triangle list. The override could therefore move the wrong triangles and leave the real node at its base position.

Fixed in the host pipeline:

- `HeadlessWorld` now retains a pristine raw GLB snapshot:
  - raw triangles
  - raw solid ids
  - raw GLB object ranges
  - raw spawn points
  - raw bounds / next solid id
- `map.geometry.raw-base-captured` is emitted at load.
- `applyAuthoredMapGeometry(HeadlessWorld&)` restores the raw base before applying GLB overrides, then finalizes collision LOD from the documented raw-base order.
- Server and client now emit `map.glb-object.applied` for each actual GLB override with:
  - object id/name
  - base/target/delta
  - triangle or vertex range
  - range validity
  - moved triangle/vertex count
- Unmatched overrides emit `map.glb-object.override-unmatched`.

### Live evidence: `logs/2026-11-08/20261108_152452`

Build artifact used: `2026-11-08T15:24:01.772036Z`.

Server:

- `map.geometry.raw-base-captured` triangles=42694, glb_objects=226.
- `map.glb-object.applied` node:90 Cube.056:
  - base `[103.2091,-62.9173,308.7747]`
  - target `[109.5890,-49.9390,304.6070]`
  - delta `[6.3799,12.9783,-4.1678]`
  - `triangle_first=17444`, `triangle_count=12`, `range_valid=true`, `triangles_moved=12`.
- Live JSON edit node:90 Z +5:
  - `map.geometry.signature` `6968283886113374888` -> `10578937424295899130`
  - reload applied geometry_revision=1
  - node:90 target became `[109.5890,-49.9390,309.6070]`, range valid, 12 triangles moved.
- Restore edit:
  - reload applied geometry_revision=2, signature back to `6968283886113374888`.
- No `movement.server-position-corrected` events were produced during the run.

Client:

- Startup `map.glb-object.applied` node:90 matched the server target, range valid, 36 vertices moved.
- Live edit `map.glb-object.applied` node:90 Z +5 matched server target `[109.5890,-49.9390,309.6070]`.
- `map.geometry.client-applied` config_revision=3, signature `10578937424295899130`.
- No `movement.position-correction.applied` events were produced during the run.
- Real `agent.move 1 0 120` after teleporting to the node area produced no position correction with the raw-base override active.

Performance evidence:

- Live funworld3 reload now takes `duration_ms` 8390.7 and 9862.3 in this run.
- `authored_apply_ms` 2702.6-3093.6; `npc_collision_build_ms` 5544.2-6606.2.
- This remains a major fixed-tick stall. It is separate from the raw-base correctness fix and still needs incremental or off-thread collision/NPC rebuild work.

### Not implemented in this step

The full hot-module implementation plan in the preceding decision set is not yet implemented:

- module catalog hot reload / `process_scope`
- full `map-editor.cpp` extraction
- module-owned undo/redo/clipboard
- clone/tombstone/arbitrary-GLB import data model
- `map-geometry` hot module

Those remain follow-up work. This step fixes the server/client GLB authority mismatch and adds the journal evidence needed to prove it.

## Repository notes

- No commit was created by this AI session. An external commit `b1207711` appeared during the session; this session does not claim it.
- Disk cleanup was required because the build failed with `No space left on device`: old `.dev/builds` artifacts and superseded run logs were removed. The current run remains in `logs/2026-11-08/20261108_055730` (mode 45 restored).
