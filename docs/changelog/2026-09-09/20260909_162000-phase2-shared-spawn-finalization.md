# Phase 2 shared spawn finalization

- Date: 2026-09-09
- Scope: Make the authoritative player and NPC spawn boundaries use one shared finalization rule.

## Change

- Added `finalizeActorSpawn()` to normalize the look direction, calculate configured spawn velocity from that direction, emit the shared actor-spawn event, and return the complete state.
- Player authoritative spawn packet creation now uses the finalized position/look/velocity state before sending the existing reliable packet.
- NPC respawn now uses the same finalization before publishing its existing snapshot state.
- Existing entry points, terminal command names, packet formats, and transitional structs remain in place.

## Evidence

- Changed source: `src/network/actor-lifecycle.h`, `src/network/actor-lifecycle.cpp`, `src/network/server-players.cpp`, `src/network/server-npcs.cpp`.
- Compilation reached all changed lifecycle sources successfully.
- Final link remained blocked because the existing `mimita.exe` was open (`Permission denied`); no process was terminated.
- Full routing of every gamemode/NPC creation path and runtime acceptance remain required.

## Regression record

- No new regression entry appended.
