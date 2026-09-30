// 2026-09-30T20:40:00Z
/* purpose
* record the procedural dungeon room-slot, teleport handoff, room-clear, and
* protected highest-room teleport fix
* does NOT claim live visual or multiplayer acceptance
*/

# Task

- Summary: Fix Infinite Dungeon Slayer `pwsids` room placement, prevent the
  first post-teleport movement report from moving or rubberbanding the player,
  and let a cleared room open the next room.
- Status: CODE_COMPLETE / BUILD_VERIFIED / RUNTIME_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`

# Changes

- `src/procedural/procedural-world-client.cpp`: client now renders inclusive
  room slots `0..generatedRooms`, matching the server's lobby slot 0 and combat
  room N slot N. Previously room 1 was rendered as slot 0 while the server
  teleported the player to slot 1.
- `src/network/movement-validation.cpp`: the first report with the new
  authoritative transform epoch is now treated as the teleport acknowledgement.
  Its accepted position is forced to the exact server target and its velocity
  and impulse are zeroed. Old transform epochs remain rejected.
- `src/network/server-npcs.cpp`: the per-tick NPC broadcast rebuild now
  preserves `proceduralRoomNumber`. Without this, every dungeon NPC became
  room 0 after the first tick, so room-clear logic could not find the dead
  room NPC and could not generate the next room.
- `src/network/server.h`, `src/network/server-packets.cpp`,
  `src/network/server-gamemode.cpp`, `src/network/server-players.cpp`, and
  `src/network/server-damage.cpp`: highest-room teleport now uses one
  authoritative teleport helper, resets movement state, restores max HP, and
  grants exactly 60 server-side fixed ticks of invulnerability.
- `src/procedural/procedural-world-client.cpp`: the client shows a pale
  turquoise elongated shell and `TELEPORT SHIELD: 60` down to `0`, using fixed
  client ticks and local rendering only.
- `src/procedural/procedural-world-client.cpp`: room doors are replaced when
  `currentRoom` changes. Room N's door is removed before room N+1's door is
  created, so the old door is never moved rapidly between rooms.
- The current room JSON spawnpoint is already inside the configured room bounds
  at `[-25.0, 0.0, 1.0]`; no user-edited value was overwritten.

# Evidence

- Earlier server logs showed `procedural_start` positions changing with the
  JSON spawnpoint and then immediately sending position corrections. One run
  showed teleport target `(69.00,1000.00,1.00)` followed by a correction from a
  stale/different client position. This supported a room-slot/teleport handoff
  bug, not a normal speed-limit rejection.
- `python build_agent.py` returned `BUILD SUCCESS`, compiled 5 translation
  units, skipped 498, and linked `mimita.exe`.
- The follow-up build returned `BUILD SUCCESS`, compiled 1 changed translation
  unit, skipped 502, and linked `mimita.exe`.
- The final build returned `BUILD SUCCESS`, compiled 2 changed translation
  units, skipped 501, and linked `mimita.exe`.
- The final follow-up build returned `BUILD SUCCESS`, compiled 1 changed
  translation unit, skipped 502, and linked `mimita.exe`.
- `git diff --check` reported only pre-existing whitespace warnings in the
  user-edited plan file.

# Human/runtime review still required

Launch the newly built executable, run `pwsids`, and confirm the visible room is
room 1, the player is at the JSON spawnpoint, and no immediate correction moves
the player away. Kill room 1's one NPC and confirm the barrier opens and room 2
spawns two NPCs. Run `procedural_world_teleport_highest` and confirm the player
lands exactly at the highest-room spawnpoint, cannot lose HP for 60 fixed ticks,
sees the shell and countdown, and then returns to normal damage. Confirm room N
spawns N NPCs and clears after the last one dies. Human/runtime review is still
required.

# Routed documents and focused skills

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/gamemodes/infinite-dungeon-slayer.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
