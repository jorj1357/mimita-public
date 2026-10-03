# Weapon collision sweep velocity response

## Change

- Converted actor-part sweep displacement to velocity before the shared
  collision response calculates bounce.
- Applied the conversion in both the active actor-triangle solver and the
  legacy body/weapon fallback, using the actual fixed substep `dt`.
- Preserved `config/weaponcollisions.json` `player_bounce` as the per-weapon
  multiplier; no weapon geometry or JSON tuning was changed by this fix.

## Evidence

Source:

- `src/physics/movement/physics-collision-shared.h`: added the shared
  displacement-to-velocity conversion helper.
- `src/physics/movement/actor-triangle-solver.cpp`: active weapon/body contacts
  now pass sweep velocity to the response.
- `src/physics/movement/physics-collision-glb-body.cpp`: legacy fallback uses
  the same conversion and receives `dt` from the collision owner.

Build:

- `python build_agent.py` recompiled the changed collision translation units
  without errors.
- Full build remains blocked by pre-existing errors in
  `src/engine/engine-tick-camera.cpp` involving `MimitaNet::ActorState` and
  unqualified duel phase constants.

Runtime and human acceptance:

- Not performed. Existing MiMITA processes were left running and were not
  stopped or replaced. A fresh executable run is still required to confirm the
  in-game wall rebound and repeated-contact behavior.
