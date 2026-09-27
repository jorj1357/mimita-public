# Collision logging, wall bounce, and limb-height settle

Date: 2026-09-23
Status: hot-code built; deterministic tests pass; human acceptance pending

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Per-limb collision logging

`src/hot-reload/packages/collision/collision-package-solver.cpp`
`collision.contact` now records, per contact, per tick:

```text
actor=<entityId> part=<capsule|head|torso|leftArm|rightArm|leftLeg|rightLeg|weapon>
tri=<world triangle index> target=<world|entity> point=(x y z) n=(x y z)
pen=<depth> incoming=<speed> resets=<0|1> grounded=<0|1>
```

The JSONL envelope already carries `entity_id`, `actor_id`, `actor_type`,
`simulation_tick`, `frame`, `server_tick`, and `client_tick`, so a record reads
like: actor 12345, limb rightArm, touched world triangle 29295, tick 29295.

Note: the collision mesh has no per-triangle material/block name
(`CollisionTriangle` is only a/b/c/normal), so the target is identified by
triangle index + contact point, not a block name. Mapping a point to a block
`texName` would need a cold capability; ask if you want that.

## Wall dash: no bounce and no reset

- No bounce: a wall **touching** contact was resolved by cancelling the
  into-surface velocity (slide) and never called the bounce response; only a
  penetrating contact bounced. Since the swept solver stops the capsule at the
  wall, a dash into a wall always slid. Now a touching non-ground contact uses
  `applyVelocityResponse`, so walls/ceilings bounce (afad20a
  `respondVelocityAgainstNormal`), while a settling ground contact still just
  cancels the into-ground component.
- No reset: the reset already fires on any contact
  (`st.collided = worldContact || bodyContact || contactCount > 0`, reset on
  `collided || grounded`). The new `collision.contact ... resets=1` records let
  you confirm the wall contact is actually produced; if it is not, the wall
  geometry is not reaching the narrowphase and the contact log will be empty.

## Idle "floating" grounded state

The limb collider was an oriented capsule whose endpoints spanned the full AABB
**plus** the radius, so its lowest point sat `radius` below the AABB's lowest
point. The ground settle then rested the capsule bottom on the floor, lifting the
visible body by the radius — the idle float (idle legs at rest, so it was
visible; moving poses shifted it). The capsule endpoints are now inset by the
radius (`localSegHalf = max(0, halfLen - radius)`), so the capsule's lowest point
matches the AABB's lowest point and the mesh rests on the ground consistently.

## Evidence

Build evidence:

- Hot DLL `DLL build success` (82 sources). No cold rebuild for any of these.

Test evidence:

- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

Human acceptance:

- Pending. Dash into a wall: expect a bounce and a `collision.contact` record with
  `resets=1`. Idle: expect the mesh to rest on the ground without floating.

## Limits

- World contact targets are triangle indices, not block names.
- The bounce strength/friction are `config/collision.json` `bounce` values
  (strength 0.35, friction 0.0); tune them live.
- Entity-vs-entity body contacts are resolved by the separate capsule-vs-capsule
  path, not this package, so `target=entity` is not produced here yet.
