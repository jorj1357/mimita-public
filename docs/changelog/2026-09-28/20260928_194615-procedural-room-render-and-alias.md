// 2026-09-28T19:46:15Z
/* purpose
* record the short Infinite Dungeon Slayer command alias, client room-render
* integration using the existing GLB instance loader, and server-authoritative
* developer flight controls
* does NOT claim live gameplay acceptance
*/

# Task

- Summary: Add `pwsids` as an alias for starting Infinite Dungeon Slayer, make
  the configured procedural room GLB visible on the client, use the v4 asset's
  Blender-authored spawnpoint, increase enemies one per room number, and add
  `fly <multiplier>` / `unfly`.
- Status: CODE_COMPLETE / BUILD_VERIFIED / RUNTIME_VALIDATION_REQUIRED
- Date, time, timezone: 2026-09-28T19:46:15Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-28 15:46 EDT).

# Changes

- `src/terminal/procedural-world-commands.cpp`: registered `pwsids`, which
  sends the existing authoritative command
  `procedural_world_start infinite_dungeon_slayer`.
- `src/procedural/procedural-world-client.cpp/.h` and
  `src/engine/engine-tick-combat.cpp`: the client now loads the configured room
  GLB through `loadWorldTemplate`, appends generated room instances with the
  existing `appendWorldInstance` owner, and removes them on stop/reset. The
  server remains authoritative for room state and NPC lifecycle.
- `src/procedural/procedural-world.cpp`: server start extracts the GLB node whose
  name contains `spawnpoint`, transforms it into the selected room slot, and
  uses it for the authoritative player teleport. Encounter size is now the
  current room number, so room N spawns N enemies.
- The configured asset exists at
  `assets/maps/procedural-mimitasizing5-infdgnslr-v5.glb` and the room config now
  points to it.
- `src/terminal/player-commands.cpp`: added `fly 10`, `fly 1`, `fly 0.1`, and
  `unfly`; WASD moves horizontally, E/Q move up/down, and Shift uses the fast
  modifier.
- `src/network/server-packet-chat.cpp`, `src/network/server-players.cpp`, and
  the existing movement-input path: the server owns fly permission, multiplier,
  and final position; clients send only movement buttons and reconcile to the
  authoritative result.
- Follow-up fix: fly now advances the local movement simulation tick and uses
  the camera-relative world vector that `pollInput()` already produces; the
  client and server no longer rotate that vector twice.

# Validation

- `python build.py build-only` completed with `BUILD SUCCESS` and linked
  `mimita.exe`.
- The same build compiled the fly command and movement/network changes and
  completed with `BUILD SUCCESS`.
- The follow-up rebuild is currently blocked by a pre-existing unrelated
  compile error in `src/impact/destructible-geometry.cpp:201` (`overlaps` is
  not declared). No change was made to that unrelated file.
- The prior normal build attempt could not launch because another
  `mimita.exe` held the file; this did not prevent the build-only link.
- Runtime validation remains open: restart `dev-loop.py`, run `pwsids`, confirm
  the player is placed on the GLB spawnpoint, visible room geometry appears,
  room N has N NPCs, hits register, and clearing the room immediately creates
  room N+1. Also confirm `fly 1` / `unfly` in the live host session.

# Asset-transform note

- The GLB loader already applies node transforms. Blender objects should
  normally be exported with intentional room-local geometry and sane applied
  scale/rotation; the room's local origin and the JSON entrance/spawn points
  must agree. Do not blindly move all geometry to world origin if the JSON uses
  a different room-local coordinate frame.
