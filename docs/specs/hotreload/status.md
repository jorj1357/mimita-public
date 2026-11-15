This status document is deprecated. Current status is generated at `build/hot-reload/status.json`. The current specification is [hotreload.md](hotreload.md). Historical status is archived at `docs/archive/hotreload/status-2026-11-14.md`.

## 2026-11-14 packet domains, authored worker, state domain foundation

- Packet structs split into session, snapshot, combat, player, gamemode,
  physical, and vehicle headers; `packets.h` remains the umbrella.
- The resident packet fallback and duplicate `dispatchInboundMessage` packet
  switch are gone. `runServer` refuses to start without the hot server API.
- `net_common.cpp` now contains pure helpers; WinSock ownership moved to
  resident `src/host/net-common-host.cpp`. Agent A must re-own the pure file.
- DLL-only `src/game/server-authored-rebuild-module.cpp` replaces the worker
  thread with CAP_JOBS for the hot generation; the EXE keeps the thread copy.
- `player_core` and `player_session` domain capability/bridge added. No handler
  uses it yet; `EntitySnapshotV1` remains for unmigrated paths.
- EXE artifact `2026-11-14T23:13:13Z` published successfully.

## 2026-11-14 client retirement and domain parity

- Standalone `--client` is retired; `runClient` and `src/network/client.cpp`
  were removed. Full client remains the supported hot/client surface.
- `src/game/net-common-module.cpp` provides the DLL-side pure endpoint helpers.
- `serverRuntimeSimulatePlayers` now reads `player_core` and `player_session`
  through the domain bridge every 300 ticks and journals
  `game.state.domain-parity`.
- `src/host/net-common-host.cpp` owns resident WinSock startup/socket options.
- The existing `authored-rebuild-bridge-module.cpp` already delegates the hot
  path to the resident worker; no duplicate worker source remains.
- Server and ICE raw socket/thread mechanisms remain resident. They were not
  moved because the host server-transport/ICE capability and hot loop
  extraction are still unimplemented.
