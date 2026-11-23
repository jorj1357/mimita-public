# Horde NPC targeting, reload, and hit evidence

Session: `work-24fea8c536eb`  
Date: 2026-11-22  
Issue records: `issue-1b2463da5666`, `issue-be4fca689b0f`, `issue-79e68f63fb01`, `issue-80dfb9ab9c5c`

## User-visible report

The user reported that Horde NPCs stop attacking their human Juggernaut after live DLL reload, `team_pick 2` loses the Juggernaut role after reload, attacks/projectiles appear to pass through the player, dead NPCs seem to stop treating the player as an enemy over time, and a pinned Horde test failed before its server journal started. The user requested that these reports be visible to other agents and investigated with as much source and runtime evidence as possible.

## Source changes

- `config/npc-difficulty.json:14,70,85`: globally set `aimAtTargetMax=0` (continuous target-facing), `perceptionSightRange=1600` (previously 99), and `turnSpeed=1080` (previously 270). `src/npc/npc.cpp:961-987` implements target-facing and applies the configured turn limit. These are generic NPC settings, not Horde-only logic.
- `config/gamemodes/jugger_horde.json:237-242`: changed the targeting policy from `opposite_team` to the existing generic `player` policy while retaining both player and NPC candidates. `src/npc/npc-targeting.cpp:78-80` makes that policy prefer a hostile human and fall back to NPCs.
- `config/behavior-profiles.json:681`: raised `juggernaut_swarm.max_attackers_per_target` from 8 to 32. `src/npc/team-brain.cpp:398,418` caps focus attackers and allocates swarm approach slots using this value.
- `src/gamemode/gamemode.cpp:379-386`: made `GamemodeRegistry::pollReload()` lazily reload `config/gamemodes` if the hot DLL swap reset the registry's directory state. The pre-fix empty-directory early return prevented registry-backed lookups from recovering. This change compiled in game DLL generation 512; a post-swap `team_pick` runtime acceptance remains open.

## Build evidence

The shared dev-loop published game DLL generation 512 from the existing pinned EXE build `2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136`. DLL SHA-256: `f1f41079e3e2c7880e7c253e3a3e03a063ed5e271ca48b21dfe716b5f1b50424`. Build attempt `game-dll-20261122T183449310183Z-383c31` completed with exit code 0. This proves compilation/linking only.

## Runtime evidence

Managed pinned run `20261122_192457` used that EXE and generation 512. `server-01/events.jsonl:5018` records `team_pick 2` assigning player 1 to team 1 `Juggernauts`, role `juggernaut_mode_juggernaut`, weapon set `juggernaut_mode`.

After that assignment, journal inspection found:

- 30 `npc.shot` events targeting player 1; all were accepted rocket-launcher projectiles. First sampled shot: `server-01/events.jsonl:5176`; last sampled shot: line 9369.
- 63 distinct NPC actors with a visible attack gate targeting player 1.
- 136 `server.damage.applied` events to player 1 totaling 3,517 damage from 25 attackers: 122 physical-contact events plus 14 rocket-explosion events. First post-assignment damage appears at line 6298; the final sampled event is line 9847. This includes one post-assignment kill. A 150-damage rocket explosion at line 1550 occurred before team assignment and is excluded.
- The 30 launcher shots had target-angle error mean 41.58 degrees, median 17.32, and maximum 160.04 degrees. Several shots showed under 2-degree body-facing/shot mismatch while still having very large target-angle error; the currentFacing-based shot direction can therefore be well aligned with the model yet point away from the selected target.
- These rockets correctly report `hit_trace_available=false`; the damage journal proves some rocket impacts and melee/contact damage, but does not prove that a projectile intersected the player capsule without damage. Source paths `src/network/server-projectiles.cpp:837,903,1074,1189,1876` route projectile contact through the shared server contact resolver; `src/network/server-damage.cpp:223` records authoritative damage. A dedicated capsule-crossing capture is still required.
- A proof task `horde-player-priority-targeting-and-damage` (`task-proof-a31d7e4747dd`) verified the pinned run's `server.damage.applied` event. Its scope does not certify every acceptance clause or visual behavior.

The earlier managed run `20261122_181147` on generation 508 showed NPC sight capped at 99 m: an actor at 281.21 m had `target_visible=false`, while the same actor became visible at 98.61 m. A temporary generation-512 runtime later recorded visible targets at approximately 283–296 m, confirming the 1600 m setting is active. That temporary run could not accept `team_pick` through the runtime manager's command path.

## Reload and launch incidents

- The older live run `20261122_144609_01` recorded `team_pick 2` as team 1 `Juggernauts`, role `juggernaut_mode_juggernaut`, and weapon set `juggernaut_mode` before generation 500; after the live reload the same command used `BLUE` with empty role/weapon data. Evidence is in `server-01/events-0001.jsonl:866-867` and `events-0003.jsonl:15540-15543`. The lazy registry recovery fix is built, but cannot be runtime-verified by a test pinned to generation 512 because pinned tests intentionally do not hot-swap their DLL. The temporary runtime lacked a command-file interface, so post-swap role restoration is still unproven.
- Failed pinned attempt `logs/2026-11-22/20261122_155609/run.json` records server exit 1 and absence of `logger.started`. The unpinned retry was rejected because the EXE requires an explicit game generation. An older Horde run rejected `team_pick 2` because it was not a managed parallel test run. These are separately recorded in `issue-79e68f63fb01`.
- A dev-loop control endpoint outage was recovered and recorded under `issue-80dfb9ab9c5c`; unrelated online runs were preserved.

## Remaining evidence gaps

- The observed 30 shots and 63 visible-gate attackers show strong, recurring engagement, but do not prove every NPC fires continuously. Attack gates still suppress firing for `no_visible_target`, cooldown, weapon readiness, and reaction delay; target-loss-after-NPC-death was not captured as a per-actor lifecycle timeline.
- The mode-specific “player first” target preference is a configuration choice using an existing generic policy. The broader perception/turning and swarm limits are shared configuration/profile owners. Do not interpret the result as proof that all projectile types, all team modes, or normal Horde rosters have been accepted.
- Live DLL reload role recovery, client receipt for each damage event, physical projectile/capsule crossing, and human gameplay acceptance are not proven in this record.
