// 2026-10-02T05:31:34Z (display: 2026-10-02 01:31:34 EDT)
/* purpose
* Focused handoff for a new agent to work ONLY on weapon collisions.
* Explains the current state, the exact bug reports, the code paths, and the
* safest first steps. Does NOT define gameplay behavior; see
* docs/specs/destructible-world/destructible-world.md and
* docs/architecture/collision/collision.md.
*/

# Handoff: Weapon Collision Fix

## The problem (human report)

- The player "falls through the world" and "bounces off the world strangely"
  when the weapon is in play. The weapon collision feels too big / too sharp.
- This is specifically the **weapon** hitbox interacting with world geometry; the
  player's body triangles are fine.

## What was just changed (attempt 12) — the likely regression

In the previous pass, `config/weaponcollisions.json` was converted so **every
weapon** uses `"source": "boxes"` (a single box per weapon, "each face 2
triangles"). The intent: run weapon hitboxes through the **same actor-vs-world
triangle path** as the player's body. Before this, weapons used the legacy
**sphere/capsule** representation (smooth, stable, per `weaponcollisions.json`
`source: "json"` or `"capsule"`).

Two changes were made together:
1. `src/physics/movement/actor-collision-mesh.cpp` `collectActorCollisionMeshes`:
   a weapon is appended as an `ActorCollisionMesh` when
   `!player.weaponCollisionDebug.valid || player.weaponCollisionDebug.usesJsonMesh`
   and `weaponColliderMesh` is non-empty. With `source:"boxes"`,
   `usesJsonMesh == true`, so the box triangles join the actor manifold.
2. `src/physics/movement/actor-triangle-solver.cpp`
   `solveActorTriangleCollision`: the legacy weapon sphere injection is now
   **skipped** when `player.weaponCollisionDebug.usesJsonMesh` is true
   (search for `weaponTriangleMode`).

So the weapon now contributes **sharp box triangles** to the actor solver.

## Suspected causes of "fall through / bounce strangely"

Investigate in this order. Do not assume; trace and measure.

1. **Sharp triangle normals on the weapon.** The actor solver's rounded feature
   shell exists because raw triangle edges/vertices produce hard "snag" normals.
   The body path is designed around this; a box weapon now presents 12 raw
   triangles whose edges may snag and eject the player. Check whether the weapon
   triangles get the rounded-feature response (they should, since
   `collectActorMeshContactsInto` is called with `contactSkin = -1`), and whether
   the box dimensions in `config/weaponcollisions.json` are too large.
2. **Weapon box size.** Attempt 12 approximated each weapon with one box, e.g.
   `projectile_rifle` box `center [0.75,0,0]`, `half_size [1.62,0.22,0.22]`
   (about 3.2 m long). If the box extends through the player or far past the
   model, the player is permanently in contact, which can wedge/launch them.
   Verify the box matches the visible weapon and does not intersect the player
   capsule.
3. **Transform mismatch.** The config geometry is placed by
   `player.weaponModelTransform` (see `collectActorCollisionMeshes`:
   `previousWeaponModelTransform` / `weaponModelTransform`), while the legacy
   capsule used `player.weaponCollisionWorld`
   (`rightArm.worldTransform * weaponLocalToArm`, built in
   `recomputeWeaponCapsule`). A large offset/rotation difference can put the box
   where the weapon is not. Compare the two transforms.
4. **Double contribution / synthesis.** Confirm `recomputeWeaponCapsule` still
   sets `player.collision.hasWeaponCollisionCapsule` in box mode (it should not)
   and that `collectBodyWeaponSpheres` is truly skipped for triangle-mode
   weapons. A leftover capsule plus the box would double the weapon.
5. **Missing rounded skin in the entity path.** The player-vs-entity side
   (`collectActorEntityContacts`) deliberately passes `contactSkin = 0` (exact
   triangles) because `-1` changed carry behavior. If the weapon box contacts a
   crate, it may behave differently than world contacts.
6. **`collides_with_world`.** Ensure the JSON entry does not accidentally allow
   the weapon to collide with the world in a way that drags the player.

## Key files

- `config/weaponcollisions.json` — the authoritative weapon hitboxes. Revert a
  weapon to `"source": "json"` + a `capsule`/`spheres` entry to get the legacy
  stable behavior for comparison.
- `config/collision.json` — `actorTriangleSolver`, `actorCollisionAccelerated`,
  `collisionSkin`, `edgeTouchTolerance`, `bounce`.
- `src/combat/weapon-collision-config.{h,cpp}` — parses the JSON; `applyCollisionConfig`
  builds `weaponColliderMesh` for `source:"boxes"` (search `buildBox`) and sets
  `usesJsonMesh`; builds debug spheres/capsules for `source:"json"/"capsule"`.
- `src/physics/movement/actor-collision-mesh.{h,cpp}` — `collectActorCollisionMeshes`
  (weapon append gate at ~line 209), `loadActorWeaponTriangles` (true GLB
  triangles, deferred), `commitActorCollisionMeshes`.
- `src/physics/movement/actor-triangle-solver.cpp` —
  `solveActorTriangleCollision` (~line 214), the `weaponTriangleMode` skip
  (~line 247), `runActorTriangleCollisionStep` (~line 561), and the legacy
  sphere injection.
- `src/physics/movement/physics-collision-mesh.cpp` — `collectActorMeshContactsInto`
  (the single narrowphase + rounded feature shell).
- `src/physics/movement/physics-collision-body.cpp` — `recomputeWeaponCapsule`,
  `collectBodyWeaponSpheres`, `collectBodyWeaponContacts`.
- `src/entities/player.h` — `weaponColliderMesh`, `weaponModelTransform`,
  `previousWeaponModelTransform`, `weaponCollisionWorld`, `weaponCollisionDebug`.

## Acceptance / tests

- `mimita.exe --actor-collision-mesh-selftest` (case 9 uses a raw weapon box).
- `mimita.exe --actor-triangle-solve-selftest`.
- `mimita.exe --moving-crate-selftest`, `--destructible-selftest`,
  `--destruction-replication-selftest`, `--destruction-stress-selftest`.
- Add/adjust a selftest that drives the weapon box against a wall and asserts the
  player does **not** pass through or get launched.

## Suggested approach

1. Reproduce with the JSON: set `projectile_rifle` to `source:"json"` (or a small
   `capsule`) and confirm the fall-through/bounce disappears. That isolates
   whether the box is the cause.
2. If the box is the cause, shrink/reposition it and confirm the rounded feature
   shell is applied. Also test with `config/collision.json`
   `actorTriangleSolver:false` to compare against the legacy path.
3. Prefer **reusing** the existing actor triangle path rather than adding a new
   weapon-specific one (AGENTS.md: one owner; reuse before adding). If the box is
   inherently too sharp, prefer a capsule/rounded representation or a smaller box
   over a new solver.
4. Do not regress the player-body collision or the destructible work; run the
   full selftest list above after every change.

## Constraints

- 60 Hz fixed tick; VSync off; cached broadphase; no per-query allocations in
  hot loops (`docs/architecture/collision/collision.md`).
- Keep one owner for collision; do not add a parallel weapon collision universe.
- Log every attempt in `docs/regressions/2026-10-01/physical-objects-collision-REG.md`
  and a changelog; separate source/build/runtime/human evidence.
- Never commit unless explicitly asked.
