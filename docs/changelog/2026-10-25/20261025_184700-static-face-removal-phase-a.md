# Static collision hidden-face removal — Phase A

- **Time UTC:** `2026-10-25T18:47:00Z`
- **Display time:** `2026-10-25 14:47:00 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `63e34fe4`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Implement Phase A of the static collision plan: remove internal faces between
touching map solids while preserving exposed floors, walls, and slopes. The
approved scope was Phase A first, convex closed solids, and base GLB plus
authored blocks together.

## What changed

- Added `src/map/map-static-union.{h,cpp}`:
  - detects fully covered coplanar opposing triangles between different solids;
  - requires closed, consistently wound, nonzero-volume, convex source solids;
  - removes only triangles whose full area is covered;
  - reports bounded counts and falls back without changing geometry on mismatch.
- `src/physics/physics-types.h`: added optional parallel `solidIds` to
  `CollisionMeshCache`.
- `src/world/world.h` / `world.cpp`: added authored-object vertex ranges and
  cleared them with the world.
- `src/map/map-loader-collision.cpp`:
  - tags collision triangles by validated source solid;
  - runs Phase A in `redecimateCollision`;
  - optional ID-preserving decimation for the server.
- `src/map/map-authored-geometry.cpp`:
  - authored blocks now record their render range and rebuild collision from the
    same render mesh path;
  - Phase A runs before chunks are rebuilt;
  - removed the obsolete direct authored collision append.
- `src/world/world-gltf-loader.cpp`: Phase A on GLB load and procedural template
  load.
- `src/mapeditor/map-editor.cpp`: editor rebuild uses the authored rebuild path.
- `src/network/server.h` / `server-world.cpp`: headless server carries solid IDs,
  validates per-node solids, decimates with IDs, and runs the same Phase A pass.
- `config/collision-lod.json`: added `static_face_removal`.
- `src/config/collision-lod-config.*`: parse and report the toggle.
- `src/game/game-cli.cpp`: added `--static-face-removal-selftest`.
- `docs/architecture/collision/collision.md`: documented the static-map owner
  and its limits.

## Runtime evidence

Build `3249` (Phase A before the final convexity guard):

- Client tdmtest3: `1364 -> 939` triangles, `425` removed, `0` fallback,
  `2.65 ms`.
- Server tdmtest3: `1364 -> 939` triangles, `425` removed, `0` fallback,
  `2.50 ms`.
- Client and server agreed on `939` triangles.

Targeted seam probe on build `3249`, same map and seam plane:

```text
before  player_position [1546.00, -1850.50, 2349.63]
after   player_position [1526.09, -1829.75, 2349.64]
crossed y = -1845.62 with z staying on the floor
near-seam contacts: floor triangle 371, normal +Z only
y velocity through the seam: +19.63 preserved
```

The pre-fix failure was the hidden face `tri 600` at `y = -1845.62`:
`y` velocity `+22.91 -> +0.04`.

Full replay on build `3249`:

- `collision.last_valid.rollback = 0`
- minimum observed collision-summary player `z = 2318.95`; map bounds minimum
  `z = 2304.09`; no below-bounds positions.
- The replay path diverges because removing the seam changes movement; this is
  expected, not a rollback.

Build `3257` (final source incl. convexity guard):

- `mimita.exe --static-face-removal-selftest` PASS:
  touching boxes `24 -> 20`, stacked boxes `24 -> 20`, gap `24 -> 24`,
  partial overlap `24 -> 22`, non-coincident overlap `24 -> 24`,
  single box `12 -> 12`, unknown IDs `24 -> 24`.
- Direct final-build server load of tdmtest3:
  `[COLLISION LOD] ... static_face_removal=1`,
  `loaded map collision triangles=939`, no fallback.

## Limits and open evidence

- Phase B (Manifold union of partially overlapping convex solids) is not
  implemented; Phase A handles fully covered coincident faces.
- Only convex closed solids participate. Concave shells and non-solid
  decoration remain raw.
- The final build was not launched through the dev loop because `.dev/current`
  is held by the pre-existing GUI server PID `12648` from the original recording
  session. That process was preserved. The final build's server map-load parity
  and selftest were verified; client gameplay feel needs human review.
- An external commit (`63e34fe4`) captured most Phase A changes while this
  session was running. The remaining source diff in this session is the
  convexity/volume guard and temporary-debug cleanup. The AI did not commit.
- Human visual, bounce-feel, and multiplayer acceptance remain pending.

## Validation summary

- Source: reviewed map load, authored rebuild, editor rebuild, collision LOD
  re-decimation, server headless load, and decimation paths.
- Build: dev-loop builds `3249`, `3257`, and `3258` published successfully.
- Tests: new static-face selftest PASS.
- Runtime: tdmtest3 client/server triangle parity; targeted seam cross with
  floor-only contacts; no rollbacks or below-bounds positions.
- Deployment: none.
