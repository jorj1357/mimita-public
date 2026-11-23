# Glock 17

**Purpose:** define the Glock 17 as a weaker, higher-capacity Bloodtest pistol.
**Audience:** weapon, projectile, networking, audio, animation, and gamemode
contributors.
**Does not:** set unprovided recoil, cadence, or damage numbers.

## Behavior

- Initial magazine capacity is 20 rounds with 100 rounds in reserve.
- It is a weaker pistol than the Desert Eagle and Judgment revolver. Exact
  damage and cadence remain JSON balance values.
- Every round is a physical projectile with finite travel time and gravity drop;
  no hitscan/instant-beam path is allowed. Follow the shared
  [projectile firearm](projectile-firearms.md) contract.
- Recoil, ballistics, spread, reload, sound, and equip/unequip duration are
  independently tunable per weapon and may be overridden live by Bloodtest.
- Use non-explosive bullet-hole/debris world impacts and Bloodtest blood/body
  actor feedback.

## Acceptance

Verify 20/100 ammunition, weaker damage role, projectile drop, authoritative
hits, and preserved live weapon state during JSON tuning.
