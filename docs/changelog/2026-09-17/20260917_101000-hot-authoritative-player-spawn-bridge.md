# Hot authoritative player-spawn bridge

Date: 2026-09-17

## Outcome

The player spawn handoff now preserves the server-owned authoritative
transform through the map-ready lifecycle and dispatches the existing generic
`actor.spawn-policy` hot event for player initial spawns and respawns.

## Evidence

- Source change: `src/network/server-players.cpp` captures
  `authoritativeTransformPosition` before the spawn reset, applies the hot
  spawn policy, and writes the resulting position before building the reliable
  spawn packet.
- The supplied runtime log showed the server selecting
  `(-604.89,28.29,2366.22)` and later emitting `[ACTOR SPAWN] pos=(0,0,0)`.
- `python devscripts/live-build.py --generation 999999` produced a valid hot
  DLL candidate and did not write or relink `mimita.exe`.
- `git diff --check -- src/network/server-players.cpp` passed.

## Boundary and remaining acceptance

The existing running executable must receive the one-time cold bridge through
an intentional no-process cold build. After that installation, edits to the
spawn decision in `src/hot-reload/modules/spawn-policy.cpp` can activate live
through the hot DLL. A live join and repeated `explode`/respawn trial remains
necessary to prove the final packet and visible position.
