# Unified actor lifecycle foundation

- Date: 2026-09-09
- Scope: Begin the shared player/NPC lifecycle and gradual ECS migration.

## Changes

- Added the ECS direction and shared actor lifecycle contract to the architecture documentation.
- Made the player/NPC architecture explicitly require one lifecycle owner for first join, reconnect, respawn, NPC creation, gamemode changes, map changes, and terminal-triggered respawns.
- Added the spawn-velocity acceptance criteria for one lifecycle event containing position, look direction, velocity, generation, epoch, reason, and tick.
- Added transitional shared actor lifecycle types and a common authoritative spawn event diagnostic.
- Emitted the common lifecycle event at the player authoritative spawn packet boundary and NPC respawn boundary.
- Preserved existing player/NPC structs, terminal command names, packet formats, and network-specific adapters.

## Evidence

- Changed documentation: `docs/architecture/ecs-entity-etc/ecs.md`, `docs/architecture/player-npc-systems/player-npc-systems.md`, and `docs/features/spawnvelocity/spawnveloc.md`.
- Changed source: `src/network/actor-lifecycle.h`, `src/network/actor-lifecycle.cpp`, `src/network/server-players.cpp`, and `src/network/server-npcs.cpp`.
- Compilation reached the changed lifecycle source files successfully.
- Final link was blocked by `mimita.exe` being open (`Permission denied`); no user process was terminated.
- Runtime and full spawn-path acceptance are still required.

## Regression record

- No new regression entry appended.
