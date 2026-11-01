# Deathball

**Status:** Desired behavior contract for the placeholder implementation.

## Purpose

Deathball is a slow, fading projectile weapon that passes through world
geometry. It is not an exploding rocket.

## Firing and ammunition

- **999 rounds per clip** (`magazine_size: 999` in the per-weapon config).
- **1.0 second fire delay** and **1.0 second reload**.
- One round is still fired per trigger pull; "clip" is a large magazine, not a
  burst.
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
- Player/NPC attacks must continue through the shared damage, knockback,
  hit-marker, damage-number, and health-update paths.
- World pass-through and actor contact are independent on the same tick. A ball
  that overlaps world geometry and an actor at the same time must still report
  the actor contact, apply that tick's damage and outward knockback, and also
  apply the world lifetime decay. A world overlap must never mask an actor
  contact.
- The authoritative contact point is the nearest point on the victim's body,
  not the center of the ball, so hit markers, impact effects, and damage numbers
  appear on the victim.

## Presentation

- Starts at RGB 0,255,255 (cyan) with alpha 1.0.
- Fades to RGB 10,10,0 with alpha 0.0 over its lifetime.
- Fade is driven by projectile age and is visible locally and for remote
  network projectiles.

## Scale: configured radius is not world-size proof

The per-weapon config currently sets `projectile_radius` and
`projectileVisualRadius` to 5.0. The engine consumes these as a world-space
radius, so the configured diameter is 10 world units. Do not assume that value
matches what players see.

Before changing target scale, measure and record the real runtime values:

- configured collision radius,
- effective visual radius and rendered diameter,
- player capsule height as the reference,
- local/offline hit radius,
- first expected/actual divergence.

The runner emits `deathball.config.resolve` at publication, and
`deathball.local_scale` on the first local update. If collision and visual
radii disagree, it emits `deathball.scale.first-divergence`. Use the dev loop
and the exact published executable's `events.jsonl`; a log line is not visual
acceptance.

The offline path must use the weapon's projectile radius for contact tests. It
must not keep a separate hardcoded player-sized threshold that disagrees with
the authoritative server.

## Acceptance

Runtime evidence must show 999-round clip behavior, 1.0 second fire delay,
1.0 second reload, slower travel, wall pass-through with faster lifetime decay,
edge/center damage, outward knockback, and the cyan-to-dark fade. Measure the
visual and collision scale before tuning it. Build success alone is not
acceptance.
