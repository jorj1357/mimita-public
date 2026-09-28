// 2026-09-28T19:19:07Z
/* purpose
* record the first playable Infinite Dungeon Slayer procedural-room slice
* preserve existing server, NPC, collision, command, and packet owners
* does NOT claim runtime, multiplayer, visual, or human acceptance
*/

# Task

- Summary: Implement the first playable Infinite Dungeon Slayer slice: a
  server-authoritative procedural-room system that appends room GLB geometry,
  spawns a fixed encounter per room, unlocks the exit when cleared, and advances
  forever, with the client mirroring the server's door barrier.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / JSON_VALIDATED /
  RUNTIME_VALIDATION_REQUIRED / HUMAN_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T19:19:07Z ISO 8601 UTC; display timezone
  America/New_York (2026-09-28 15:19 EDT).
- Branch/commit: existing working branch; no commit created.

# Current owners and implementation

- `src/procedural/procedural-world.h/.cpp`: new owner of the procedural data
  model (room/mode definitions, `ProceduralRoomState`, `ProceduralWorldState`),
  the shared config loader, deterministic placement math, and the
  server-authoritative runtime (`serverProceduralWorldStart/Tick/Stop/OwnsNpc/
  TeleportTarget`). State lives in `ServerGamemodeState.procedural` so there is
  one source of truth.
- `src/procedural/procedural-world-client.h/.cpp`: new client application of the
  replicated state. Reconstructs the current room's locked exit as one static
  `PhysicalEntity` box barrier and removes it when the server unlocks the exit
  or disables the mode. No client-side room completion or generation.
- `src/network/packets.h`: `PROTOCOL_VERSION` 35→36; added
  `ProceduralRoomStateNetwork` and `ProceduralWorldNetworkState` (with `modeId`)
  and embedded one `procedural` section in the existing `DuelStatePacket`
  (measurement ~792 B, under `MAX_GAME_DATAGRAM_BYTES`). No new packet type or
  second replication loop.
- `src/network/server-gamemode.h/.cpp`: embedded `procedural` state and a
  `PendingProceduralRequest`; `serverGamemodeTick` applies queued requests and
  runs the procedural tick/broadcast before the `mapOnly` early-return;
  `broadcastDuelState` fills the procedural section; `serverCommunityMapStart`
  sets `matchMode = "sandbox"`.
- `src/network/server-packet-chat.cpp`: added host command branches
  `procedural_world_start <mode> [seed]`, `procedural_world_stop`,
  `procedural_world_generate_next`, and `procedural_world_teleport_highest`
  (teleport is not host-gated and uses the request path). Commands only queue
  intent; the fixed server tick, which owns the world/npcSystem, applies it.
- `src/network/server.h`, `src/network/server-world.cpp`,
  `src/world/world-gltf-loader.h/.cpp`: added safe transformed world-instance
  append/truncate and a factored `buildHeadlessCollisionChunks`; `ServerNpc`
  gained `proceduralRoomNumber`.
- `src/network/server-npcs.cpp`: the shared NPC respawn loop skips procedural
  encounter NPCs so a cleared room cannot repopulate.
- `src/terminal/procedural-world-commands.h/.cpp`: registered
  `procedural_world_help/start/info/generate_next/teleport_highest/stop`, wired
  from `src/main-systems.cpp`. Commands forward over the existing
  `PACKET_SERVER_COMMAND` path; `procedural_world_info` reads
  `CommunityMatchClient::procedural()`.
- `src/physics/physical-entity.h/.cpp`: added `PhysicalEntitySystem::remove(id)`
  for the client door; no new subsystem.
- `src/engine/engine-tick-combat.cpp`: calls `clientProceduralWorldTick()` before
  the fixed physics update, guarded by the same replay check.
- `src/network/community-match-client.cpp`: calls `clientProceduralWorldReset()`
  from `reset()` so a map change cannot leave a stale door handle.

# Configuration and coordinates

- `config/procedural-world.json`: mode `infinite_dungeon_slayer`, room
  `procedural_mimitasizing5`, `enemies_per_room 5`, `spacing 104.0`,
  `origin [0,1000,0]`, `axis [1,0,0]`, `door_half_extents [1.5,13,30]`.
- `config/procedural-world/rooms/procedural-mimitasizing5.json`: geometry
  `assets/maps/procedural-mimitasizing5-infdgnslr-v3.glb`, entrance
  `[-45,0,1]` facing +X, exit `[51,0,30.5]`, five floor spawns, bounds
  `[-52,-52,-1]..[52,52,103]`.
- Measured with a local GLB accessor/node-transform reader (no repo tool): the
  only door openings are on the ±X walls at `y∈[-12.5,12.5]`, floor `z=1`,
  lintel `z≈44.75` (stepped to `60.23`). Measured with the engine's `gravityZ`
  convention, Z is up, so progression was set to +X to match the openings. The
  earlier provisional config assumed a Y axis on solid walls and was corrected.

# Validation

- `-fsyntax-only` passed for every changed translation unit.
- `python build.py` completed the build; `build/mimita-game-live-16260-1.dll`
  (14:07) and `mimita.exe` (14:06) are newer than the last source edit (14:06),
  and the log shows `[BUILD] compiled ... 14:03:49` with no compile/link errors.
  The client booted to the main menu.
- JSON validation passed for both new config files.
- Runtime gameplay is not yet verified: no room was started, no NPC killed, and
  no door/next-room transition was observed in this session.

# Pre-existing work

The worktree already contained extensive unrelated deletions and modifications
(including `devscripts/`, docs, and other config) before this session; a
dev-loop process and game instance were also already running. Those were
preserved and are not claimed here.

# Required human/runtime review

- Start a listen/sandbox server, run `procedural_world_start
  infinite_dungeon_slayer`, and confirm the player is placed in room 1 and the
  +X exit barrier blocks movement.
- Kill the five room-1 NPCs and confirm the barrier clears, room 2 generates and
  populates, `procedural_world_info` advances `currentRoom`, and the process
  repeats.
- Confirm `procedural_world_teleport_highest`, `procedural_world_stop` (removes
  only procedural rooms/NPCs/barrier), and a map change leaves no stale door.
- No confirmed regression was identified in this session.

# Routed documents and focused skills

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/changelog/2026-09-28/20260928_123632-infinite-dungeon-audit.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
