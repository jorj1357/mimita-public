# NPC avatar live-bind repair

Timestamp: 2026-09-24 EST

## Result

Updated the NPC avatar path so avatar identities fit the existing 15-byte
network field and client replicas retry their NPC-specific avatar bind after
background avatar metadata becomes ready. This prevents truncated avatar names
from falling back to the white body and prevents NPCs from retaining the local
player avatar such as Jason.

## Files

- `src/npc/npc-avatar.cpp`: filters discovered NPC avatar names to the current
  wire capacity and logs skipped names.
- `src/network/multiplayer-tick.cpp`: rebinds NPC replicas from snapshot avatar
  identity on every snapshot when the cached avatar is ready.

## Evidence

- `git diff --check` was run for both changed files.
- The build reached compilation and found no error in the avatar changes after
  the local alias correction. The full build remains blocked by pre-existing
  dirty-tree errors in `src/network/server-npcs.cpp` (`GameNpcLifecycleFn` and
  `SpawnPoint::yaw`).
- Live visual acceptance is still required.

## Hot-reload boundary

The selector remains hot policy code. The network snapshot binding change is in
the EXE-side client path, so it requires a cold client build/restart; moving
that binding into the hot presentation/runtime boundary is future migration
work.
