# Server policy startup ABI defaults rejected every server launch

- Recorded: 2026-11-17T21:06:37Z
- Status: FIXED IN SOURCE; fresh Payload server/client runtime and visible-window evidence recorded
- Changelog: [20261117_211612-payload-startup-abi-request-version-fix.md](../../changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md)

## Expected behavior

The dev-loop's Payload mode 57 launch should pass startup configuration into
the active game generation, load Payload on `funworld3`, and continue into
world/server startup.

## Observed failure

Run `20261117_210637` selected and started the published executable from
`2026-11-17T20:50:12.808035Z` (SHA-256
`27be990772c8aa175433512ff483c6eeb6155dde911c7cb78e75c0da39f9e5ef`). The
server journal records `server.startup.stage` for `configuration_load` as
failed with `invalid server startup request`, then the server exits. The
failure occurred before Payload mode configuration or map loading.

Journal: `logs/2026-11-17/20261117_210637/server-01/events.jsonl`.

## Cause

`src/game/server-policy-module.cpp` validates startup, world-load, and
configure request versions against `kGameServerPolicyAbiVersion`, now `2`.
However, `ServerStartupV1`, `ServerWorldLoadV1`, and `ServerConfigureV1` in
`src/host/abi/game-server-abi.h` defaulted their `version` fields to `1`.
The resident code value-initializes these request structs, so the startup
policy rejected the request before reading mode configuration.

## Correction and evidence

Changed those three request defaults to
`kGameServerPolicyAbiVersion`. The root dev-loop observed source generation
98 and published artifact
`2026-11-17T21:12:47.128057Z` (SHA-256
`c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac`). Fresh
mode 57 run `20261117_211306` reached `server.startup.stage` complete for
`configuration_load`, `world_load`, and `map_config_load`; emitted
`payload.configured`; registered room `K2TTN85` with 19 startup NPCs; and
remained running. The client with the same build connected and applied
authoritative snapshots. The dev-loop recorded a successful managed-mode
launch, and the visible `mimita.exe` window displayed the Payload HUD and
intermission phase.

Evidence journals:

- `logs/2026-11-17/20261117_211306/server-01/events.jsonl`
- `logs/2026-11-17/20261117_211306/client-01/events.jsonl`
- `logs/2026-11-17/20261117_211306/dev-loop/events.jsonl`

## Specification

`docs/specs/dev-loop/dev-loop.md` and
`docs/workflows/runtime-scenario-validation.md` require startup and runtime
behavior to be demonstrated by the exact launched executable and its own
`events.jsonl`; build publication alone is not runtime proof.

## Remaining review

- A person should review the visible scene's lighting and the open console
  overlay; this session did not claim human play acceptance.
