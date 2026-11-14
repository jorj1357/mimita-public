# Server cutover, packet registry, and live change classification

Date: 2026-11-14

## Source changes

- `src/host/abi/game-server-abi.h`: new resident/DLL server ABI
  (`ServerFrameV1`, `ServerPacketResultV1`, `GameServerV1`,
  `GetMimitaGameServerV1`).
- `src/game/server-runtime.h` / `src/game/server-runtime-module.cpp`: DLL-owned
  `process_packet`, `simulate_players`, `simulate_world`, `publish`, and
  `gamemode` calls.
- `src/game/game-module.cpp`: exports `GetMimitaGameServerV1`.
- `src/host/host-loader.h` / `src/host/host-runtime.*`: resolve the server API,
  expose `serverApiAvailable()` and barrier-wrapped server calls, and make probe
  events visible under the default `general` category.
- `src/network/server.cpp`: dedicated and listen loops route inbound packets and
  the four simulation/publish/gamemode segments through the DLL; the resident
  bodies remain only as a counted fallback. Hotness events moved to the
  `network` category and now report `server_path`, `server_calls`, and
  `server_failures`. Generated packet-registry classification replaces the
  hand-maintained migrated-type switch.
- `src/main.cpp`: server mode configures the whole-game host before
  `runServer`; `runServer` keeps owning host startup after the structured
  logger is ready.
- `config/hot-reload/packet-schema.json`,
  `devscripts/packet_registry_codegen.py`,
  `src/host/generated/packet-registry-v1.h`,
  `config/hot-reload/packet-ledger.json`: generated packet path registry.
- `devscripts/live.py`: runs both codegens, watches the packet schema and
  `packets.h`, and adds `touched_sources`, `game_only`, and
  `exe_rebuild_required` to the change report.

## Runtime evidence

- EXE artifact `2026-11-14T17:35:25Z` and generation 143 (final ABI,
  including ICE packet routing).
- Log `logs/11-14-2026/20261114_124317/events.jsonl`:
  - `game.host.probe.started` loaded `game-g143.dll`.
  - `game.hotness.summary` reported `server_path=dll`,
    `server_calls=2936` and growing at 60 Hz, `server_failures=0`.
  - A raw PING produced `game.transport.handled type=6 path=dll` from both the
    DLL dispatch and the resident path note, `dll=1`, `resident=0`,
    `violations=0`; the server returned a 28-byte PONG.
- Earlier proof on generation 140 / artifact `17:26:21Z`, log
  `20261114_122706`, showed the same shape (`server_calls=260..`).
- Parser codegens: `--check` passed for state (8 domains / 47 fields) and
  packets (78 packet ids).
- Static gate: 0/0.

## Not proven

- Connected-client join/chat through the hot server path.
- Live generation swap while a server session is active.
- ICE-managed session behavior under the cutover.
- Domain adapters and `EntitySnapshotV1` retirement.
