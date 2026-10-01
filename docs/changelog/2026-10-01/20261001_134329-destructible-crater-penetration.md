// 2026-10-01T13:43:29Z (display: 2026-10-01 09:43:29 EDT)
/* purpose
* Record the session that fixed the destructible-crater penetration regression:
* repeated shots now deepen one crater and eventually punch through, because the
* projectile sweep's closest-point-on-triangle helper no longer fabricates a
* phantom overlap in empty space. Also records the shared entity surface cache /
* AABB tree and the coarser cutter tessellation added in the same session.
*/

# Task

- Summary: Fix repeated same-crater shots not deepening / not penetrating. Root
  cause was `closestPointOnTriangle`'s wrong vertex-C region test producing a
  phantom sphere/triangle overlap at the entry plane. Also reduce generated
  hole triangles and reuse one cached per-entity world surface + `AabbTree` for
  the projectile entity query.
- Status: PASS_WITH_HUMAN_REVIEW
- Branch: `afad20a-rebuild`; base commit `647d9987`.
- Result states: selftests `PASS`; runtime visual/multiplayer/FPS acceptance NOT
  performed.

# Human feedback driving this pass

- "Shoot the hole I already made -> it gets deeper -> eventually penetrates the
  whole crate." Repeated shots at the same crater must monotonically deepen it
  and, after enough shots, punch a full tunnel through the object.
- Earlier symptom: after waiting, shooting made no new holes (cut history
  wedged).

# Root cause and fix (the penetration bug)

## Diagnostic (temporary, now removed)

Step 1 of the agreed plan was to identify the winning hit triangle. Temporary
fields (`ProjectileStepResult::hasHitTriangle/hitFeature/hitTriangleA..C`;
`CollisionCandidate::hitTri/hitFeature`; an `outFeature` argument on the
file-local `sweepSphereTriangle`) plus test-23 prints showed:

- Winning triangle for the phantom hit:
  `a=(-2.50,-2.50,-2.50) b=(-2.50,2.50,-2.50) c=(-1.25,0.33,-2.50)`.
- Axis coverage of that triangle: `u=-0.633 v=1.998` (outside).
- Winning feature index: `-2` => the static-overlap branch, not face/edge/vertex.
- Independent axis ray (`rayHitsMesh`): no hit.

All diagnostics were removed after the fix.

## Fix

File: `src/combat/projectile-simulation.cpp`, `closestPointOnTriangle`.

Old (wrong vertex-C region test):

```cpp
if (d5 >= 0.0f && d6 >= 0.0f) return c;
```

New (canonical Ericson condition):

```cpp
if (d6 >= 0.0f && d5 <= d6) return c;
```

Why: the wrong test almost never selected vertex C for a point past C, so the
interior branch returned the projection onto the triangle's infinite plane — a
point outside the triangle. That phantom point sat on the crate's entry plane at
`(0,0,-2.50)`, so `sphereOverlapsTriangle` reported an overlap once the sphere
came within radius, and the bolt stopped at the entry plane on every shot. The
face/edge/vertex branches were not the cause. Because the fix is the scalar
closest point, a cutter-placement change (the authorized "small fix") was proven
unnecessary and was intentionally not made (smallest correct change).

# Performance work in the same session

## Coarser cutter tessellation

File: `src/impact/boolean-mesh.cpp`, `circularSegmentsForRadius`.

- Old: edge target `0.05`, clamp `[8, 48]`.
- New: edge target `0.1`, clamp `[8, 24]`.

Effect: the rifle-scale rim (r ~1.29) no longer hits the 48 cap. Generated
triangles after five shots fell from 2116 to 814; the destructible self-test
still passes. To be re-raised only if a human review reports faceted holes.

## One shared cached entity surface + AABB tree

- `src/physics/physical-entity.h`: new `EntitySurfaceCacheView` and
  `cachedEntitySurface(const PhysicalEntity&)`.
- `src/physics/physical-entity.cpp`: the world-surface cache + `AabbTree`
  previously private to `collectActorEntityContacts` moved into
  `cachedEntitySurface`; `collectActorEntityContacts` now uses it.
- `src/network/server-projectiles.cpp` and
  `src/combat/client-collision-world-view.cpp`: `queryEntityTrianglesSwept` now
  reuses `cachedEntitySurface` and queries its tree, instead of re-transforming
  every entity triangle per substep. Tree results are sorted back into
  local-triangle order so the deterministic first-hit tie-break is unchanged.
- Motivation: efficiency-checker rule "one concept = one owner"; the tree/query
  work is now built once per `(id, geometryRevision, transform)` and shared.

