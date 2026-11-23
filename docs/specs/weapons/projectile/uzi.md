# Uzi

**Purpose:** define a very fast-firing Bloodtest projectile SMG.
**Audience:** weapon, projectile, networking, audio, animation, and performance
contributors.
**Does not:** authorize hitscan, client-authoritative damage, or skipping
projectiles to make the fire rate appear faster.

## Behavior

- Initial capacity is 30 rounds in the magazine and 180 in reserve.
- At the requested initial rate, an uninterrupted full magazine empties in
  about 0.5 seconds. The exact cadence is a JSON value and must be measured
  using the shared fixed-tick weapon scheduler.
- Every round is an individual physical projectile with finite travel time,
  gravity-driven drop, and its own authoritative contact. The visible muzzle
  flash/tracer is cosmetic and never substitutes for a projectile or hit.
- Local clients may predict projectile visuals immediately. Server contact,
  damage, ammunition acceptance, and kills remain authoritative. Batch network
  transport records and use shared pooled/cached simulation work as described
  by [projectile firearms](projectile-firearms.md); do not merge bullets into a
  damaging beam or send unvalidated client damage claims.
- Recoil, spread, damage, ballistics, sound, reload, and equip/unequip time are
  configurable. Prefer an existing in-repository Uzi-like automatic-fire sound
  or a newly authored similar report; do not assume an external San Andreas
  recording is available or licensed.
- World impacts use bullet marks and bounded debris; actor impacts use
  Bloodtest blood/body feedback.

## Acceptance

Verify 30/180 ammunition, the measured full-magazine firing duration, individual
projectile travel/drop/contacts, server reconciliation, no hitscan damage, and
stable tick/CPU/network behavior under concurrent automatic fire.
