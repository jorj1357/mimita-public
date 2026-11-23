# Desert Eagle

**Purpose:** define the Desert Eagle as a powerful, high-recoil Bloodtest
pistol.
**Audience:** weapon, projectile, networking, audio, animation, and gamemode
contributors.
**Does not:** set final damage, fire cadence, or ammunition values that have
not been chosen.

## Behavior

- The Desert Eagle is a strong pistol with pronounced physical recoil. Recoil
  moves the held gun/arms and recovers through the shared aim-body system; it
  does not fake accuracy with a camera-only shake.
- Every round is an individual physical projectile with finite travel time and
  gravity drop. Hitscan and instant-beam damage are forbidden. Follow
  [projectile firearms](projectile-firearms.md).
- Damage, impact profile, ballistics, spread, magazine/reserve, cadence, reload,
  sound, and equip/unequip timing are mode-overridable JSON fields.
- Use a distinct, forceful firing report and shared non-explosive world impact
  and Bloodtest actor feedback.

## Acceptance

Verify high physical recoil, projectile drop, server-confirmed hits, and live
JSON tuning without resetting ammunition or reload state.
