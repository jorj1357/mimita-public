# Large-map collision no longer culled by absolute world extent

Time (UTC): `2026-10-02T02:07:50Z`
Branch: `afad20a-rebuild`

## Result

Maps whose geometry is authored far from the origin (bhop1) now generate
collision. Previously every collision triangle beyond 5000 units on any axis
was silently dropped at load, and every broadphase query beyond 10000 units was
clamped to a different region, so the player fell through the blocks.

## Root cause

`assets/maps/bhop1.glb` is authored off-origin. Its single spawn node
`spawnpoint.015` is at `(-21855.4, -1367.7, 3061.4)` and the node translations
span `x = -21855.4 .. -3204.2`. Three independent absolute-coordinate limits
discarded that geometry or mis-queried it:

1. `buildCollisionMeshFromRenderMesh` (`src/map/map-loader-collision.cpp`) had
   `MAX_WORLD_EXTENT = 5000.0f` and `continue`d past any triangle whose min or
   max left `[-5000, 5000]`. Nearly the whole map was dropped, so the collision
   mesh had almost no triangles and `boundsMin/boundsMax` were wrong.
2. `appendChunkTrianglesForAABB` (`src/physics/movement/physics-collision.cpp`)
   clamped every query AABB to `[-10000, 10000]` before computing chunk coords,
   so a query at `x = -21855` searched around `x = -10000` and found nothing.
3. `clampAABB` in `physics-collision-glb-setup.cpp` clamped to `[-5000, 5000]`
   for invalid AABBs, the same absolute-coordinate assumption.

These are rectangular coordinate limits, not spatial-hash limits: the chunk
hash (`collisionChunkCoord`, chunk size 6.0) indexes any position inside int
range, so the fixed bounds were unnecessary for correctness and only broke
off-origin maps. The same defect also affected existing large maps such as
`funworldv6big.glb` (node translations up to `x = 15179`, `z = 25986`).

## Changes

- `src/physics/physics-types.h`: added one shared owner for the limit,
  `inline constexpr float kMaxWorldExtent = 1.0e7f;` (and
  `kMaxWorldAABBSize = kMaxWorldExtent * 2.0f`), documented as a sanity guard
  that must not clip legitimate large maps.
- `src/map/map-loader-collision.cpp`: included `physics/physics-types.h` and set
  `MAX_WORLD_EXTENT = kMaxWorldExtent` (was `5000.0f`).
- `src/physics/movement/physics-collision.cpp`: `appendChunkTrianglesForAABB`
  `MAX_EXTENT = kMaxWorldExtent` (was `10000.0f`) with an explanatory comment.
- `src/physics/movement/physics-collision-glb-setup.cpp`: `isValidAABB` size
  check uses `kMaxWorldAABBSize`; `clampAABB` uses `kMaxWorldExtent` (was
  `5000.0f`).

The existing `MAX_CELLS_PER_AXIS = 100` guard still bounds work for absurd AABBs,
so widening the clamp does not open an explosion path.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md` (route: movement, physics, collision).
- `docs/architecture/collision/collision.md`.
- `docs/specs/movement/movement.md` (no requirement defines an absolute world
  coordinate limit; searched for extent/bounds/5000/20000).
- Skill `docs/skills/spec-behavior-review-v1.md`: result
  `PASS_WITH_HUMAN_REVIEW`. Finding: no specification names a world extent, so
  the fixed `5000` cull was an unspecified code constant, not a specified rule.
  Severity medium. No spec-spec conflict found. No unresolved spec warning.
  Runtime/human acceptance still required.

## Validation

- Source: `git diff` reviewed; only the four collision/type files above changed.
- Build: `python build_agent.py` -> `BUILD SUCCESS`, 72 compiled, 436 skipped,
  return code 0. Evidence is build-only.
- Human in-game validation still required: load bhop1, confirm the player lands
  on and collides with the blocks instead of falling through, and confirm other
  large maps (funworldv6big) still behave.

## Notes

- No regression file added: this was a latent coordinate-cull defect newly
  exposed by off-origin maps, not a previously working behavior that broke. A
  regression entry can be added if human review confirms it previously worked.
- Pre-existing unrelated edits in the working tree were left untouched.
