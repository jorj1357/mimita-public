# SSBM gamemode rename and weapon-set override fix

Time created: 2026-11-03T23:04:29Z

## Scope

Implemented the accepted plan to preserve explicit GUI/CLI weapon-set choices
for managed gamemodes, use each gamemode's default for Auto, rename the
Captain Falcon gamemode to SSBM, and preserve legacy mode/command aliases.
The pre-existing Melee Only weapon set 32 was used as the explicit test set;
its earlier creation was not part of this session's source changes.

## Source changes

- Added explicit-versus-Auto state to launch settings and CLI parsing. Managed
  server and listen-server startup now apply only explicit selections, before
  match setup. Explicit sets remain authoritative through role/preset loadout
  resolution for humans and NPCs.
- Canonicalized the mode as `ssbm` / `SSBM` across config, menu metadata,
  mode-specific checks, profiles, and scenario expectations. Kept `melee` as
  a mode ID alias and `startmelee` as a `startssbm` alias. Generic melee
  weapon/action identifiers remain unchanged.
- Added structured set/source fields to match configuration, NPC loadout
  resolution, and client spawn-application events.
- Updated the gamemodes specification and OGI-021, added two focused runtime
  scenarios, and recorded the confirmed startup regression in the gameplay
  ledger.

## Build and checks

- Dev-loop build succeeded at `2026-11-03T22:51:35.684979Z`.
- `python devscripts/quality/agent-gate.py` completed RED. Its canonical build,
  focused selftests, weapon JSON sync, gamemode JSON sync, grenade scenario,
  and NPC wall-containment scenario passed. Remaining failures: lint reported
  557 repeated windows versus the existing 551 baseline; scenario recording
  roundtrip assertion failed; roundtrip helper reported a leftover server
  sidecar. No baseline was changed.
- `python devscripts/gamemode_json_sync.py --check` passed with 34 definitions;
  `python devscripts/weapon_json_sync.py --check` passed.
- Exact executable version output was captured from
  `C:\mimita-v9\.dev\builds\2026-11-03T23-02-31.420124Z\mimita.exe`;
  its printed `EVENTS_JSONL_PATH` was
  `logs/11-03-2026/20261103_180606/events.jsonl`.

## Runtime evidence

- TDM explicit-set scenario run `20261103_224237` passed. Its
  `events.jsonl` records `mode=tdm`, `weapon_set_id=32`,
  `weapon_set_source=host_override`, a human loadout on set 32, and an NPC
  profile on set 32 containing Chainsaw, Mace, Swordsword, and the `nothing`
  inventory placeholder.
- Run `20261103_225831` recorded set 32 applied from the server spawn packet
  and a rejected `rocket_launcher` equip. The scenario's later respawn command
  timed out; run `20261103_224554` independently records multiple respawn
  loadout resolutions retaining set 32 after `respawn_all`.
- Run `20261103_230304` recorded SSBM mode rules and the explicit alternate set
  32. It also recorded the server accepting `startssbm` after the legacy
  `startmelee` alias was entered. The scenario runner timed out after 15 seconds
  before it observed command completion; the server acceptance event arrived
  later in that same journal.
- Runtime Auto defaults (SSBM set 12 and TDM set 29) were verified against
  their active gamemode configs and the implemented selection path, but not
  exercised in a live run. No human visual GUI acceptance was performed.

## Worktree note

The worktree contained numerous unrelated user changes before this task.
They were preserved. This record describes only the weapon-set override and
SSBM changes made for the accepted plan.
