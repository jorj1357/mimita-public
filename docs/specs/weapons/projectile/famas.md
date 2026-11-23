# FAMAS

**Purpose:** define the FAMAS as the weaker rifle in the initial Bloodtest
rifle trio.
**Audience:** weapon, projectile, networking, audio, animation, and gamemode
contributors.
**Does not:** prescribe unprovided numeric damage or add hitscan modes.

## Behavior

- The FAMAS is weaker overall than the AK-47 and M4A1. Balance can use damage,
  cadence, recoil, spread, capacity, projectile speed, or drop; the final mix
  remains weapon data rather than a separate combat system.
- Every shot is an individually simulated physical projectile with finite
  travel time and gravity-driven drop. Hitscan and instant beams are forbidden.
  Apply the shared [projectile firearm](projectile-firearms.md) authority and
  networking contract.
- Magazine/reserve, fire mode and cadence, reload, recoil, ballistics,
  presentation, and per-weapon equip/unequip time are JSON-configurable.
- World impacts use a bullet hole and bounded debris burst. Actor impacts use
  the shared Bloodtest blood/body feedback.

## Acceptance

Show lower overall combat output than both strong rifles under documented
comparison conditions, with visible drop and server-confirmed projectile hits.
