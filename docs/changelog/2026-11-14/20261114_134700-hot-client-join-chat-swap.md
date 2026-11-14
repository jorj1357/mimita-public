# Hot client join, chat, and live swap proof

Date: 2026-11-14

## Changes

- `src/host/abi/game-host-abi.h`: added `kEventStartupConnect` and
  `StartupConnectV1` (direct address, room code, player, map, password,
  server name, and up to four startup commands).
- `src/host/host-runtime.*`: queues the launcher request and delivers it to the
  active generation on the next frame under the generation barrier.
- `src/main.cpp`: builds the startup request before the whole-game frame branch,
  so the DLL-owned frame path receives the same launcher join as the legacy
  path.
- `src/game/game-module.cpp`: applies the request to the DLL's
  `MultiplayerConnectInfo`, sets `GAME_PLAYING`, and queues startup commands.
- `src/engine/whole-game-entry.*`: executes queued startup commands once the
  hot session is connected and the world is loaded.
- `src/network/server.cpp`: `game.hotness.summary` now carries `players` and
  `npcs`.
- `devscripts/server_hotness_probe.py`: added `--client` and `--chat`; it
  asserts a hot join, a DLL chat request, and optionally a generation swap with
  the client still connected.

## Evidence

- EXE `2026-11-14T18:40:50Z`, DLL generation 149.
- `python devscripts/server_hotness_probe.py --client --chat hello-hot`:
  `ok`; joined `players=1`; chat request `path=dll`.
- `python devscripts/server_hotness_probe.py --client --chat swap-chat --swap`:
  `ok`; server `server_path=dll`, `resident=0`, `server_failures=0`; client
  stayed connected after a distinct-module generation swap; PING still
  answered.
- First hot join also showed `packetsIn=2120`, `packetsOut=4617`,
  `input=1097`, `dll=2120`, `resident=0` in the server journal.

## Not proven

- A behavior-changing generation swap (the probe swaps identical code through a
  distinct module path).
- ICE peer sessions.
- Domain adapters and `EntitySnapshotV1` retirement.
