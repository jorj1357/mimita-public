# Rounded limb movement proxy investigation

- Date: 2026-09-28
- Scope: sharp actor-triangle edges, wall penetration, and limb snagging
- Status: triangle-path fix validated; rounded movement proxy deferred

## Result

- Kept the actor triangle solver authoritative for exact body hitboxes.
- Kept `collisionSkin` as a hot-reloadable query/recovery margin rather than
  turning shallow contacts into artificial physical penetration.
- Kept `edgeTouchTolerance` hot-reloadable in `config/collision.json`.
- Tested the existing sphere/capsule collector as a complete movement proxy,
  but it regressed floor grounding, wall stopping, crate blocking, and
  depenetration in the actor solver self-test. It is not enabled as the player
  movement owner.

## Validation

- `python build_agent.py`: SUCCESS; `Compiled: 1`, `Skipped: 487`.
- `mimita.exe --actor-triangle-solve-selftest`: PASS.
- `mimita.exe --moving-crate-selftest`: PASS.
- `mimita.exe --collision-selftest`: PASS.
- Live corner/wall acceptance remains open; no claim is made from these
  deterministic tests alone.

## Follow-up TODO

- Add a separate rounded movement proxy made from body-part capsules/spheres,
  with its own broadphase and floor/wall/depenetration tests, while retaining
  triangle contacts for exact hit/damage queries.
- Do not increase global skin to solve snagging; that changes physical depth
  and causes repeated bounce.
