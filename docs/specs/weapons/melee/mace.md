# Mace

**Status:** Desired behavior contract for the placeholder implementation.

## Purpose

Mace is a heavy continuous-contact melee weapon for players and NPCs. It uses
the shared animated physical-contact owner and the shared authoritative damage
path.

## Behavior

- The mace is always a physical contact shape. Touching a target can damage it
  even when no attack is pressed.
- Idle contact damage is 1–5.
- The attack action has three phases:
  - 60 ticks windup
  - 30 ticks active
  - 90 ticks recovery
- During the active phase, contact damage is 25–125.
- Damage and knockback scale with contact force: a faster, more direct hit
  deals more damage and transfers more force.
- A primary click starts the swing. Player and NPC use the same action timing.
- The swing is usable on the ground and in the air by default.
- The mace is a large weapon and should read as heavy.

## Placeholder presentation

- Use `assets/objects/weapons/mimita-mace-v1.glb`.
- Reuse the HAFS sounds with 1.5x volume and 0.6x pitch, retriggered.
- Use procedural placeholder poses until dedicated authored mace animation
  clips exist.

## Acceptance

Runtime evidence must show idle 1–5 damage, active 25–125 damage, force-scaled
knockback, the 60/30/90 timing, and NPC use through the same action and contact
path. Build success alone is not acceptance.
