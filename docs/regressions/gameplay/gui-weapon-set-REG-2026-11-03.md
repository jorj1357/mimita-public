# GUI weapon-set override was replaced by a mode default

Time created: 2026-11-03T23:04:29Z
Time last updated: 2026-11-03T23:04:29Z

Status: ATTEMPTED FIX (1) — explicit cross-mode override validated in runtime;
Auto default and human GUI acceptance still need review

## Regression Occurrence 1

### Observed

The GUI exposed an explicit weapon-set selection, but a managed non-Sandbox
server could start with the selected gamemode's configured weapon set instead.
The server startup parser accepted `--weapon-set`, but the managed-mode branch
only applied it for Sandbox. The in-process listen-server path also called the
setter for Auto, converting Auto into set 1.

### Expected Behavior

Auto uses the selected gamemode's `weapon_set_id`. A concrete GUI or CLI set
overrides that default for human and NPC loadouts, attack authorization, and
respawns. TDM rules remain active when a different set is selected.

### Cause

`LaunchOptions` did not preserve whether `--weapon-set` was present, and the
managed-mode startup path did not apply the selected set. Role and actor presets
also had later opportunities to replace an explicit set.

### Fix Attempt

Preserved explicit selection from both GUI launch paths and CLI parsing. Applied
the host override before match setup for every managed mode and kept it over
role/preset resolution. Applied SSBM as the canonical gamemode ID while retaining
`melee` and `startmelee` compatibility aliases. Added the selected set to the
authoritative client spawn-application event and NPC profile-resolution event.

### Evidence

- Build through the dev loop succeeded at
  `2026-11-03T22:51:35.684979Z`; the required agent gate's canonical build and
  focused selftests also passed using artifact
  `2026-11-03T22:54:14.010800Z`.
- TDM Melee Only scenario passed on run `20261103_224237`, artifact
  `2026-11-03T22:40:16.368058Z`. Its journal recorded TDM rules, set 32 with
  source `host_override`, human loadout set 32, and NPC set 32 with
  `chainsaw`, `mace`, `swordsword`, and the `nothing` inventory placeholder.
- Run `20261103_225831` recorded client set 32 and rejected the attempted
  `rocket_launcher` equip. The later respawn command timed out in the scenario
  runner. A separate live journal, `20261103_224554`, contains subsequent
  `respawn loadout resolved` events with set 32 after `respawn_all`.
- Run `20261103_230304` recorded canonical SSBM match rules and set 32 with
  source `host_override`. The legacy `startmelee` command was rewritten to
  `startssbm` and the server emitted an accepted command result. The scenario
  runner timed out before recording command completion because the result
  arrived after its 15-second command-delivery window.
- `python devscripts/gamemode_json_sync.py --check` and
  `python devscripts/weapon_json_sync.py --check` passed. No visual GUI review
  or Auto-default runtime run was completed.

Related specification: `docs/specs/gamemodes/gamemodes.md`,
`docs/specs/gamemodes/supersmashbrosmelee.md`

Related issue: OGI-021 in `docs/specs/open-gameplay-issues.md`

Related changelog: `docs/changelog/2026-11-03/20261103_230429-ssbm-weapon-set-overrides.md`
