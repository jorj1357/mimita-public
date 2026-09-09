# Phase 2 NPC spawn routing

- Date: 2026-09-09
- Scope: Route direct authoritative NPC creation, map-change, and gamemode spawn paths through the shared lifecycle finalization.

## Change

- Added the shared NPC lifecycle adapter `finalizeServerNpcSpawn`.
- Routed authoritative NPC adoption, normal respawn, map-change spawns, and gamemode-start spawns through shared look/velocity/event finalization.
- Preserved existing NPC snapshot structures and terminal command entry points.

## Evidence

- Changed source: `src/network/server.h`, `src/network/server-npcs.cpp`, `src/network/server-gamemode.cpp`.
- All changed source files compiled successfully.
- Final link was blocked because `mimita.exe` remained open (`Permission denied`); no process was terminated.
- Full runtime testing is still required.

## Regression record

- No new regression entry appended.
