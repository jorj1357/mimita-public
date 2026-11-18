# Duel NPC weapon cycle escaped its rocket-only loadout

- Recorded: 2026-11-18T06:36:49Z
- Status: SOURCE FIX SAVED — hot build observed in progress; runtime behavior pending
- Changelog: [20261118_064536-npc-loadout-weapon-cycle-rainbow-terminal-note.md](../../changelog/2026-11-18/20261118_064536-npc-loadout-weapon-cycle-rainbow-terminal-note.md)

## Expected behavior

The gamemode specification says that an explicit host-selected weapon set is
the effective loadout for every human and NPC. NPC behavior profiles can choose
how to use weapons in that loadout, but cannot add weapons to it. A Duel NPC
resolved to Rocket Launcher plus `nothing` should therefore not equip melee.

## Observed behavior and evidence

In run `20261118_062057`, an untracked Duel server process (PID 14176, launched
with `--mode duel --weapon-set 3`) wrote server events into the tracked
`client-01/events.jsonl` alongside the tracked Payload run. The Duel records
show:

- `gamemode.match.configured`: `mode=duel`, `weapon_set_id=3`,
  `weapon_set_source=host_override` at `2026-11-18T06:36:49.657Z`.
- `npc.roster-profile-resolved` for actor 200000: role
  `duel_monster_test_red_npc`, profile `elite_squad`, weapons
  `rocket_launcher`, `nothing`, at `2026-11-18T06:37:06.357Z`.
- `npc.grenade-availability` for actor 200000: equipped
  `rocket_launcher`, with only the resolved rocket and `nothing` entries,
  at `2026-11-18T06:37:10.511Z`.
- Immediately after, `npc.weapon-cycle` and `npc.weapon-switched` select
  `swordsword` (`cycle_start`); later events select `chainsaw`.

The active dev-loop run itself remains the separate Payload instance
(`server-01` PID 18992 and `client-01` PID 19804). This mismatch in process
ownership limits runtime claims to the separately identified PID 14176 Duel
server; it was not stopped or altered during this investigation.

## Cause in source

`src/npc/npc.cpp`, `NpcSystem::updateOneNpc`, built its cycle order by copying
the full `elite_squad.weapon_cycle` from `config/behavior-profiles.json`, then
appending loadout entries. It did not intersect the profile list with the
resolved actor loadout. `npcSwitchWeapon` accepts any registered weapon ID,
so the profile's `weapon_cycle_include_melee: true` allowed `swordsword` and
`chainsaw` despite their absence from the Duel NPC's authoritative loadout.

## Source change

The cycle order now starts with only profile-listed weapons present in the
resolved loadout, then appends the loadout's remaining entries. The profile
still controls tactics and visit lengths; the mode/role loadout controls which
weapons exist for that NPC.

## Validation still required

At the time of this record, `python devscripts/dev-loop.py --status` reported
`game-build status=building`; no agent build was requested or run. A subsequent
live Duel run must show actor 200000's cycle candidates and equipped weapon
remaining inside the Rocket Launcher set. The prior journal belongs to the
pre-change generation and does not prove the fix. Visual acceptance is also
pending.
