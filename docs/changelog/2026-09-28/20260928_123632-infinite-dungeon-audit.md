# Infinite Dungeon Slayer audit

- UTC timestamp: 2026-09-28T16:36:32.539Z
- Display time: 2026-09-28 12:36:32 EDT
- Branch: `afad20a-rebuild`
- Result: `NEEDS_SPEC_DECISION`

## Scope

Inspected the pasted Infinite Dungeon Slayer request before implementation. No
gameplay source, asset, configuration, packet, or build changes were made.

## Existing owners and evidence

- GLB maps are loaded by `loadWorldFromGLB()` in
  `src/world/world-gltf-loader.cpp:38`; it transactionally replaces the single
  `World` mesh, collision cache, broadphase, and spawn-point list.
- World geometry is ordinary `World::mesh` plus `World::collisionMesh` and
  collision indexes in `src/world/world.h:145` and
  `src/map/map-loader-collision.cpp:35`.
- Spawn markers are currently extracted from GLB node names containing
  `spawn` by `extractSpawnPointsFromGLB()` in
  `src/world/world-gltf-loader.cpp:148`; there is no room-definition loader.
- NPC creation uses `NpcSystem::spawnNpc()` in `src/npc/npc.h:214` and the
  existing safe-spawn path in `src/game/spawn-utils.cpp:45`.
- NPC death is marked by `DeathSystem::kill()` and processed by
  `DeathSystem::update()` in `src/combat/death-system.cpp:81` and `:316`.
  Server NPCs normally respawn through `simulateSharedNpcs()` in
  `src/network/server-npcs.cpp:1002`; there is no encounter observer or
  one-life room policy.
- No generic gameplay trigger or teleport action was found. Existing teleport
  code is developer tooling and console support, not an entity action.
- No generic barrier/door implementation was found.
- The map editor is currently a placeholder: `src/terminal/editor-commands.cpp:64`
  reports that `savemap` would save a map. The GUI editor is for GUI layouts,
  not world entities.
- Server world state is a separate `HeadlessWorld` loaded by
  `loadHeadlessWorld()` in `src/network/server.cpp:334` and `:901`. Snapshot
  replication carries actor transforms (`SnapshotEntity`) and NPC lifecycle
  epochs, but not procedural seed, room transforms, room state, or encounter
  membership.
- Console commands are registered in focused modules and wired from
  `src/main-systems.cpp:421` and `:565`; `src/terminal/terminal-commands.md`
  requires commands to reuse shared actions and provide explicit contracts.

## Proposed smallest coherent slice

After the required scope decision, create one procedural-world owner with three
plain data layers: a room definition/config loader, a generator that appends
ordinary GLB geometry and records transforms, and an encounter state that
observes existing NPC lifecycle. Register the six requested commands in a
focused terminal module. Keep the first slice local/listen-server compatible
only if the server receives the same generated-room state; otherwise add one
small authoritative room-state packet rather than a new world engine. Keep the
future barrier as a logical exit-lock state, with no renderer-specific door
type.

Likely files to create:

- `src/procedural/procedural-world.h/.cpp`
- `src/terminal/procedural-world-commands.h/.cpp`
- `config/procedural-world/rooms/basic_room.json`
- `config/procedural-world.json`
- `assets/maps/basic_room.glb` (when supplied)

Likely files to modify:

- `src/world/world-gltf-loader.h/.cpp` for safe transformed instance append,
- `src/main-systems.cpp` for command registration,
- one fixed-tick owner (`src/engine/engine-tick-state.cpp` or the server tick)
  for room progression and trigger evaluation,
- the narrow server/client packet files only if room state is made
  authoritative in this slice.

Keep untouched: existing duel/gamemode lifecycle, NPC movement/combat,
collision solver ownership, GUI editor, economy/progression, destruction,
and automatic map selection.

## Blocking decision

The repository has no `basic_room.glb` under `assets/maps/`, and the current
network contract cannot make generated rooms consistent for connected clients.
Using an existing map as a temporary room or implementing a client-only dungeon
would not satisfy the requested minimum proof. Human direction is required on
one of these choices: supply/identify the room GLB and accept the small room
state network extension, or explicitly authorize a local-only prototype using
an existing map asset.

## Documents and focused reviews read

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/ecs-entity-etc/ecs.md`
- `docs/architecture/ecs-entity-etc/ecs-migration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/skills/asset-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`

Focused results: specification review `NEEDS_SPEC_DECISION`; terminal command
review `NOT_APPLICABLE` to implementation; asset review `BLOCKED` by the
missing room GLB. No build or runtime evidence was produced because no source
changes were made.

## Pre-existing worktree state

The checkout already contained extensive deletions, modifications, and
untracked files before this audit, including changes under `src/`, `config/`,
`docs/`, and `devscripts/`. Those were preserved and are not attributed to
this audit.
