# Add Large Machine Gun to Stable weapons

Date: 2026-10-07 19:59 EDT (UTC 2026-10-07T23:59:02.848Z)
Branch: current working tree
Result: PASS_WITH_HUMAN_REVIEW

## Scope

Added `large_machine_gun` to the existing host-selectable Stable weapons set
so it can be granted by weapon-set inventory construction and equipped for
live behavior testing. No Juggernaut, NPC weapon-switching, or sound code was
changed.

## Exact change

File: `config/weaponsets.json:5-6`

- Old description ended with `Big Shotgun (8), Nothing (9).`.
- New description ends with `Big Shotgun (8), Large Machine Gun (9), Nothing (10).`.
- Old weapons array ended with `"big_shotgun", "nothing"`.
- New weapons array ends with `"big_shotgun", "large_machine_gun", "nothing"`.

The Stable set therefore grants the LMG at logical slot 9. The existing
community logical-to-native slot resolver maps that entry to the LMG's native
weapon slot 17.

## Investigation-only findings

- The active Juggernaut mode team references role
  `juggernaut_mode_juggernaut` in `config/gamemodes/juggernaut.json`.
- That role references `weapon_set: juggernaut_mode` and
  `starting_weapon: large_machine_gun` in `config/roles.json:34-42`.
- `config/weaponsets.json:75-79` already defines `juggernaut_mode` as
  `large_machine_gun`, `hitscan_rifle`, and `nothing`.
- The older generic `juggernaut` role at `config/roles.json:13-18` still
  references the legacy `juggernaut` set, which contains rocket launcher and
  shotgun. This is a role distinction, not a change made in this session.
- NPC weapon selection in `src/npc/npc.cpp:1281-1390` may switch between
  usable weapons in the role loadout according to scored distance and behavior
  policy. `npcSwitchWeapon` does not intentionally toggle to Nothing; repeated
  LMG/Nothing presentation would require runtime evidence from the real actor
  path.
- The current client network presentation selects
  `weapon/machinegun/machinegunshoot` for
  `NETWORK_WEAPON_LARGE_MACHINE_GUN` in `src/engine/engine-tick-net.cpp:685-707`.
  A wrong sound in a live run therefore remains unverified as either stale
  executable/configuration, an incorrect event weapon ID, or an audio-runtime
  issue.

## Validation

- `config/weaponsets.json` parsed successfully with PowerShell JSON parsing.
- `git diff --check` found no whitespace errors in the changed file; it also
  reported only pre-existing line-ending warnings for other working-tree
  files.
- No build was run because this is active JSON configuration only.
- Existing unrelated working-tree changes were preserved:
  `config/accounts/default.json`, `config/weaponcollisions.json`, and the
  pre-existing untracked collision-bounce changelog.

## Human review still needed

Launch a server using Stable weapons set 1, equip logical slot 9 (or native
slot 17 through the relevant command path), and verify the LMG can be equipped,
fired, reloaded, and heard. Runtime evidence is still needed for the separate
Juggernaut-role/NPC switching/sound symptoms.
