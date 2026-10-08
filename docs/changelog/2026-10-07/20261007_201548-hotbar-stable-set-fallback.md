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

## Follow-up: community-set precedence over stale local duel state

### `src/terminal/weapon-commands.cpp:350-466`

The numbered equip commands previously selected `DuelWeaponPool` whenever
`gDuelManager.enabled()` was true. That allowed a stale or unintended local
duel flag to override an active network/community session, causing
`equipslot9` to report that the weapon was unavailable even though Stable set 1
listed the LMG.

The commands now use the community logical-set resolver for all numbered
weapon selection. The rejection text now says `active weapon set` instead of
incorrectly claiming every failure is a duel failure.

### Follow-up validation

- `python build_agent.py` succeeded; `src/terminal/weapon-commands.cpp` was
  compiled and the executable linked.
- `mimita.exe --versioninfo` succeeded and wrote
  `logs/10-07-2026/20261007_202657/events.jsonl`.
- The versioninfo journal contains the canonical logger start, identity, and
  stop events.
- Live dev-loop/community gameplay acceptance remains required.

## Follow-up: removed obsolete local duel weapon pool

The repository no longer contains a purely local duel weapon configuration.
Removed `config/duel-weapons.json`, the `DuelWeaponPool` implementation, its
startup and hot-reload hooks, and the equip-time allowlist gate. Network matches
now use the network-owned `config/weaponsets.json` path, including its Stable
set fallback.

### Cleanup validation

- No active source, config, or architecture-document references to
  `duel-weapons.json` or `DuelWeaponPool` remain.
- `git diff --check` passed; only existing line-ending normalization warnings
  were reported for working-tree files.
- `python build_agent.py` completed successfully and linked `mimita.exe`.
- The current link response contains no deleted duel-weapon-pool object.
- `mimita.exe --versioninfo` succeeded and wrote
  `logs/10-07-2026/20261007_202947/events.jsonl`; the journal contains the
  canonical logger start, version identity, and stop events.

## Follow-up: registered the configured Large Machine Gun

The Stable and Juggernaut weapon sets already referenced
`large_machine_gun`, and `config/weapons.json` already contained its complete
definition, but `WeaponData::registerBuiltinWeapons()` never created a registry
entry for that ID. Added a hitscan base definition at native slot 17 and let
the existing JSON overlay supply the LMG's configured stats, sound, and
automatic-fire behavior. This allows logical slot 9 to resolve to the LMG.

### Registration validation

- `python build_agent.py` succeeded and compiled the weapon registration and
  dependent weapon units before linking `mimita.exe`.
- `mimita.exe --versioninfo` succeeded and wrote
  `logs/10-07-2026/20261007_203531/events.jsonl`.
- Live acceptance remains: launch the rebuilt client/server and confirm the
  startup output includes `Registered: large_machine_gun (slot 17)`, then
  verify Stable slot 9 appears and equips.

## Follow-up: LMG model, pose, and sound presentation

- `config/weapons.json` now points the LMG at
  `assets/objects/weapons/mimita-lmg-v1.glb`.
- The LMG now uses its own `large_machine_gun` animation pose entry, copied
  from the shotgun for idle, shooting, just-shot, reloading, and equipping
  states. This removes the previous `pose_id: revolver` mismatch that could
  send the right arm into the wrong aim orientation.
- LMG sound pitch is `0.9` and sound volume is `1.5`, both controlled through
  the existing `weapons.json` sound fields.
- Both edited JSON files parse successfully, and the new GLB exists at the
  configured path.
- The build completed successfully with no source changes required for this
  presentation-only update. Live visual acceptance remains required.

## Follow-up: LMG weapon collision entry

- Added `large_machine_gun` to `config/weaponcollisions.json`.
- It uses the existing explicit long-gun box-collider convention from
  `hitscan_rifle`, with inherited actor bounce and the same collision skin.
- This makes the LMG use the configured weapon-collision path instead of the
  automatic model-derived fallback capsule during normal physical RMB aiming.
- Build validation completed successfully; live RMB aiming still needs to be
  exercised in-game.

## Preserved unrelated work

Existing changes in `config/accounts/default.json`, `config/analytics.json`,
`config/impact_decals.json`, and the pre-existing untracked collision-bounce
changelog were preserved.
