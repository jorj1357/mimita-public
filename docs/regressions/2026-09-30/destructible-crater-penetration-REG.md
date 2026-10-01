# Destructible Crater Penetration

Time created: 2026-10-01T13:43:29Z
Time last updated: 2026-10-01T13:43:29Z

Status: ATTEMPTED FIX (5)

Related specification:
`docs/specs/destructible-world/destructible-world.md`

Related changelog:
`docs/changelog/2026-10-01/20261001_134329-destructible-crater-penetration.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-30T21:56:34Z`

Human playtest report: shooting a hole that was already made did not cut again.
Repeated shots at the same crater stopped removing material, and no shot ever
punched through the crate. A related symptom appeared while iterating: after
waiting, shooting made no new holes at all (the cut history appeared wedged).

### Expected Behavior

Per `docs/specs/destructible-world/destructible-world.md` section 17
(penetration) and section 19 (destruction operations), a projectile should
subtract a capsule from its entry point toward its penetration end. Shooting an
existing crater must monotonically deepen it, and after enough shots the path
must become empty so the object is penetrated.

### Actual Behavior

Repeated shots at the same crater stopped deepening. The full-chain self-test
(`mimita.exe --destructible-selftest`, test 23) drove the real projectile kernel
from `(0,0,-8)` along +Z: the bolt bored a real tunnel (remaining volume fell
from 125 to ~98.8 m3) but the kernel still reported an entity impact at exactly
`(0,0,-2.50)` with normal `(0,0,-1)` on every shot, so the test's
"eventually penetrate" assertion never became true.

### Why This Is Bad

The central destructible-world promise ("shoot the hole I already made and it
gets deeper, eventually penetrating") was not met. The bolt was effectively
stopped by an invisible surface located in already-empty space.

### Specification

`docs/specs/destructible-world/destructible-world.md`

Relevant requirements:

- Section 17: penetration is continuous; a projectile with enough retained
  motion continues through the material.
- Section 19: `subtractCapsule(start = entryPoint, end = penetrationEnd,
  radius = calculatedRadius)`.
- Section 44: heavy destruction work is budgeted.

### Wrong Code

File:

`src/combat/projectile-simulation.cpp`

```cpp
// closestPointOnTriangle, vertex-C region
glm::vec3 cp = p - c;
float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
if (d5 >= 0.0f && d6 >= 0.0f) return c;
```

### Confirmed Cause

The sphere-vs-triangle static-overlap test calls `closestPointOnTriangle`. Its
vertex-C region test was wrong. It returned true only for points both "beyond B"
and "beyond A" (`d5 >= 0 && d6 >= 0`), which almost never holds for a point past
vertex C. Such a point then fell through to the interior branch, which returns
the projection onto the triangle's infinite plane instead of clamping to the
triangle. That plane point was far outside the triangle.

Diagnostic evidence (temporary instrumentation, now removed): the winning
triangle for the phantom hit was
`a=(-2.50,-2.50,-2.50) b=(-2.50,2.50,-2.50) c=(-1.25,0.33,-2.50)` — entirely on
the left of the crate face — and the sweep's own barycentric coverage of the
axis was `u=-0.633, v=1.998` (not inside). The winning feature index was `-2`,
meaning the hit came from the static-overlap branch, not the face/edge/vertex
branches. An independent axis ray (`rayHitsMesh`) hit nothing. The closest
point the buggy function returned for the axis was `(0,0,-2.50)`, so once the
sphere came within its radius of that phantom point, `sphereOverlapsTriangle`
reported a false overlap and the projectile stopped at the entry plane.

The face/edge/vertex branches were therefore not the cause; the earlier
hypothesis that "the sweep face branch accepts an occluded/back surface" was
close but incorrect.

### Attempted Fixes (prior sessions, uncommitted)

Attempt 1: SDF/grid surface path — abandoned; it eroded the whole shell.

Attempt 2: Canonical base mesh + ordered cut history with Manifold; one
localized spherical cut. Correct geometry, but repeated same-spot shots did not
deepen.

Attempt 3: Queue + budget (`ImpactSystem::submit` enqueues; `flushPendingCuts`
runs once per fixed 60 Hz tick with `kMaxCutsPerEntityPerTick = 8` and
`kCutBudgetMsPerTick = 2.0 ms`) and no-op cut detection. This fixed the
"after waiting, shooting makes no new holes" queue-wedge stall via
`discardPending()` on a failed rebuild, but not penetration.

Attempt 4: A swept-capsule "bore" cutter (`ImpactEvent::boreLength`) plus a
deterministic bore-walk in `ImpactSystem::submit` that starts each new capsule
past the deepest existing same-axis capsule. This made the cut history deepen
and produced a real tunnel (volume 125 -> 98.8 m3), but the kernel still
reported the phantom entry-plane impact every shot, so the test could not pass.

### Attempted Fix 5

Time:
`2026-10-01T13:43:29Z`

Change:

Corrected the `closestPointOnTriangle` vertex-C region test to the canonical
Ericson condition. Removed the temporary instrumentation. No cutter-placement
change was needed: the diagnostic proved the geometry was never "lidded".

Result:

- `mimita.exe --destructible-selftest` -> PASS, including test 23's
  "repeated shots on one crater eventually penetrate the crate".
- The bolt now advances each shot (hit z = -0.31, 0.58, 1.48, 2.38 then no
  surface -> tunnel) instead of stopping at the entry plane.

### Corrected Code

File:

`src/combat/projectile-simulation.cpp`

```cpp
glm::vec3 cp = p - c;
float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
// Vertex C region (Ericson, ClosestPtPointTriangle): the point is beyond C
// when d6 >= 0 and d5 <= d6. The previous test (d5 >= 0 && d6 >= 0) rarely
// fired for points past C and let the interior branch return a plane point
// outside the triangle, fabricating a sphere overlap where none exists.
if (d6 >= 0.0f && d5 <= d6) return c;
```

### Fix

`closestPointOnTriangle` now clamps to vertex C for points past C, so it can
only return a point on or inside the triangle. The phantom overlap at the entry
plane is gone and the projectile reaches the crater floor each shot.

### Proof

Human review:

Pending. Needs a live two-client session: shoot an existing hole and confirm it
deepens, then penetrates; measure burst FPS.

Automated proof:

- `python build.py build-only` -> success; relinked `mimita.exe`.
- `mimita.exe --destructible-selftest` -> PASS (tests 1-23).
- `mimita.exe --moving-crate-selftest` -> PASS.
- `mimita.exe --destruction-replication-selftest` -> PASS.

### Solution

Not yet claimed. Status remains `ATTEMPTED FIX (5)` until a human confirms the
behavior in a live session.

---

## Related follow-up work in the same session (performance)

- `src/impact/boolean-mesh.cpp`: `circularSegmentsForRadius` now targets a
  0.1 m edge length and clamps to `[8, 24]` (was `0.05` / `[8, 48]`). The
  rifle-scale rim (r ~1.29) no longer hits the 48 cap; generated triangles after
  five shots fell from 2116 to 814 with the self-test still passing.
- `src/physics/physical-entity.{h,cpp}`: the per-entity world-space collision
  surface cache and `AabbTree` previously private to `collectActorEntityContacts`
  are now one shared owner, `cachedEntitySurface`. The server and client
  projectile entity queries (`src/network/server-projectiles.cpp`,
  `src/combat/client-collision-world-view.cpp`) reuse it and query the tree
  instead of re-transforming the whole surface per substep.
