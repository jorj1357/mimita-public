# Deathball

**Status:** Desired behavior contract for the placeholder implementation.

## Purpose

Deathball is a slow, fading projectile weapon that passes through world
geometry. It is not an exploding rocket.

## Firing and ammunition

- One round in the clip.
- 1.0 second reload.
- Projectile speed is 9 m/s (5x slower than the current rocket launcher speed).
- Projectile lifetime is 10 seconds.
- Use `assets/objects/weapons/mimita-deathball-v.glb`.
- Use the raygun equip, reload, and shoot sounds.

## Movement and wall interaction

- The deathball passes through walls instead of exploding or bouncing.
- While the projectile is intersecting world geometry, its lifetime decays at
  3x the normal rate on that tick.
- World contact does not create an explosion.

## Damage

- There is no explosion.
- On each tick that the ball intersects a player, apply one damage event:
  - near the outer edge of the ball: 1 damage
  - near the center of the ball: 10 damage
  - interpolate between those values by the contact distance
- Knockback is 0.1 per point of damage applied on that tick, pushing outward
  from the victim's origin.
- Damage repeats on later ticks while the ball continues to intersect the
  victim.

## Presentation

- Starts at RGB 0,255,255 (cyan) with alpha 1.0.
- Fades to RGB 10,10,0 with alpha 0.0 over its lifetime.
- Fade is driven by projectile age and is visible locally and for remote
  network projectiles.

## Acceptance

Runtime evidence must show one-round firing, 1.0 second reload, slower travel,
wall pass-through with faster lifetime decay, edge/center damage, outward
knockback, and the cyan-to-dark fade. Build success alone is not acceptance.
