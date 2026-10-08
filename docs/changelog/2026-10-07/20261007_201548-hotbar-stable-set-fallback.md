# Stable-set hotbar and number-key fallback

Date: 2026-10-07 20:15 EDT (UTC 2026-10-08T00:15:48.905Z)
Branch: current working tree
Result: PASS_WITH_HUMAN_REVIEW

## Scope

Make the Stable weapons set usable through the normal hotbar and number-key
path even before a community weapon-set ID has been synchronized. The LMG
remains available at logical slot 9; Juggernaut role, NPC switching, and sound
behavior were not changed.

## Exact changes

### `src/engine/engine-tick-ui-game-hud.cpp:341-357`

Changed hotbar construction from only using `MP_CONTEXT.communityWeaponSetId`
when it was positive to using an effective set ID:

```cpp
const int weaponSetId = MP_CONTEXT.communityWeaponSetId > 0
    ? MP_CONTEXT.communityWeaponSetId : 1;
```

The hotbar now displays Stable set 1 while the network set ID is still zero,
so logical slot 9 resolves to `large_machine_gun` instead of falling through to
the native-slot 1-10 fallback that could not display native slot 17.

### `src/terminal/weapon-commands.cpp:85-109`

Applied the same effective-set rule to logical-to-native slot resolution and
weapon-to-logical attack-slot conversion. With no synchronized set ID,
`equipslot9` now resolves through Stable set 1 to the LMG's native slot 17.
If a configured set cannot resolve a slot, the existing safe fallback behavior
is preserved.

## Evidence

- `config/weaponsets.json` currently contains `large_machine_gun` at logical
  Stable-set slot 9 and `nothing` at slot 10.
- JSON parsing passed.
- `git diff --check` passed; only pre-existing line-ending warnings were
  reported for unrelated working-tree files.
- `python build_agent.py` succeeded and compiled the two changed C++ units:
  `src/engine/engine-tick-ui-game-hud.cpp` and
  `src/terminal/weapon-commands.cpp`.
- Built executable: `C:\mimita-v9\mimita.exe`.
- `mimita.exe --versioninfo` succeeded with build time `20:04:02` and wrote
  `logs/10-07-2026/20261007_201548/events.jsonl`.
- That journal contains `logger.started`, `versioninfo.executed`, and
  `logger.stopped`.

## Human acceptance still needed

Run the built client in the Stable weapons set and press `9`. Confirm that the
hotbar visibly includes the LMG at slot 9, the LMG equips, and the weapon can
fire. This session did not perform a live graphical gameplay run.

## Preserved unrelated work

Existing changes in `config/accounts/default.json`, `config/analytics.json`,
`config/impact_decals.json`, and the pre-existing untracked collision-bounce
changelog were preserved.
