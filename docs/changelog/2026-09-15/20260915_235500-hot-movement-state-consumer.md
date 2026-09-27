# Hot movement state consumer

Date: 2026-09-15
Status: implemented phase 2

## Change

The hot `movement.main` module now consumes and updates the EXE-owned,
versioned movement runtime-state component. Grounded state, jump state, air
jumps, dash availability, dash cooldown, input edge latches, and freeze state
no longer live in DLL globals.

Shift and Q are edge-triggered in the hot module, and the hot module supports a
single air jump without requiring JSON configuration.

## Evidence

- `python build_game_dll.py --generation 999001 --changed src/hot-reload/modules/movement-system.cpp`: success.
- `python devscripts/run-movement-tests.py`: PASS (18/18, 21/21, 33/33).
- `git diff --check`: passed.

## Remaining

The cold player/server/NPC movement callers remain in place until the shared
hot movement entry point is migrated across those authorities. No claim of live
multiplayer acceptance is made in this phase.
