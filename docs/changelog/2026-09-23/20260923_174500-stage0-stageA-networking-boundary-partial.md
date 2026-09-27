# Stage 0 + Stage A (partial): networking boundary foundation and dead-file removal

Date (UTC): 2026-09-23T17:45:00Z
Status: implemented; cold build and selftests verified; remaining Stage 0/A work listed below

## Scope

First session of the live-networking migration. Stage 0 adds the safe-reload
journal vocabulary and the capability ids later stages need. Stage A removes the
two files the plan agreed to delete (`server-melee.cpp`, `client.cpp`) and
updates the manifest. The whole-file hot relocation of `snapshot-chunks.cpp` and
`movement-validation.cpp` is **not** done in this session; the reason is recorded
under "Not done".

## Changes

1. **Deleted dead/legacy networking files.**
   - `src/network/server-melee.cpp` — inert: `handleMeleeHitRequest` returned at
     the top of the body, `tickServerSwordCombat` had no callers. Removed the two
     declarations from `server.h` and the `PACKET_MELEE_HIT_REQUEST` dispatch
     branch in `server-packets.cpp`. The packet id/schema stays for compatibility;
     an inbound melee request is now counted as unknown instead of silently
     swallowed.
   - `src/network/client.cpp` — legacy debug-only `--client` path, self-documented
     "NOT USED"; real multiplayer runs through `mpTick`. Removed the `--client`
     parse branch and usage line (`net_mode.cpp`), the `LaunchOptions::client`
     field and mutual-exclusion check (`net_mode.h`, `main.cpp`), the `runClient`
     declaration, and the stale GUI hint that told users to run `--client`
     (`gui-main.cpp`).

2. **Network journal vocabulary** (`src/live-code/network-journal-events.h`, new):
   central string constants for the migration's JSONL events
   (`network.reload.*`, `network.packet_*`, `network.reconnect_*`,
   `network.socket_preserved`, `network.connection_preserved`,
   `network.generation_activated`, `network.generation_rollback`,
   `network.shutdown_*`, `network.crash_boundary_missing`, ...). Strings only.

3. **Safe-point barrier records** (`hot-reload-system.cpp`): `tryActivateCandidate`
   now records `network.reload.barrier_entered` immediately before the module
   swap and `network.generation_activated` + `network.reload.barrier_released`
   after it; `rollback()` records `network.generation_rollback`; `beginBuild()`
   records `network.reload.requested`. This makes the existing implicit safe tick
   boundary explicit in the JSONL without changing when activation happens.

4. **New capability ids** (`game-api.h`, append-only): `clock.read`,
   `connection.read`, `connection.transition`, `net.snapshot-codecs`. No new
   `GameplayContextV1` field; providers register at runtime through the existing
   `resolveCapability` doorway.

5. **Manifest** (`hot-modules.json`): removed `src/network/server-melee.cpp` and
   `src/network/client.cpp` from `cold`.

## Evidence

- Source changes: `src/network/server-melee.cpp` (deleted),
  `src/network/client.cpp` (deleted), `src/network/server.h`,
  `src/network/server-packets.cpp`, `src/network/net_mode.h`,
  `src/network/net_mode.cpp`, `src/main.cpp`, `src/gui/gui-main.cpp`,
  `src/live-code/network-journal-events.h` (new),
  `src/hot-reload/hot-reload-system.cpp`, `src/hot-reload/game-api.h`,
  `src/hot-reload/hot-modules.json`.
- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  return code 0, `mimita-20260923T173317.exe` (560 s). A running `mimita.exe` was
  not touched.
- Hot DLL: `python build_game_dll.py` -> `DLL up to date, skipping` (the cold
  build's pipeline already rebuilt it against the new `game-api.h`).
- Automated tests (test evidence):
  - `--live-code-selftest` -> PASS (GameAPI load/ABI/self-test, all active
    modules, hot packet-codecs provider, hot round trip).
  - `--snapshot-chunk-selftest` -> PASS.
- Runtime / human acceptance: pending (no live edit observed this session).

## Not done (next sessions)

- **Stage A core: whole-file hot relocation of `snapshot-chunks.cpp` and
  `movement-validation.cpp`.** Both are safe by the relocation test, but their
  current APIs cross the boundary with STL (`std::vector<std::vector<uint8_t>>`,
  `std::string*`) and read `ServerPlayer`. A whole-file move therefore requires a
  POD capability API (`net.snapshot-codecs`, `net.movement.validate`) plus cold
  caller rewiring, not a file copy. Rushing it would either leak STL across the
  boundary or break the build, so it is deferred to a focused session.
- **Stage 0: move `pollAndAdvance` onto the listen-server tick thread.** The
  listen server ticks on a background thread while the main thread calls
  `pollAndAdvance` (`engine-tick-setup.cpp:84`); activating there races the tick.
  Fixing it needs a listen-server-active accessor reachable from the engine tick
  (the state currently lives as `gListenServer` in `gui-main.cpp`). Deferred so it
  is not a blind cross-module change.
- **Stage 0: ICE callback generation tokens + `mEvents` drain before unload.**
  `ice-agent.cpp` already queues callbacks and never enters hot code, so there is
  no live hazard today; the explicit token/drain hook needs a cold seam from the
  reload system to the ICE transports. Deferred.

## Notes

- `packets.h` remains in the manifest `cold` block; the combat-boundary doc
  already describes it as hot-editable. That contradiction is the Phase 6 target,
  not this session.
