# NPC Waves human/NPC teams and tagged spawns

- Timestamp: 2026-10-05 20:51:08 EST
- Branch: `afad20a-rebuild`
- HEAD at review: `51a63c5a`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request and final behavior

NPC Waves now has two ordered teams: team index 0 / Team 1 is Humans and team
index 1 / Team 2 is NPCs. Humans use map spawn-point names that do not contain
`NPC` (case-insensitive). NPCs use map spawn-point names that contain `NPC`
(case-insensitive). Wave NPC creation and human reset/respawn use the same
team-aware spawn resolver used by Counter-Strike.

## Exact changes

- `config/gamemodes/npc_waves.json:5-20`
  - Before: no `teams` declaration, so NPC Waves had no mode-owned spawn
    groups or map-name filters.
  - After: declared `humans` with `spawn_tag_excludes: "npc"` and `npcs`
    with `spawn_tag_contains: "npc"`, using `human_spawn` and `npc_spawn`.
- `src/gamemode/gamemode.h:41-55`
  - Added optional `GamemodeTeam::spawnTagContains` and
    `spawnTagExcludes` fields.
- `src/gamemode/gamemode.cpp:175-188`
  - Loads the two optional team spawn-tag filter values from JSON.
- `src/network/server-gamemode.cpp:95-115,1016-1033`
  - Added case-insensitive, data-driven team spawn-tag matching and applied
    declared filters before the existing Counter-Strike CT/T compatibility
    classifier.
- `src/network/server-gamemode.cpp:1164-1175`
  - Treats NPC Waves as a team-aware spawn mode, so the human actor's Team 1
    spawn is selected during match reset and respawn.
- `src/network/server-gamemode.cpp:2598-2604`
  - Places each wave NPC through `gamemodeSpawnPoint(d, 1)`, selecting the NPC
    team cluster instead of the neutral shared anchor.
- `src/network/server-gamemode.cpp:4179-4202`
  - Extended the existing spawn-tag self-test with the four NPC Waves filter
    cases and verifies the JSON teams are present.
- `src/network/server-gamemode.h:92-95`
  - Updated the spawn-cluster ownership comment to cover mode filters and the
    legacy CT/T classifier.

## Documents and focused review

- Read `docs/ROUTER.md`.
- Read `docs/specs/gamemodes/gamemodes.md` and
  `docs/specs/networking/networking.md` for shared actor/team/server rules.
- Read `docs/skills/spec-behavior-review-v1.md`.
- Read `docs/architecture/player-npc-systems/player-npc-systems.md`.
- Read `docs/operations/build-and-exe/build-and-exe.md` and
  `docs/operations/task-completion/task-completion.md`.
- Read `docs/features/gamemodes/counterstrike.md` for the existing team-spawn
  implementation and reuse boundary.

## Validation evidence

- JSON parse: `config/gamemodes/npc_waves.json` — PASS.
- Canonical build: `python build_agent.py` — `BUILD SUCCESS`, one affected
  translation unit compiled, executable linked as build 1511.
- Focused runtime self-test:
  `.dev/builds/1511/mimita.exe --spawn-tag-selftest` — PASS.
- `git diff --check`: not clean because of a pre-existing trailing-space line
  in `config/behavior-profiles.json:185`; that unrelated file was not edited.
- No production deployment or active running match was changed.

## Human review still needed

Run NPC Waves on a map containing both ordinary spawn points and nodes named
with `NPC`, then visually confirm the human begins on an ordinary point and
every wave NPC begins on an NPC-named point. Build and self-test evidence do
not prove live map placement or visual acceptance.

