# Zombie Tower persistent NPC mode

- Timestamp: 2026-10-05 21:00:18 EST
- Branch: `afad20a-rebuild`
- HEAD at review: `51a63c5a`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Final behavior

Added a selectable `Zombie Tower` mode. It uses the existing human/NPC team
spawn filters: humans use spawn points whose names do not contain `NPC`, and
NPCs use spawn points whose names contain `NPC`, case-insensitively.

The mode has no time limit and does not transition to RESULTS when all current
NPCs are killed. After GO, the server waits the configured fixed-tick interval
and spawns a random NPC-team spawn. It never exceeds the configured living-NPC
cap. When an NPC dies, the next eligible interval refills the missing slot.

## Exact changes

- `config/gamemodes/zombie_tower.json:1-41`
  - Added the mode definition, two teams, no time limit, five-second
    intermission, three-second countdown, and JSON-owned persistent spawning:
    `max_count: 50`, `interval_ticks: 60`, `per_interval: 1`.
- `config/onlinemodes.json:76-83`
  - Added Zombie Tower to the community mode list.
- `src/gamemode/gamemode.h:220-223`
  - Added generic `npc_spawn` policy, maximum, interval, and per-interval
    fields to gamemode data.
- `src/gamemode/gamemode.cpp:392-401`
  - Parses the JSON `npc_spawn` block and clamps interval/count values to safe
    positive fixed-tick settings.
- `src/network/server-gamemode.h:234-255`
  - Added authoritative persistent-spawn state and next-spawn tick.
- `src/network/server-gamemode.cpp:604-620,3202-3213`
  - Resolves the JSON capability at match start and applies live JSON reloads.
- `src/network/server-gamemode.cpp:1173-1181`
  - Routes Zombie Tower actor resets through the team-aware spawn resolver.
- `src/network/server-gamemode.cpp:2631-2658`
  - Counts living NPCs, selects random NPC-team spawn points, caps the batch,
    and schedules the next spawn on the fixed 60 Hz tick timeline.
- `src/network/server-gamemode.cpp:3829-3967`
  - Integrates persistent spawning into WAITING, COUNTDOWN, GO, and ACTIVE
    without the wave-cleared RESULTS transition.
- `src/network/server-gamemode.cpp:4269-4274`
  - Extends the focused spawn-tag self-test to verify Zombie Tower and its
    exact JSON values.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/networking/networking.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/player-npc-systems/player-npc-systems.md`,
  `docs/operations/build-and-exe/build-and-exe.md`, and
  `docs/operations/task-completion/task-completion.md`.
- JSON parse for `config/gamemodes/zombie_tower.json` and
  `config/onlinemodes.json`: PASS.
- `python build_agent.py`: build status SUCCESS; affected objects compiled and
  executable build 1517 was produced.
- `.dev/builds/1517/mimita.exe --spawn-tag-selftest`: PASS.
- Scoped `git diff --check` for changed files: PASS. The repository-wide check
  still reports the pre-existing trailing-space line in
  `config/behavior-profiles.json:185`, which was not edited.

## Human review still needed

The actual `Zombie Tower 5` map asset is not present or registered yet. After
that map is added, run the mode and confirm the human starts on a non-NPC point,
NPCs use the 42 NPC-named points, the first spawn occurs after the configured
interval, kills refill the population, and the living count never exceeds 50.

