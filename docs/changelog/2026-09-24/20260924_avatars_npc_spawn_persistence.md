# Preserve manual NPC spawn lifecycle origin

Time: 2026-09-24 EST

## Finding

`npc_spawn` inserted `ServerNpc::origin = GAME_NPC_ORIGIN_MANUAL`, but
`rebuildServerNpcMap()` created a new mirror record each tick without copying
that origin. The record therefore reverted to the enum zero value
(`GAME_NPC_ORIGIN_STARTUP`). Automatic reconciliation then removed it when
automatic startup NPCs were disabled, explaining the brief appearance,
missing health bar, and inability to damage it.

## Change

`src/network/server-npcs.cpp` now preserves `origin` and `startingWeapon` from
the prior mirror record during rebuild. This keeps manual NPCs alive and
authoritative across ticks.

## Validation

Source inspection and `git diff --check` are required. Full build/runtime
acceptance remains pending because the working tree still contains unrelated
pre-existing `server-npcs.cpp` lifecycle compilation errors reported earlier.
