# Hitscan rifle damage and knockback tuning

- time: 2026-09-29 20:39:44 EDT
- status: PASS_WITH_BUILD_AND_RUNTIME_PENDING
- scope: per-weapon hitscan damage scaling and victim knockback

## Changed

- Added `damage_scale` to `WeaponDefinition` and the shared hitscan damage calculation. It defaults to `1.0`, so existing weapons keep their current damage.
- Added `hitscan_rifle.damage_scale: 1.0` in `config/weapons.json`.
- Added rifle-specific victim knockback controls and set `victim_knockback_per_damage` to `0.01`.

## Evidence

- PASS: JSONC load and assertion for the rifle's `damage_scale` and `victim_knockback_per_damage` values.
- PASS: focused diff check for the changed damage/config files.
- NOT RUN: full build and live multiplayer acceptance because the active game process was preserved.
