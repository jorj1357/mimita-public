# Weapon collision actor-bounce mode

## Change

- Added `bounce_mode` to `config/weaponcollisions.json`.
- The new default, `inherit_actor`, makes a weapon use the same actor/limb
  bounce response instead of applying a weapon-specific multiplier.
- Added `custom` for the existing `player_bounce` multiplier behavior.
- Added `none` for weapon contacts that block/project velocity without adding
  weapon bounce.
- Applied the mode in both the active actor-triangle solver and the legacy
  body/weapon fallback.

## Validation

- JSONC parsing passed for `config/weaponcollisions.json`.
- `python build_agent.py` completed with `BUILD SUCCESS` and return code 0.
- Built executable: `C:\mimita-v9\mimita.exe`.
- `mimita.exe --versioninfo` passed and reported
  `logs/10-07-2026/20261007_200850/events.jsonl`.
- Live wall-contact feel testing was not performed; human acceptance remains
  required for revolver, shotgun, and at least one long weapon in hybrid mode.
