# Projectile Rifle Loadout Tuning

Date: 2026-09-26

## Changes

- Added `projectile_rifle` to weapon set 1 after Force Punch.
- Set the rifle magazine to 999 rounds.
- Set `reserveAmmo` to 99,999.
- Set `selfKnockbackMultiplier` to 0.04, reducing shooter blast knockback to one twenty-fifth.

## Evidence

- Source/config validation: passed; set 1 contains the rifle, clip is 999, reserve is 99,999, and self-knockback multiplier is 0.04.
- Build: `python build_agent.py` completed successfully at 22:37:03; nothing changed was required, return code 0.
- Runtime/human acceptance: not performed; live firing and shooter movement still need confirmation in a room.
