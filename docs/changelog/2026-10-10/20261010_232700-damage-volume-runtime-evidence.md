# Damage-volume runtime evidence

## Scope

Added bounded diagnostics for authored damage volumes and added a developer launch
teleport for the Zombie Tower map. The diagnostics trace the runtime gate, entity
configuration, player containment, damage result, and damage-confirmation status.

## Source/config changes

- `src/network/server-gamemode.cpp`
  - Logs `damage-volume.runtime-gate` every 60 fixed ticks.
  - Logs `damage-volume.probe` for each enabled damage volume, including shape,
    position, radius, size, player positions, containment, lifecycle state, health,
    and teleport-invulnerability ticks.
  - Logs `damage-volume.player-result` when an active player is evaluated.
  - Sends the existing reliable damage-confirmation event for environment damage.
- `devscripts/dev-launch-modes.json`
  - Mode `8` now starts the Zombie Tower client with:
    `teleport -1263.483154296875,-820.2572021484375,379.64031982421875`.

## Build evidence

- Built successfully with `python build.py build-only`.
- Relinked named executable:
  `mimita-20261010T232300-damage-volume-diag.exe`.
- `--versioninfo` identified the executable and created the canonical JSONL run
  path before runtime testing.

## Runtime evidence

Server journal:
`logs/10-10-2026/20261010_192351/events-000001.jsonl`

Client journal:
`logs/10-10-2026/20261010_192421/events-000001.jsonl`

The server confirmed:

- `map-entity.loaded`: `zombietower4`, `entity_count: 4`.
- `damage_volume_1`: center `[-1263.483154296875, -820.2572021484375,
  379.64031982421875]`, radius `5`, damage `5`, interval `5` ticks.
- At tick `900`, the player was at exactly that center and the probe reported
  `inside: true` and `players_inside: 1`.
- The same probe reported `active: false`; no `damage-volume.player-result` or
  `damage-volume.damage` event was emitted because the runtime correctly skips
  dead/non-active players.
- The runtime gate was active (`match_mode: zombie_tower`, `phase_active: true`),
  so the missing damage is after entity loading and containment, at player
  lifecycle activation.

The startup teleport itself worked: the client log contains the requested
coordinates and the server later received the corresponding player position.

## Acceptance status

Human visual acceptance of the health bar and damage popup was not claimed. This
run proves the authored volume and containment path, and isolates the remaining
blocker to the player not being authoritative `Active` in the direct diagnostic
launch.