# Files changed and exact old/new content

- `src/combat/projectile-simulation.cpp`
  - `closestPointOnTriangle`: `d5 >= 0 && d6 >= 0` -> `d6 >= 0 && d5 <= d6`
    (vertex C).
- `src/impact/boolean-mesh.cpp`
  - `circularSegmentsForRadius`: target `0.05` -> `0.1`, clamp max `48` -> `24`.
- `src/impact/destructible-selftest.cpp`
  - Removed the temporary test-23 INFO/diagnostic block; the three test-23
    assertions remain.
- `src/physics/physical-entity.h`
  - Added `struct AabbTree;` forward declaration, `EntitySurfaceCacheView`, and
    `cachedEntitySurface`.
- `src/physics/physical-entity.cpp`
  - Added `cachedEntitySurface`; removed the duplicate private
    `EntitySurfaceCache` map from `collectActorEntityContacts`.
- `src/network/server-projectiles.cpp`
  - Added `#include "physics/movement/collision-aabb-tree.h"`; rewrote
    `queryEntityTrianglesSwept` to use `cachedEntitySurface` + tree query.
- `src/combat/client-collision-world-view.cpp`
  - Same include and rewrite as the server view.

# Reasoning

- Spec alignment: `destructible-world.md` section 17 (continuous penetration),
  section 19 (`subtractCapsule(start=entryPoint, end=penetrationEnd)`), section
  44 (explicit work budgets). The fix restores the specified behavior at the
  shared projectile kernel rather than working around it in the test.
- Smallest correct change: the penetration failure was one wrong scalar test.
  The speculative cutter-placement change and extra face-branch hardening were
  dropped once the diagnostic disproved the hypotheses.
- Performance: the segment cap and the shared cache/tree reduce per-cut and
  per-substep cost without changing authority or the 60 Hz fixed-tick rule.

# Pre-existing work (not authored this session)

Left untouched: `config/accounts/default.json`, `config/analytics.json`,
`docs/specs/20260930plan.md`, `docs/specs/20261001plan.md` (untracked), and
`src/impact/impact-system.cpp` (the capsule bore + bore-walk from the prior
session). The `src/impact/impact-system.cpp` change was relied on but not
modified here.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md` (17, 19, 20, 21, 44)
- `docs/specs/manifold-destructible-integration-plan.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`
- `docs/regressions/README.md`

# Validation

- Build: `python build.py build-only` -> success; relinked
  `C:\mimita-v9\mimita.exe` (object mtimes confirm the changed units compiled:
  `physics_physical-entity.o`, `network_server-projectiles.o`,
  `combat_client-collision-world-view.o`, `combat_projectile-simulation.o`,
  `impact_boolean-mesh.o`).
- `mimita.exe --destructible-selftest` -> PASS, including:
  - "no shot stalls the cut history (queue does not wedge)"
  - "the bolt cuts deeper surface after surface"
  - "repeated shots on one crater eventually penetrate the crate"
  - tests 1-22 unchanged.
- `mimita.exe --moving-crate-selftest` -> PASS.
- `mimita.exe --destruction-replication-selftest` -> PASS.
- Cold-build debt record: appended `Cold-build occurrence 37` to
  `docs/regressions/2026-09-20/cold-build-required-REG.md`.

# Specification/behavior review result

No spec-code disagreement remains for this behavior. One older claim in the
handoff ("`sweepSphereTriangle` is shared by body/GLB collision") is inaccurate:
`src/combat/projectile-simulation.cpp` defines its own file-local `static`
sweep; actor/body/GLB collision uses the separate function in
`src/physics/movement/physics-collision-glb-sweep.cpp`. Body-sweep behavior was
therefore never at risk from this change; the selftests confirm it.

# Human review still needed

- Two-client visual: both clients see the same deepening crater and the eventual
  through-hole.
- Burst FPS: rapid fire into one crate must not spike a frame (the queue drains
  under the per-tick budget).
- Penetration tuning: `config/weapons.json` `projectile_rifle` has
  `"penetration_count": 3`; confirm the intended single-shot vs multi-surface
  behavior and retune `cut_radius_scale` / `penetration_scale` if needed.
- Confirm the coarser 24-segment rim still looks round at gameplay distance.
- Fracture tuning after real play (unchanged this session).

# Skipped / deferred (with reason)

- Cutter-placement "lid" fix (Step 2b): diagnostic proved no lid exists; skipped
  as speculative to keep the patch minimal.
- Extra face-branch hardening: the face branch was not the cause; skipped.
- `Manifold::Simplify`: not added; segment reduction was sufficient and
  Simplify risks moving/erasing a hole.
