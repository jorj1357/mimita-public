# Automated server hotness probe and live swap proof

Date: 2026-11-14

## Changes

- `devscripts/server_hotness_probe.py`: launches the published EXE with
  `MIMITA_GAME_DLL`/`MIMITA_GAME_PROBE`, waits for the server to answer a real
  PING, and asserts the journal contains:
  - `game.host.probe.started`;
  - `game.transport.path` or `game.transport.handled` with `path=dll` for
    `PACKET_PING`;
  - `game.hotness.summary` with `server_path=dll`, `server_calls>0`, and
    `server_failures==0`.
- `--swap` copies the DLL to a distinct path, publishes a probe-only ready
  manifest at generation 2, requires `game.generation.activated`, and requires a
  PING to still answer. The probe cleans up its manifest and DLL copy.
- `devscripts/live.py --once --verify-server` runs the probe after publishing.
- `src/network/server.cpp`: `game.transport.path` moved from the filtered
  `executable` category to `network` so the runtime gate can observe it.

## Evidence

- `python devscripts/server_hotness_probe.py`: `ok`, PONG 28 bytes,
  `server_path=dll`, `server_calls=260`, `server_failures=0`.
- `python devscripts/server_hotness_probe.py --swap`: `ok`, generation swap
  observed with a distinct module path and PING still answered.
- Probe report written to `build/game/server-probe.json` and
  `build/game/server-probe-swap.json`.

## Not proven

- Join/chat through a connected client.
- ICE peer sessions.
- A behavior-changing generation swap (the probe swaps identical code through a
  distinct module path).
