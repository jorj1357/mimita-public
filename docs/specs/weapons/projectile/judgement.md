# Judgment Revolver

**Purpose:** define the Judgment as a six-shot, exceptionally powerful Bloodtest
revolver.
**Audience:** weapon, projectile, networking, audio, animation, and gamemode
contributors.
**Does not:** promise a specific damage number, guaranteed one-shot kill, or
historical firearm accuracy.

## Behavior

- The revolver holds six rounds. It is exceptionally powerful and has a loud,
  distinctive report. Reload and reserve capacity remain explicit JSON tuning
  fields; the proposed staged T/R cylinder interaction is not part of this
  initial contract.
- Each shot is a physical ballistic projectile with finite travel time and
  gravity-driven drop. It never uses hitscan or an instant beam. Apply the
  shared [projectile firearms](projectile-firearms.md) authority and networking
  rules.
- Impact damage, falloff, recoil, muzzle velocity, gravity, spread, sound,
  reload, and per-weapon equip/unequip time are JSON-configurable. Extreme
  damage does not bypass authoritative contact or body-part damage rules.
- World impacts use bullet-hole/debris presentation. Actor impacts use shared
  Bloodtest blood/body feedback without HUD damage confirmation.

## Acceptance

Verify six-round behavior, loud firing presentation, substantial physical
recoil, finite travel/drop, server-confirmed contact, and live override behavior
in Bloodtest.
