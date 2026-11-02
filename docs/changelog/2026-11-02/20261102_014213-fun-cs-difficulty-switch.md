# Fun Counterstrike difficulty switch

Time created: 2026-11-02T01:42:13Z
Time last updated: 2026-11-02T01:42:13Z

Status: IMPLEMENTED; CONFIGURATION VALIDATED; LIVE PLAY REVIEW NOT RUN

## Goal

Restore elite as the active Fun Counterstrike NPC difficulty and document a
one-setting way to switch to the previously requested easy tuning.

## Changes

- `config/behavior-profiles.json`: restored `fun_cs_rush` to the elite tier and
  added `fun_cs_rush_easy` with beginner tier, 25-degree aim error, a 120-tick
  minimum shot interval, and 0.1 optional dash chance. Both retain objective
  rush and the same formation settings.
- `config/gamemodes/funcounterstrike.json` remains set to `fun_cs_rush`, so the
  mode now uses elite difficulty.
- `docs/specs/gamemodes/funcounterstrike.md` and
  `docs/specs/npc-behavior/npc-behavior.md` explain that changing the mode's
  single `npc_behavior_profile` value between `fun_cs_rush` and
  `fun_cs_rush_easy` switches difficulty.

## Validation and acceptance

- Repository JSONC reader parsed both profiles and the mode config; assertions
  confirmed the elite active selection and easy profile values.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- No build or live run was performed because this change only selects between
  existing behavior tiers and profile data. Human gameplay review remains open.
- Specification/behavior review: the tier defaults would have been overridden
  if the easy profile's explicit aim, shot interval, and dash values remained
  on the elite profile. Separate variants avoid that conflict and allow a
  single mode-config field to switch between them.
