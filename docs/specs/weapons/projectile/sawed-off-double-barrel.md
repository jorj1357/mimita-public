# Sawed-off Double-barrel

**Purpose:** define a wide-spread, high-damage, two-shot Bloodtest shotgun.
**Audience:** weapon, projectile, networking, audio, animation, and gamemode
contributors.
**Does not:** define pellet count or final damage before balance trials.

## Behavior

- The weapon is a sawed-off double-barrel shotgun based on the existing
  Bloodtest shotgun projectile behavior, with wider spread and very high
  close-range impact damage.
- Each trigger event launches the configured pellet projectiles from the
  physical muzzle with deterministic spread. Pellets have finite travel time,
  gravity-driven drop, and independent authoritative world/actor contacts; no
  hitscan cone, instant beam, or single aggregate damage ray is permitted.
- Firing mode, shells loaded, reserve, pellet count, spread, ballistics, damage,
  recoil, reload/stage timings, audio, and equip/unequip duration are per-weapon
  JSON values. Break-action reload choreography is a follow-on design choice.
- World impact creates bullet marks and a bounded debris burst, never a
  rocket/grenade explosion. Actor impacts drive Bloodtest blood/body effects.

## Acceptance

Show a wider pellet pattern than the existing shotgun at equal range, high
close-range damage, distinct pellet contacts, visible drop, and stable network
behavior during concurrent fire.
