# Static map collision double-scaled on the server

- Recorded: 2026-11-27T05:34:55Z
- Issue: `issue-b09435bb9d9d`
- Status: ATTEMPTED FIX (1), runtime evidence for arena2; full fuzz proof pending
- Owners: `src/map/map-authored-geometry.cpp`, `devscripts/gen-arena2.py`,
  `config/maps/arena1.json`
- Related: `src/render/world-box-mesh.cpp` (box builder contract)

## Symptoms

On arena2 the authoritative server collision did not match the authored blocks:

- Both authored cars settled at `z = 7.1377` instead of `0.325`. The spawn
  settle ray chose `ground_z = 6.8027`; the authored spawn is `z = 0.2`.
- The arena ball, kicked at 30 m/s along a flat floor, received an 18-24 m/s
  velocity change at `x = 9.75` with 64 world contacts, then bounced out of the
  side wall and fell below the world.
- The settle fuzz probe reported `side_rests = 41` of 55 kicks, because the fuzz
  car started on a phantom surface and was kicked from 7 m.

## Root cause

`applyAuthoredMapGeometryHeadless` built collision geometry in metres:

- boxes: `buildBoxCollisionTriangles(..., block.size * 0.5f)`;
- profiles/curves: points and extrusion half-length already in metres;

then applied `blockTransform(block)`, which contains
`glm::scale(..., block.size)` because the render path scales a **unit cube**.
The server collision was therefore multiplied by the block size a second time.
For the west goal ceiling (`position (-55.6, 0, 6.6777)`,
`size (8.8, 17.8551, 0.5)`) the phantom face sat at
`(-55.6 +/- 38.72, +/-159.4, 6.8027)` — exactly `center +/- (half * size)`.
The client render path (`box mesh` from the unit cube) was correct, so server
and client collision disagreed.

## Evidence

| Run | Observation |
|---|---|
| `20261127_045051` | `vehicle.spawn.settle` `ground_z=6.8027` `chosen_z=7.1377`; `arena.ball.world.contact` `delta_speed=18.21` at `(9.75, 0, 0.9)` with 64 contacts |
| `20261127_050830` | world collision 2,982 triangles; spawn triangle center `(-42.69, -53.13, 6.8)`; identical phantom after the box-mesh fallback change |
| `20261127_051141` | after `blockPlacement` in the headless builder: `ground_z=-0.01`, `chosen_z=0.325`, 1,587 world triangles; ball still launched by 10 m tile seams |
| `20261127_052258` | after single floor/ceiling boxes and analytic sphere-world contacts: `chosen_z=0.325`, ball rest `z=0.9093` (0.0068 m above the radius), no seam launch |

## Fix

1. `blockPlacement(block)` (translate + rotations, no scale) for metre-space
   collision: headless boxes, headless curved shapes, chunk boxes, and the
   client curved-shape render. The unit-cube box path keeps `blockTransform`.
2. `arena2` floor and ceiling are one solid box instead of a 12 x 9 tile grid;
   tiling added a collision seam every 10 m and the ball's contact shell caught
   each seam corner. A box has thickness, so the swept sphere cannot pass it.
3. `arena1` ball data now uses the reference contact constants: friction 0.35,
   restitution 0.6 (was 0.03 / 0.45; 0.03 is the drag constant, not friction).
4. Sphere entities use analytic sphere-vs-world contact (closest point plus the
   authored inward face normal) instead of the tessellated mesh's rounded
   0.1 m shell. This removes the 10 cm hover and the seam/corner launches.

## Remaining proof

- The settle fuzz probe now holds a supported rest for 0.5 s and expands the
  side recovery band to `up.z > -0.90`; the next run must show
  `side_rests == 0`.
- Verify at least one GLB map's collision bounds after the placement change,
  because every static map's server collision was doubled.
- The client curved-shape render also changed (it was double-scaled); visual
  acceptance of arena2's ramps and goal recess is pending.

## Rule

Metre-space geometry is placed, not scaled. A function named
`blockTransform` owns the render unit-cube scale; collision geometry built at
`size * 0.5` or from metre profile points must use `blockPlacement`.
