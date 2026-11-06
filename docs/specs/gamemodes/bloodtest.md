# Bloodtest

**Status:** Initial multiplayer mode slice; biological layer destruction is
still pending an actor-body damage owner.

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
- The mode selects the `bloodtest` movement preset and Bloodtest-specific
  projectile revolver and shotgun definitions, plus Swordsword.
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

The intended visual model is nested skin, meat, and bone geometry beneath each
visible body part. Strong impacts should expose and break successive layers, and
extreme part damage should detach the part while an otherwise living actor can
continue. These behaviors are **not implemented in this mode slice**. The
existing actor representation has body-part meshes and contact hit locations,
but no authoritative part-health state, nested material geometry, or replicated
part-detachment state. Those must be added to the shared actor damage and body
replication owners before Bloodtest can claim this behavior.

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

Body-layer exposure, part health, gore fragments, and surviving limb detachment
remain separate acceptance items until their shared actor owners exist. Build
and configuration evidence do not prove runtime, visual, or multiplayer
acceptance.
