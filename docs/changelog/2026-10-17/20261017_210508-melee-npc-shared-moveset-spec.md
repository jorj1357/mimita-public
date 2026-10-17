# Melee NPC shared moveset specification

- UTC: `2026-10-17T21:05:08Z`
- Scope: clarify the first Melee roster and require the single NPC to use the
  same Captain Falcon moveset as the human, including entry and respawn.
- Evidence reviewed: `config/roles.json` maps `falcon` to the `captain_falcon`
  weapon set and `captain_falcon_moveset`; `config/weaponsets.json` gives that
  set ID 12; `config/weapons.json` defines the moveset. The current scenario
  asserts one remote NPC, but it does not assert that the NPC has the correct
  role or equipped definition. Existing server code has role-resolved NPC
  loadout application, while the generic participant assigner can leave actors
  beyond declared role capacity without a role.

## Changes

- Expanded the Melee roster contract: one human plus one NPC, both assigned the
  `falcon` role and identical weapon set / equipped definition identity; no
  conventional gun fallback; prevent extra NPCs; reapply on respawn.
- Added the same player/NPC invariant and required live evidence to the Melee
  character/loadout section and the shared weapon specification.
- Updated the shared `startmelee` contract to require scenario assertions for
  active mode, actor counts, and matching player/NPC loadouts.

## Validation and status

- Documentation and active configuration were inspected; no code was changed.
- No build or live run was performed. The current scenario's one-NPC assertion
  does not yet prove the NPC's effective moveset. Implementation and live
  scenario instrumentation/assertions remain outstanding.
