# Static collision hidden-face removal — Phase A

- **Time UTC:** `2026-10-25T18:47:00Z`
- **Display time:** `2026-10-25 14:47:00 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `51f274ba`
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

- Only convex closed solids participate in union. Concave shells and non-solid
  decoration remain raw for the conservative face-removal pass.
- Maps above `static_solid_union_max_solids` skip union and use face removal;
  this is intentional to bound map-load cost.
- The final build was not launched through the dev loop because `.dev/current`
  is held by the pre-existing GUI server PID `12648` from the original recording
  session. That process was preserved. The final build's server map-load parity
  and selftest were verified; client gameplay feel needs human review.
- External commits (`cfae20a5`, `63e34fe4`, `51f274ba`) captured Phase A and the
  human's parallel NPC/map-editor work while this session was running. The AI
  did not commit.
- Human visual, bounce-feel, and multiplayer acceptance remain pending.

## Validation summary

- Source: reviewed map load, authored rebuild, editor rebuild, collision LOD
  re-decimation, server headless load, and decimation paths.
- Build: dev-loop builds `3249`, `3257`, `3258`, `3262`, `3272`, and `3273`
  published successfully.
- Tests: static-face, solid-union, authored-geometry, and map-config selftests
  PASS.
- Runtime: tdmtest3 client/server union parity; targeted seam cross; full replay
  completed with no below-bounds positions.
- Deployment: none.

## Phase B — convex solid union

`unionStaticSolidGroups` now unions validated convex closed solids through
`MimitaImpact::booleanUnion`, appends raw triangles for skipped groups, and falls
back without changing geometry on failure or output-cap overflow. It welds
vertices before Manifold import and reverses inward-wound solids.

- Added `static_solid_union`, `static_solid_union_max_triangles`, and
  `static_solid_union_max_solids` to `config/collision-lod.json` /
  `CollisionLodConfig`.
- Client GLB load, authored rebuild, LOD re-decimation, server headless load, and
  authored headless apply call union before the Phase A pass.
- Added `--static-solid-union-selftest`.

### Phase B runtime evidence

Build `3273`, run `20261025_193211_01`,
journal `C:\mimita-v9\logs6-10-2561025_193211_01\events.jsonl`.

tdmtest3:

```text
input triangles   1364
union solids      122
union output      1280
union_ms          74.4 server / 78.9 client
union_fallback    false
client/server     1280 / 1280
```

dust2cyberiav4 (cap path):

```text
union solids      485 > static_solid_union_max_solids (256)
union fallback    solid_count_cap
face removal      7442 -> 5469 (1973 removed, 21-23 ms)
```

The solid cap prevents the ~1.2 s full union seen before the cap was added.

Selftests on build `3273`:

- `--static-face-removal-selftest` PASS
- `--static-solid-union-selftest` PASS: touch `volume=2.0`, stack `2.0`,
  gap `2.0`, overlap `12.0`, single `1.0`
- `--map-authored-geometry-selftest` PASS
- `--map-config-selftest` PASS

Targeted tdmtest3 seam probe (headless server/client):

```text
before [1546.00, -1850.50, 2349.64]
after  [1541.86, -1838.22, 2349.64]
crossed y = -1845.62 at floor height
```

Full recorded-input replay completed all `4858` frames. The original scenario
header was rejected with `weapon_set_mismatch` because the current community
weapon set is `6`; a temporary copy with `weapon_set=0` was used for the
collision replay and then deleted.

Replay observations:

- `313` collision summaries, `0` below-bounds positions.
- minimum player `z = 2319.04`; map bounds minimum `z = 2304.09`.
- `6` seam summaries at `y ~= -1845.62`; all had floor-only contact normals.
- `0` non-floor contacts at that seam.
- one post-replay head rollback at `core_penetration = 0.1023 m` on a steep
  contact (threshold `0.1 m`); this is a residual limb-rollback observation, not
  a fall-through, and needs human review.
- Playback divergence is expected because the collision surface changed; the
  recorded input is not a deterministic world replay.

Minimal compatibility fixes were required to unblock the human's in-progress map
editor refactor: `MapConfigRegistry::save(bool)` signature alignment,
`HeadlessGLBObject` tracking for headless GLB overrides, `basePosition`
population on loaded GLB objects, and removal of the stray headless `options`
field. These preserve the editor refactor intent; they are not Phase B behavior.


