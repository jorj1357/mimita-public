# Developer-loop mode 10 damage-volume evidence

## Scope

Mode 10 is now the diagnostic Zombie Tower launch mode. It uses the managed
developer loop and starts the server/client with the same map, gamemode, and
startup teleport used for this investigation. Mode 8 was not used for the
final run.

## Mode 10 contract

`devscripts/dev-launch-modes.json` mode `10` now adds:

- `--mode zombie_tower`
- `--gamemode zombie_tower`
- `--no-npcs`
- client startup command:
  `teleport -1263.483154296875,-820.2572021484375,379.64031982421875`

The developer loop built and launched its managed artifact at:

`C:\mimita-v9\.dev\builds\1960\mimita.exe`

No manually named executable was created for this run.

## Logging changes

`src/network/server-gamemode.cpp` now records the authored damage-volume
definitions in `damage-volume.runtime-gate`, including ID, enabled state, shape,
position, radius, size, damage, and interval. It also records
`damage-volume.player-skip` with the explicit player lifecycle reason when a
player is inside but is not eligible for damage.

The spawn lifecycle now records map-ready receipt, authoritative spawn issue,
client ack send/retry, server ack rejection/activation, client activation, and
the client map-ready gate state. Startup commands now wait for
`MultiplayerContext::gameplayActive`, so a startup teleport cannot advance the
authoritative transform epoch before the initial spawn ack is accepted.

## Runtime evidence

Journal:

`logs/2026-10-11/20261011_002053/events-000001.jsonl`

The fresh mode-10 run confirms:

- `run.started` identifies launch mode `10`, map `zombietower4`, build `1960`,
  and the managed `.dev` executable.
- The server is in `match_mode: zombie_tower` and reaches an active gameplay
  phase with one player.
- `damage_volume_1` is enabled, has damage `5`, interval `5`, radius `5`, and
  the requested center position.
- The first diagnostic run showed the player `inside: true` but stuck at
  `spawn_state: 1`, `awaiting_spawn_ack`; every ack was rejected because the
  server expected transform epoch `2` while the client retried epoch `1`.
- The same trace identifies the cause: the startup teleport ran before
  gameplay activation and created the epoch-2 transform.
- The corrected run shows matching map-ready/spawn/ack/activation events and
  repeated `damage-volume.damage` events for `damage_volume_1` with
  `players_hit: 1` and damage `5` every five ticks.

## Interpretation

`active: false` is not expected to appear in `config/maps/zombietower4.json`.
It is a runtime lifecycle value derived from the server player's spawn state.
The map JSON is loading and containment is working. The original damage-volume
symptom was caused by the startup teleport racing the initial spawn handshake,
not by the volume coordinates, shape, or containment calculation. The launch
gate fix is built and proven in the managed mode-10 run.
