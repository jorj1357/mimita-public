# Behavior-changing server swap proof

Date: 2026-11-14

## Changes

- `src/host/abi/game-server-abi.h`: `GameServerV1.runtimeRevision` identifies the
  server behavior revision of a generation.
- `src/game/game-module.cpp`: exports `runtimeRevision` (raised from 1 to 2 for
  the acceptance build).
- `src/network/server.cpp`: `game.hotness.summary` now includes
  `runtime_revision` from the active server API.
- `devscripts/server_hotness_probe.py`: added `--swap-dll` and
  `--expect-runtime-revision`; the swap gate can now assert that the new
  generation's code is active.

## Evidence

- Generation 152: `runtimeRevision=1`.
- Generation 153: `runtimeRevision=2`.
- Command:
  `server_hotness_probe.py --dll generation-152 --swap-dll generation-153 --client --chat behavior-swap --swap --expect-runtime-revision 2`
- Result `ok`:
  - PONG before the swap;
  - client joined `players=1`;
  - chat request `path=dll`;
  - swap observed;
  - `runtime_revision=2` after the swap;
  - client stayed connected `players=1`;
  - `server_failures=0`;
  - PONG still answered.

## Not proven

- A gameplay-visible behavior delta (the current marker is a code generation
  revision, not a changed rule).
- ICE peer sessions.
- Domain adapters and `EntitySnapshotV1` retirement.
