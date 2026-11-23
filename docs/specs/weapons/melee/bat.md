# Bat

**Purpose:** define a heavy, high-impact bat for Bloodtest's later melee and
Juggernaut variants.
**Audience:** weapon, melee, animation, physics, networking, and asset
contributors.
**Does not:** claim that a bat model or animation currently exists, or make the
bat part of Bloodtest's initial firearm loadout.

## Behavior

- The bat is a two-handed, heavy melee weapon. Its mass, grip, reach, contact
  shape, and impact response are authored weapon data.
- A swing has windup, active, recovery, and return-to-idle phases. Only actual
  weapon/body contact during the active phase may damage; damage and impulse
  derive from the shared authoritative contact/force path. The swing is
  dangerous and can be tuned for the later Juggernaut-versus-fighters variant.
- Authored animation guides the attack while the simulated weapon remains
  physical. Collision follows the actual weapon transform, allowing contact
  with objects outside the idealized animation arc when physics moves it there.
  See [momentum-driven two-handed melee](momentum-two-handed.md).
- Model, clips, sound, mass, damage, force, and phase durations are configurable
  content/weapon data. Add a bat model and clips through the existing asset and
  animation workflows; map-editor weapon-model authoring is not a prerequisite.

## Acceptance

Verify visible two-handed grip, phase timing, contact only during the active
window, force-scaled actor/world contact, shared player/NPC use, and stable
network contact authority. Human review must confirm the model and animation
read as a heavy bat.
