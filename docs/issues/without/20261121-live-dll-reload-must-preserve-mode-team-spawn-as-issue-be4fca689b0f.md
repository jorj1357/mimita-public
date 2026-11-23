# issue-be4fca689b0f: Live DLL reload must preserve mode team spawn assignments

## Human report (verbatim, immutable)

> dll reloads make it so gamemodes dont respec tthe team spawn positions, issue


## User quote

### 2026-11-22T17:06:19.636Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Reproduced in the existing live Horde run 20261122_144609_01. Before reload, the same host team_pick 2 assigned Juggernauts role juggernaut_mode_juggernaut; after generation 500, the command returned BLUE with empty role and weapon set. Evidence: logs/2026-11-22/20261122_144609_01/server-01/events-0001.jsonl:866-867 and events-0003.jsonl:15540-15543. This gives the older team/spawn reload report a concrete same-mode role/team regression to investigate; the exact state-transfer cause and spawn consequences remain unproven.

## User quote

### 2026-11-22T17:41:04.100Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Confirmed same-server before/after DLL reload differential, run 20261122_144609_01: before gen500, team_pick 2 assigned player1 team1 Juggernauts with role juggernaut_mode_juggernaut and weapon set juggernaut_mode (server-01/events-0001.jsonl:866-867). After gen500 loaded, the same command recorded team1 but team_name=BLUE, empty role/weapon set, and reply 'switched to BLUE' (events-0003.jsonl:15540-15543). Module probe 'policy not migrated' appeared before and after, so that probe alone is not causal. The postreload NPCs still targeted and fired at player1; this is independent of team metadata corruption. Root cause in reload rehydration/config preservation is unresolved; no fix built/proven.

## Assignment

### 2026-11-22T18:10:13.565Z — horde-behavior-evidence

Owner: horde-behavior-evidence

## User quote

### 2026-11-22T18:10:16.714Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Source trace for same-run postreload team_pick regression: serverRequestTeamChange resolves the current GamemodeRegistry entry, then roundTeamName uses gm.teamNames and roundTeamRoleId returns empty when gm.teams is empty (src/network/server-gamemode.cpp:3208-3222, 10357-10396). GamemodeRegistry::get returns a default Gamemode when its loaded modes vector has no entry (src/gamemode/gamemode.cpp:1090-1096); GamemodeRegistry::pollReload returns immediately when directory_ is empty (src/gamemode/gamemode.cpp:379-381). The per-tick server poll at server-gamemode.cpp:8577 calls pollReload but cannot repopulate a new DLL generation's empty static registry. This matches the same-process log changing from Juggernauts/role to BLUE/empty role after gen500 activation. This is a source-supported root-cause candidate; a post-reload pinned runtime after repair is still required.

## User quote

### 2026-11-22T19:27:08.803Z — horde-behavior-evidence

Via horde-behavior-evidence; supports runtime:

> Pinned managed run 20261122_191248 (EXE SHA 4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136, DLL gen512 SHA f1f41079e3e2c7880e7c253e3a3e03a063ed5e271ca48b21dfe716b5f1b50424): team_pick 2 completed at tick 2129 and assigned player 1 to team 1 Juggernauts, role juggernaut_mode_juggernaut, weapon set juggernaut_mode (server-01/events.jsonl line 6421). Before the follow-up data change, run totals showed 54 NPC shots aimed at player 1, all deathball/rocket projectiles (42/12), zero hit_entity/hit_world trace; mean target-vs-shot angular difference 28.55 degrees, median 16.79, max 168. At least four weapon.contact.result sword hits dealt 126 total damage in the sampled journal, and 85 server damage-policy checks were allowed after team assignment. The same run observed target-visible attack-gate distances through 293.86 m with the new global 1,600 m sight range. Source explanation: config/gamemodes/jugger_horde.json selected npc_targeting.mode=opposite_team with players and NPCs included, while generic src/npc/npc-targeting.cpp Player mode explicitly prefers a hostile human before any NPC; config/behavior-profiles.json juggernaut_swarm capped attackers at 8 and src/npc/team-brain.cpp uses that cap for attack approach slots. I switched the data to generic player-priority mode and raised the swarm profile cap to 32; new pinned run 20261122_192457 is exercising those changes.
