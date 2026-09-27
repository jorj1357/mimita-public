# NPC startup, immediate respawn, and shared lifecycle

Date: 2026-09-24

## Change

- Community/listen-server defaults now start with no automatic NPCs; NPCs are
  created explicitly with `npc_spawn`.
- The shared hot respawn policy now uses the next fixed tick when respawns are
  enabled, rather than inheriting the server delay.
- Initial NPC creation now enters the same actor lifecycle envelope used by
  NPC respawn, keeping lifecycle initialization consistent across first life
  and later lives.

## Validation

- Hot DLL generation 29 built successfully:
  `build/hotreload/mimita-live-g000029.dll`.
- The live build did not modify `mimita.exe` or restart the running process.
- `git diff --check` passed; existing line-ending warnings only.

## Cold boundary

- The default startup behavior changes in `src/gui/menus/online-menu.cpp`,
  `src/gui/gui-main.cpp`, `src/network/server.h`, and the initial NPC bridge in
  `src/network/server-npcs.cpp`; those require the next canonical EXE build to
  affect a newly started server.
- The respawn policy change in `src/hot-reload/hot-respawn.h` is hot and is
  included in generation 29.
