# Projectile Rifle Direct Bullet Behavior

Date: 2026-09-26

## Changes

- Added the rifle to the fixed-tick shared projectile simulation.
- Replaced rifle splash/explosion damage with one direct terminal hit for the collided player or NPC.
- Reused the revolver/shotgun weapon knockback fields for victim and shooter behavior.
- Disabled rifle blast knockback semantics and bypassed explosion-position reconciliation for rifle terminals.
- Increased projectile speed from 90 to 180.
- Kept `projectile_radius` at 0.1; server projectile collision reads this same weapon definition field.

## Evidence

- Source/config validation: JSON parses; rifle speed is 180, radius is 0.1, direct damage is 250, and shared knockback fields are present.
- Build: `python build_agent.py` completed successfully at 22:40:57 with return code 0.
- Runtime/human acceptance: not performed in this pass. A live room test is still required against both a player and NPC.
