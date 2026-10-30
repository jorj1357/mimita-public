# Rocket Duel and Juggernaut Horde implementation

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Timestamp: `2026-10-30T03:47:50Z` (UTC).
- Branch: `20261011zt`.
- Commits: none created; HEAD unchanged.
- Scope: this session implemented the two modes from the earlier intake and the requested Horde timer, countdown, victory rules, and endless match cycle. Unrelated working-tree edits were preserved.

## Changes

- Added selectable `rocket_duel` using weapon set 3 for the human and NPC, with standard Duel scoring, spawn and phase rules.
- Added selectable `jugger_horde` with one human Fighter, NPCs filling the remaining 63 Fighter slots, and four NPC Juggernauts. Fighters respawn after the minimum server delay; Juggernauts do not respawn during a match.
- Added shared per-team respawn delay and kill-heal percentage configuration. A Juggernaut kill restores 100% of its 3500 maximum HP, capped at maximum. Added a configured timeout winner that awards Juggernauts the match at 300 seconds if the team still has an actor in play. Elimination is checked before the deadline, so killing the last Juggernaut on the deadline awards Fighters the match.
- Added Horde HUD layout with a prominent top `SURVIVE m:ss` countdown, shared 3-2-1 / `GO!` presentation, named winner result, and next-round intermission. Removed Horde-only title and team labels after visual inspection showed they crowded the timer at 820x641.
- Added direct development launch choices 43 and 44, updated mode specs, and synchronized the new schema keys to existing canonical mode JSON files.

## Evidence

- `python devscripts/gamemode_json_sync.py --check` passed; Horde HUD JSON parsed; focused `git diff --check` passed. Repository-wide `git diff --check` reports whitespace in unrelated existing edits. `config_selftest.py` still fails on pre-existing avatar normalization data.
- The existing dev loop built and ran exact build 3846. Its `mimita.exe --versioninfo` was inspected and the live server/client journal is `logs/2026-10-30/20261030_034324/events.jsonl`.
- Rocket Duel live run in `logs/2026-10-30/20261030_033500/events.jsonl` resolved both the human and NPC to weapon set 3, Rocket Launcher plus `Nothing`, and entered ACTIVE with two participants.
- Horde live run in `logs/2026-10-30/20261030_033728/events.jsonl` resolved 64 Fighters and four Juggernauts, including 67 NPCs. Fighter NPC 200022 returned from 0 to 100 HP with transform epoch 1 to 2, then 2 to 3, after combat deaths. Juggernaut NPC 200064 healed from 1760/3500 to 3500/3500 on a confirmed kill.
- A temporary five-second timeout run in `logs/2026-10-30/20261030_034234/events.jsonl` awarded Juggernauts at tick 816 after ACTIVE at tick 516, exactly 300 fixed 60 Hz ticks, then started another ACTIVE match. The authored configuration was restored to 300 seconds and build 3846 launched with that value.
- In final build 3846, sending `npc_void` through the live game caused all four one-life Juggernauts to become dead. The server emitted `gamemode.match.team-eliminated` at tick 1508 with Fighter winner team 0, then entered results, intermission, countdown, and a new ACTIVE match at tick 2278. The same journal shows a Juggernaut kill healing from 3378 to 3500 HP.
- Captured the running game window after the HUD edit and directly saw the yellow `SURVIVE 1:54` text unobstructed at the top. The countdown code and phase transition were checked, but the 3-2-1-GO sequence was not directly observed on screen in this session.

## Remaining review

- Human gameplay review of Rocket projectile feel, 64-versus-4 balance, winner text, and the visible 3-2-1-GO transition remains open. Build and journal evidence do not constitute human acceptance.
