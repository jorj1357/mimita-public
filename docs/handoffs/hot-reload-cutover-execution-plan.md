# Hot-reload cutover execution plan

Date: 2026-11-16

## Current state

- Strict cold gate is active in `build_target_gate.py` and `build.py`.
- A strict failure blocks EXE publication.
- Current gate: `exe=609 cold=39 tests=0 game_in_exe=570`.
- Current cold trial: `198` undefined symbols.
  - resident: `158`
  - bootstrap: `56`
  - test: `0`
- `map-editor` and `map-geometry` are folded into the game DLL.
- `world-rendering` remains a separate module entry for now; it was briefly
  folded and reverted because the build was blocked by an unrelated
  `packet_registry_codegen.py` stale-ledger failure and the fold was not
  runtime-verified.
- Tests are deleted from production. `--ice-connect` remains as a production
  diagnostic.

## Hard rules

1. No test executable and no `MIMITA_WITH_SELFTESTS`.
2. Behavior evidence is `events.jsonl` from the real EXE/dev-loop.
3. One game DLL owns all game and folded module sources.
4. EXE end state is cold-core only: `resident`, `bootstrap`, `launcher`, `tool`.
5. Strict gate must end green with:
   - `game_in_exe=0`
   - `test_sources=0`
   - cold undefined symbols `=0`
6. No resident fallback for migrated behavior.
7. No new raw debug `printf` in game/hot paths; use `StructuredLogger`.

## Server cutover

Use a separate optional export, not a `GameServerV1` layout change:

`GetMimitaGameServerPolicyV1`

This keeps the last published EXE able to load newer game DLLs while the server
cutover is implemented.

### Pass 1: config and gamemode

Add `ServerStartupV1`, `ServerStartupResultV1`, and `GameServerPolicyV1` in
`src/host/abi/game-server-abi.h`.

Add optional lookup in `HostLoader`/`HostRuntime`:

- old EXE ignores the policy export
- new EXE uses it
- missing policy fails closed

Move from `src/network/server.cpp` into a game-owned startup function:

- vehicle/weapon/NPC config
- community server config
- gamemode registry, mode packs, roles, monster pools, behavior profiles,
  gamemode map pool
- `serverGamemodeStart`
- community map/mode/weapon/match operations
- arena ball spawn
- map default/map-pool helpers

Resident keeps:

- logger and hot-server requirement
- socket create/bind/nonblocking
- ICE registration/poll
- transport adapters
- session containers
- fixed-tick scheduler
- room-code file

### Pass 2: world/collision

Add `ServerWorldLoadV1` or a second policy function with opaque world pointers.

Move:

- `loadHeadlessWorld`
- authored base capture/apply/seed
- `buildNpcWorldCollision`
- `loadServerBodyTemplateFromGlb`

Resident creates the objects and owns lifetime; game fills them.

Required events:

- `server.policy.startup`
- `server.policy.world`
- `server.startup.stage`
- `network.room.registered`
- `network.connection-state.changed` → `Connected`
- `network.client-snapshot-window`
- `game.server.handler` with `path=dll`

## Test removal

Completed:

- deleted self-test TUs/headers
- split `runIceConnect` into `network/ice/ice-connect.cpp`
- split ICE event emission into `network/ice/ice-events.cpp`
- replaced stdout test events with `network.ice.*` events
- removed 67 selftest branches from `game-cli.cpp`

Do not add tests back. Any remaining diagnostic must emit an owner event.

## Module fold sequence

Completed:

- `map-editor`
- `map-geometry`

Remaining:

1. `world-rendering`
2. `npc-perception`
3. `network-policy`
4. `navigation-policy`
5. `actor-rendering`
6. `infinite-worldgen`
7. `chunk-collision`
8. `npc-behavior`

Per module:

1. remove catalog entry
2. make source owner `game`
3. generate `module-identity.h`
4. rename `GetMimitaModuleAPI` export per module
5. initialize locally in `module-host-bridge-module.cpp`
6. dispatch every event locally
7. remove the module DLL build/load path
8. prove via events that the local module ran

After all folds:

- delete `config/modules/modules.json`
- delete module catalog build/drive code
- delete module DLL loading from resident `ModuleHost`
- game DLL owns all game + module sources

## Drive cold refs to zero

Start with the server policy split. After each cluster:

1. run `python devscripts/dev-loop.py --request cold-link-trial`
2. inspect `build/hot-reload/cold-link-trial.json`
3. reduce one owner cluster
4. repeat

Expected next clusters after server:

- `badconn`
- `NotificationSystem`
- `MatchLeaderboard`
- `ReplayPlayer`
- `GamemodeRegistry`/`MatchRoleRegistry`
- audio/config services
- bootstrap game orchestration

Do not re-own `Debug`, `Perf`, or `path_utils` resident without a host import or
capability; that previously broke the game-DLL link.

## Final strict-zero flip

Only when the cold trial is zero:

1. change `exe_source_relpaths()` to select only `resident`, `bootstrap`,
   `launcher`, `tool`
2. EXE source count becomes the cold set
3. game DLL source set equals all game + folded module sources
4. run the normal EXE build
5. strict gate passes
6. launch and validate via `events.jsonl`

Do not flip the EXE source set earlier; the cold trial is the authoritative link
demand until then.

## Validation matrix

- strict build target gate: `game_in_exe=0`, `test_sources=0`
- cold trial: `undefined=0`
- normal EXE build publishes current
- hot client gate passes
- server scenario:
  - startup config/world events
  - room registered
  - client connected
  - snapshots
  - server packet path `dll`
  - gamemode events
- no production `--*-selftest` commands
- no new raw debug prints in hot paths
