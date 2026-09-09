# Phase 2 respawn-all routing

- Date: 2026-09-09
- Scope: Complete the remaining NPC `respawn_all` lifecycle routing.

## Change

- Added shared finalization for the authoritative `ServerNpc` mirror used by `respawn_all`.
- `respawn_all` now assigns NPC look direction, configured spawn velocity, and lifecycle event state together instead of resetting only health and epoch.
- Preserved the existing terminal command and response behavior.

## Evidence

- Changed source: `src/network/actor-lifecycle.h`, `src/network/actor-lifecycle.cpp`, `src/network/server-gamemode.cpp`, `src/network/server.h`.
- Build: `python build_agent.py` completed with `Status: SUCCESS`; 20 files compiled and `mimita.exe` linked.
- Runtime: repeated player/NPC spawn acceptance is still required.

## Regression record

- No new regression entry appended.
