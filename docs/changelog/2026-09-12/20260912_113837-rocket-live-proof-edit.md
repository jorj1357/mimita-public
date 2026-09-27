# Rocket live-code proof edit

- EST timestamp: 2026-09-12 11:38:37 EDT (UTC 2026-09-12T15:38:37Z)
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Updated `src/hot-reload/modules/rocket-behavior.cpp` so the replaceable
gameplay module returns visibly different rocket policy values:

- `adjustRocketFlight`: `out->speedScale = 1.30f`.
- `explosionParameters`: `out->baseDamage = base->baseDamage * 1.50f`.

The EXE-owned rocket state, projectile authority, packet flow, and world state
were not changed.

## Validation

`python devscripts/live-build.py` passed and produced:

`build/hotreload/mimita-live-g000001.dll`

The live build reported that `mimita.exe` was never written. In-game proof is
still required: save/reload while the running game is active, observe faster
rockets and increased damage, verify PID/session/entity IDs remain unchanged,
then test a compile failure and rollback.

## Pre-existing edits

All unrelated working-tree changes were preserved.
