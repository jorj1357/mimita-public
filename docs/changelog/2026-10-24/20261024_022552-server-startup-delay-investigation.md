# Server startup delay investigation

- Status: `PASS_WITH_HUMAN_REVIEW`
- Recorded: 2026-10-24T02:25:52Z (2026-10-23 22:25:52 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `2103321b012f546c07bb418add26269c66bbace0`
- Request: investigate reports that server startup takes about 10–30 seconds and sometimes does not complete as expected.
- Scope: read-only source and existing runtime evidence review. No gameplay source, configuration, regression record, running process, or existing user change was modified.

## Finding

The startup path waits for map/collision setup, NPC navigation preparation, and online ICE/coordinator registration before the server writes its room code. The GUI cannot finish its room-code wait or proceed to the join until that work completes.

Fresh existing runtime evidence in `logs/2026-10-24/20261024_021031/events.jsonl` identifies two real contributors:

- GUI-launched server PID 32440 (`payload2`, mode `payload`, build 2976): room-code wait was 11,650 ms. Headless map loading took about 2,061 ms. ICE startup then spent 7,362 ms requesting TURN credentials, 190 ms gathering candidates, and 116 ms registering the room. The room code was received. The TURN request accounts for most of this measured delay.
- A separate server PID 33420 (`xxity2`, mode `melee`, same build/run): navmesh baking took 7,713 ms, TURN credentials took 7,185 ms, and room registration took 63 ms. The room was registered about 15.9 seconds after `server.process.started`.

A prior Zombie Tower investigation reported 32.9368 seconds from process start to room registration, including 10.932 seconds of map loading. Its raw journal is absent in this checkout; the remaining roughly 16.46 seconds after a navmesh failure were not attributed. That report is supporting historical evidence, not a fresh measurement.

The room-code wait was extended from 15 seconds to 60 seconds in pre-existing work. That prevents the old timeout from firing on a 33-second startup, but does not make the server ready sooner. Current evidence points to variable network time for TURN credentials and map-dependent navmesh preparation as the main causes of 10–20 second starts. The exact cause of the historical 30-second Zombie Tower tail remains open. Room-code receipt is proven for the 11.65-second run; player-visible successful entry was not evaluated here.

## Ownership trace

- `src/gui/gui-main.cpp::launchServerProcess()` launches the dedicated process and starts the asynchronous room-code wait. `pollPendingServerRoomCode()` waits for the registered room file; the current source deadline is 60 seconds.
- `src/network/server.cpp::runServer()` loads the world and prepares startup state before calling `initServerIceListener()`. It writes the room code file only after registration succeeds.
- `src/network/server-ice.cpp::initServerIceListener()` requests TURN credentials, waits for ICE candidate gathering (source cap: 15 seconds), then registers the room.
- `src/network/coordinator-client.cpp::coordinatorRequestTurnCredentials()` passes a 5,000 ms timeout to WinHTTP for each configured request phase. One measured request took 7,362 ms, so the configured value is not a total wall-clock deadline.

The current source has bounded `network.server-ice-startup.stage` events for TURN, candidate gathering, and room registration. The matching build 2976 journal contains those events, so these timings came from the real startup owner rather than inference from source alone.

## Evidence and limits

- Executable identity from the existing journal: build `2976`, SHA-256 `662e981a7fce8e368873733093ddad91bf2bfbf55ea3f09cd0925b8b86066d70`, `source_current=true`.
- Exact journal: `C:\mimita-v9\logs\2026-10-24\20261024_021031\events.jsonl`.
- Runtime events: `network.room-code.wait.started/completed`, `server.process.started`, `map.server-load-start/success`, `npc.navmesh.bake-success` or `npc.navmesh.prepare-failed`, `network.server-ice-startup.stage`, and `network.room.registered`.
- No new build, scenario, or test was run in this investigation. Source/build evidence and the existing journal are recorded separately from human acceptance.
- No regression record was added: this investigation did not confirm a repaired behavior or establish the full cause of the 30-second case.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/networking/networking.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/regressions/README.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/logging-checker-v1.md`.
- `spec-behavior-review-v1`: PASS for identifying the implemented readiness contract and keeping the unexplained Zombie Tower interval open.
- `logging-checker-v1`: PASS; the stage events are rate-bounded startup records owned by the ICE startup path and contain elapsed milliseconds and outcome.
- `docs/ROUTER.md` names `docs/regressions/regressions-v1.md`, but that file is absent at the routed path; the active regression README and individual records were used.
- No network-feature workflow file exists under `docs/workflows/`; runtime-scenario validation is the available routed workflow. No source or packet changes were made.

## Pre-existing working-tree state

The following edits were present before this investigation and were left untouched: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/gamemodes/tdm.json`, `config/lighting.json`, `config/maps/zombietower4.json`, `config/movement.json`, `config/onlinemodes.json`, `config/roles.json`, `config/weaponsets.json`, `devscripts/dev-launch-modes.json`, `devscripts/dev-loop.py`, `docs/ROUTER.md`, `docs/jorj-docs/20261023plan.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/teamdeathmatch.md`, `docs/specs/networking/networking.md`, `docs/specs/open-gameplay-issues.md`, `src/game/game-cli.cpp`, `src/gamemode/gamemode.cpp`, `src/gamemode/gamemode.h`, `src/gamemode/map-config.cpp`, `src/gamemode/map-config.h`, `src/gui/gui-main.cpp`, `src/map/map-authored-geometry.cpp`, `src/network/community-match-client.cpp`, `src/network/community-match-client.h`, `src/network/multiplayer-physical-entities.cpp`, `src/network/packets.h`, `src/network/physical-entity-replication.cpp`, `src/network/server-gamemode.cpp`, `src/network/server-gamemode.h`, `src/network/server-ice.cpp`, `src/network/server-npcs.cpp`, `src/network/server-packet-chat.cpp`, `src/network/server-packets.cpp`, `src/network/server-players.cpp`, `src/network/server.h`, `src/physics/physical-entity.h`, `src/terminal/actor-commands.cpp`, `src/terminal/crate-commands.cpp`, untracked `assets/maps/payload2.glb`, `assets/objects/things/gamemode-objects/`, `config/gamemodes/payload.json`, `config/maps/payload1.json`, `docs/changelog/2026-10-23/20261023_182134-clarify-gameplay-issues-intake.md`, `docs/changelog/2026-10-23/20261023_182820-extend-room-startup-wait.md`, `docs/changelog/2026-10-23/20261023_184545-audio-format-support-audit.md`, `docs/changelog/2026-10-23/20261023_224504-set-zombietower-radii.md`, `docs/changelog/2026-10-23/20261023_232212-actor-preset-command.md`, `docs/changelog/2026-10-24/`, `src/gamemode/payload-runtime.cpp`, and `src/gamemode/payload-runtime.h`.

## New file

This changelog is the only file added by this investigation. No prior text existed in this file; the exact addition is this complete changelog.
