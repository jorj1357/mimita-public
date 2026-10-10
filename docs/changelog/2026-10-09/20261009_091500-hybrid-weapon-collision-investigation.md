# Hybrid weapon collision investigation

Date: 2026-10-09
Status: investigation only; no source or configuration behavior changed.

## Request

Investigate why `config/aimbody.json` with `mode: "hybrid"` can fling the
player out of the map or strongly upward/downward when a weapon contacts world
geometry, while `mode: "default"` feels more consistent but less reactive.

## Source and ownership findings

- `config/aimbody.json:6-9` selects hybrid body physics and hybrid arm mode.
- `src/sim/simulate-tick.cpp:140-155` runs ordinary movement/collision first,
  then runs the physical aim body. Default mode deactivates that aim body.
- `src/ragdoll/ragdoll-mode.cpp:729-757` hybrid mode moves each child limb
  toward the procedural pose and damps its linear velocity. This creates a
  moving swept limb/weapon pose rather than only a visual pose.
- `src/physics/movement/actor-collision-mesh.cpp:201-218` submits body parts
  and the equipped weapon as movement-affecting collision meshes. The weapon
  sweep uses `previousWeaponModelTransform` to `weaponModelTransform`.
- `src/physics/movement/physics-collision-shared.h:95-188` converts the
  swept part velocity into `partInto` and, when it dominates, writes an
  outward velocity into the root player. The global collision bounce is
  enabled at strength `0.35` in `config/collision.json:29-37`.
- `src/physics/movement/actor-triangle-solver.cpp:534-554` and
  `src/physics/movement/physics-collision-glb-body.cpp:210-229` apply the
  weapon-specific mode only when the contact label is exactly `weapon`.
  Hybrid arm, leg, torso, and head contacts retain the global actor response.
- `config/weaponcollisions.json:12-18,25-30` documents that
  `inherit_actor` ignores `player_bounce`; most weapons currently use that
  mode. `large_machine_gun` is explicitly `none` at lines 155-162.

## Current conclusion

The strongest source-supported explanation is a two-part interaction:

1. Hybrid continually changes the physical arm and weapon transforms to follow
   animation/aim targets.
2. The collision response treats the resulting limb/weapon sweep as impact
   velocity and can transfer that velocity to the player root. Repeated hybrid
   pose correction can create repeated contacts, including vertical normals,
   producing the reported large upward/downward flings.

Changing aimbody mode to default removes the always-on physical aim-body step,
so the large hybrid pose-driven sweep is absent. That explains the improved
consistency without proving that the weapon collider itself is the only cause.

## Evidence boundary

No clean live player wall-contact journal was available in this investigation.
Existing journals contain hybrid NPC/aimbody records, but not the required
player contact sequence with weapon/body label, normal, penetration, sweep
velocity, and player velocity before/after response. Therefore this is a
source-confirmed causal hypothesis, not a runtime-confirmed reproduction or
fix.

## Next diagnostic gate

Run the real fixed-60-Hz game scenario with bounded structured records at the
existing collision and aim-body owners. Compare hybrid versus default while
recording mode, equipped weapon, contact label, normal, penetration, sweep
velocity, player velocity before/after, and the first position/velocity
divergence. Human feel and out-of-map acceptance remain open.
