Time created: 2026-11-04T01:00:00Z
Time last updated: 2026-11-04T01:00:00Z

Purpose:
Make NPC collision geometry complete, restore reliable solid-interior detection,
add bounded push-out recovery, keep recording stop from blocking the client, and
reduce the weapon/shadow frame stalls reported on `atdm` and `dust2cyberiav4`.

## NPC collision geometry

- `src/network/server-npcs.cpp`
  - `buildNpcWorldCollision()` now copies `hw.triangles` and the matching
    `hw.solidIds`; union output is no longer silently dropped.
  - Active destructible entity triangles receive matching zero solid IDs.
  - Added `npc.collision-world.summary` with headless/NPC triangle counts,
    removed/union/unknown counts, and solid-id alignment.
- `src/physics/physics-types.h`
  - Added distinct `kRemovedCollisionSolidId` and `kUnionCollisionSolidId`.
- `src/map/map-static-union.cpp` now writes `kUnionCollisionSolidId` for union
  output instead of the removed-solid sentinel.
- `src/network/server-packets.cpp`, `server-world.cpp`,
  `server-projectiles.cpp`, and `server-world-destruction.cpp` now skip only
  truly removed solids, so union geometry is collidable and raycastable.
- `src/map/map-loader-collision.cpp` now rebuilds collision world bounds from
  the triangle AABBs after building chunks.

Root cause of the `atdm` NPC fall: the server had 51,879 headless triangles
but built the NPC world with 45,227 because all 6,654 union triangles used the
same `UINT32_MAX` value the NPC world treated as removed.

## Solid-interior detection and recovery

- `src/physics/movement/actor-final-pose.cpp`
  - `actorRootInsideClosedSolid()` now tests every outward-wound closed solid
    whose AABB contains the root, smallest first.
  - Cached per-solid bounds and interior orientation avoid treating the map's
    inward-wound room shell as a solid.
  - `findSolidPushOut()` pushes the root through the nearest enclosing face,
    capped at 2 m.
  - Recovery order is now: push-out, last-clear pose, bounded safe point,
    respawn.
  - Safe-point search is capped at 24 candidates / 2 ms.
- `src/network/server-npcs.cpp`
  - NPC recovery uses the shared push-out helper before the safe-standing search.
  - NPCs below the map floor for 60 ticks are respawned with
    `npc.below-map.respawned`.
  - `npc.root.pushout` and `npc.root.inside` now identify the solid ID.
- `src/npc/npc.h`: added a push-out log rate limiter.
- `docs/specs/npc-behavior/npc-behavior.md`: recorded the `badhouse` roof
  out-of-bounds report as spec-only work; the out-of-bounds/perch checker is
  deliberately not implemented yet.

## Recording stop

- `src/devtools/scenario-commands.cpp`: `scenario_endrecord` no longer busy-waits
  for the merge. Blocking there starved network heartbeats and disconnected the
  client.
- `src/devtools/scenario-recorder.cpp`: client/server merge now runs through a
  background future; the fixed tick only observes completion.
- `src/devtools/scenario-tracks.cpp`: merge streams the client file in two
  passes instead of loading every parsed row into memory; replace retries for
  transient Windows file locks.

## Rendering/performance

- `src/engine/engine-tick-render.cpp`: shadow map refresh now alternates frames
  (mode 2) instead of updating the full depth pass every frame.
- `src/engine/engine-tick-combat.cpp`: added
  `Combat::PostWeaponContainment` scope so weapon-related containment cost is
  visible in the perf timeline.
- `src/combat/weapon-system.cpp`: crosshair/laser world rays use the cached
  broadphase instead of scanning every map triangle.

## Validation evidence

- `npc_wall_containment_v1`: PASS in a clean run; probe reported
  `inside_closed_solid=true`, zero `npc.pose.reset`, zero
  `npc.pose.penetration`.
- `atdm` 30 s TDM run: NPC collision summary reported
  `headless_triangles=51879`, `npc_triangles=51879`, `union_triangles=6652`,
  `solid_ids_aligned=true`; final NPC traces were above the map with
  `inside_solid=false` and no negative Z falls.
- `scenario_record_roundtrip_v1`: PASS when run against a clean dev-loop
  daemon; `check_scenario_roundtrip.py` validated client/server ticks and
  removed the test artifact.
- `npc_dust2_corner_containment_v1`: zero resets, zero penetration, zero
  below-map respawns; one spawn command timed out because the client frame was
  slow, so this remains a partial check.
- Latest C++ build used by the running executable includes these changes.

## Not claimed

- Full `agent-gate.py` green was not reached because another agent was using the
  dev loop and the build/serialization lock during this session. Build,
  selftests, lint, JSON sync, and the recording roundtrip were green in the
  last complete gate attempt; the failing items were scenario command timeouts
  from the shared client, not source compilation.
- No human visual, feel, or multiplayer acceptance.
- FPS is improved by the shadow cadence change but was not re-measured at the
  reported `atdm` center point with a weapon.
- The running `mimita.exe` was intentionally left open.
