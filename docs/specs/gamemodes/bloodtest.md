# Bloodtest

**Status:** Shared actor body-part damage, replication, procedural skin/meat/bone
layers, part destruction, and force-scaled blood presentation are implemented.
Runtime and visual acceptance remain open until exercised in a real multiplayer
run.

## Purpose

Bloodtest is a close-quarters combat sandbox for testing direct weapon contact,
movement, blood presentation, and destructible geometry. It uses the shared
gamemode lifecycle and free-for-all damage rules. The first configured arena is
`mimita-duels-map-v3`; world destruction persists across matches and clears on
map reload.

## Mode contract

- Players use shared multiplayer spawning, respawning, inventory, damage,
  projectile, and match lifecycle systems.
- Players can damage one another. The mode does not use teams, a score target,
  or a match timer.
- `features.destructible_body_parts: true` enables shared player/NPC body-part
  health and destruction for this testing mode. Head/torso destruction is lethal.
- The mode selects the `bloodtest` movement preset and Bloodtest-specific
  projectile revolver and shotgun definitions, plus Swordsword.
- NPCs use the shared `beginner` combat behavior profile. Their health and
  damage remain controlled by the normal role and weapon owners.
- NPCs respawn on the first eligible fixed simulation tick after death through
  the shared respawn lifecycle. Bloodtest's `respawn_seconds: 0.01` is the
  minimum nonzero mode respawn timer; it is not a zero-delay same-tick revive.
- Map destruction is enabled and uses reset policy `never`, so a match boundary
  does not restore the map. A map reload uses the shared world reset owner.
- Blood presentation and client blood feedback are enabled through the existing
  presentation policy.

## Weapons

- The revolver fires one physical sphere from its actual muzzle position.
- The shotgun fires fifteen independently simulated, deterministic physical
  sphere pellets from the same muzzle position, using its configured spread.
- Both use the shared authoritative projectile and contact paths. Their
  configured mass and speed determine physical world impact; direct actor
  damage scales with impact energy relative to the weapon's configured impact
  profile.
- Swordsword remains the shared always-active physical blade contact weapon;
  contact follows its blade shape and does not require an attack press.

## Actor damage layers

Each visible body part has its own authoritative health in the shared actor body
state. Players and NPCs use the same six parts: head, torso, left arm, right arm,
left leg, and right leg. Each part starts at 100 health.

- Above two thirds health the part shows skin.
- At or below two thirds health the part exposes meat.
- At or below one third health the part exposes bone.
- At zero health the part is destroyed and disappears from rendering and hit
  validation.
- Head or torso destruction kills the actor.
- Arm or leg destruction removes that part while the actor may continue;
  destroyed parts remain gone on the corpse. A destroyed part is not left as a
  persistent physical limb.
- Part health resets on a new life and is replicated to every client through the
  shared snapshot path. Confirmed-damage events identify the damaged part and
  the resulting part state.
- Skin, meat, and bone are procedural nested shells generated from the existing
  part mesh; no separate authored layer geometry is required.

Blood and gore presentation scale with impact force and damage. Destroying a part
produces a larger spray, and surface splatter places larger decals near the
impact center with smaller decals toward the cone edges.

## Acceptance

1. Multiple human clients can join the arena and damage one another.
2. Point-blank muzzle contact is resolved by the physical projectile sweep.
3. Revolver shots and all shotgun pellets originate at the muzzle; pellets use
   deterministic spread and independent fixed-tick world/actor collision.
4. Faster impacts from the same configured weapon produce greater direct damage
   and stronger world impact inputs.
5. Swordsword damage follows its visible blade on any physical contact.
6. Arena destruction persists across match boundaries and clears after a map
   reload.
7. Blood presentation is visibly enabled in a real run.
8. Bloodtest NPCs resolve the `beginner` combat profile and respawn on the
   first eligible fixed simulation tick after death.
9. Part health crosses the meat, bone, and destroyed thresholds in a real run;
   head/torso destruction kills and limb destruction removes that part.
10. Both clients observe the same part health and visible layer.
11. Stronger impacts produce more and larger blood presentation than weak
    impacts, with splatter shrinking toward the cone edges.

Body-layer exposure, part destruction, replicated part state, and force-scaled
blood remain runtime and human-review acceptance items. Build and configuration
evidence do not prove runtime, visual, or multiplayer acceptance.
