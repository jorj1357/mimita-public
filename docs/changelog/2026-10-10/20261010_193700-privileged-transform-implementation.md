# Privileged fly and teleport authority implementation

Date: 2026-10-10

## Scope

Implemented the server-authoritative fly/unfly and administrator teleport
handoff described in the networking, movement, and terminal-command specs.

## Source changes

- Centralized privileged-transform permission behind the existing server host
  identity and applied it to command and legacy teleport-packet paths.
- Routed the multiplayer terminal teleport through the server command path;
  the client no longer commits the networked player position optimistically.
- Reused `beginAuthoritativeTransform` / `beginAuthoritativeTeleport` for
  fly-enable, fly-disable, and teleport epochs.
- Added a server-authoritative-flight validation mode: movement input remains
  accepted as intent, while client position, velocity, and wall-crossing data
  cannot overwrite the server flight simulation.
- Generalized the procedural teleport client gate into a privileged-transform
  epoch handoff used by fly, unfly, teleport, and procedural teleport.
- Added bounded structured transform-assignment and client-command diagnostics.

## Validation evidence

- Source check: `git diff --check` passed.
- Build: `python build_agent.py` succeeded; `build/changelog.txt` reported
  `Status: SUCCESS`, return code 0, with the final diagnostic-only rebuild
  compiling one changed translation unit.
- Executable identity: `C:\mimita-v9\mimita.exe --versioninfo` through the
  repository's Windows command wrapper reported build time `19:32:15` and
  journal `logs/10-10-2026/20261010_193607/events-000001.jsonl`.
- Networking self-test: snapshot chunk self-test passed.
- Runtime server smoke: the new executable loaded `dust2cyberiav4`, built its
  collision world, reached `SERVER TRANSPORT READY`, and exited cleanly after
  the bounded timeout. No player command was exercised in that smoke run.

## Remaining acceptance boundary

The available UDP multiplayer harness could not run because its existing
helper source `tests/network-protocol-smoke.cpp` is missing. A real connected
client/server run still needs to exercise `fly`, wall crossing, `unfly`, and
coordinate teleport and then verify the new transform events plus the absence
of a position-correction storm in the live JSONL journal. Human visual and
multiplayer acceptance is not claimed here.
