# Cutover-1 server policy split and audited migration lane

## Task

- Task ID: 20261116-cutover-server-policy
- Session ID: 20261116-cutover-server-policy
- Owner: opencode
- Focus: hot-reload Cutover-1 server policy
- Summary: Move dedicated and listen server startup/config/world/gamemode policy
  behind the optional `GetMimitaGameServerPolicyV1` export, add an explicit
  audited migration EXE lane, and reduce `server.cpp` cold references to zero.
- Status: partial
- Date, time, timezone: 2026-11-16T15:24:19Z
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `b928c7f6042a8e77d8427e90511df7d225d26fe6`
- Final commit: uncommitted work on top of `b928c7f6`

## Pre-existing changes

- `config/analytics.json` and submodule `devscripts/blender-ai` were already
  modified and are not part of this session.
- Commit `b928c7f6` contains concurrent work by another session, including the
  cold-only `exe_source_relpaths` flip and GL/render bridge changes. This session
  preserved that work and added a migration-lane exception rather than reverting
  it.
- The many GL/avatar/packet changes present during the session were not authored
  or reverted here.

## Requested behavior

Implement the agreed Cutover-1 plan: start with the server policy split, add an
audited migration EXE lane, and produce real dev-loop evidence instead of a
synthetic test.

## Specification alignment

- `docs/specs/hotreload/hotreload.md`
  - resident server keeps only the pump;
  - the replaceable generation owns server behavior;
  - migrated behavior must not silently fall back to resident code;
  - hot claims require same-PID runtime evidence.
- `docs/handoffs/hot-reload-cutover-execution-plan.md`
  - optional `GetMimitaGameServerPolicyV1` export;
  - move config/gamemode and world/collision startup into the generation;
  - drive cold refs to zero before the final EXE flip.
- Decision recorded in this session: use an explicit, audited migration EXE lane
  while the strict gate is still red; remove it at the final flip.

## Exact implementation changes

### `src/host/abi/game-server-abi.h`

- Added `kGetGameServerPolicySymbol`, `ServerStartupV1`,
  `ServerStartupResultV1`, `ServerWorldLoadV1`, `ServerWorldLoadResultV1`,
  `ServerConfigureV1`, `ServerConfigureResultV1`, `ServerAuthIdentityV1`,
  `ServerProjectilePerfV1`, `ServerConnectionIdV1`, and
  `GameServerPolicyV1`.
- Added `GameExportV1` and `kGetGameExportSymbol` for the replay export
  subprocess.
- Kept `GameServerV1` layout unchanged. The policy export is optional and
  additive.

### `src/host/host-loader.h`

- Resolves `GetMimitaGameServerPolicyV1` after the existing server API.
- Validates the policy ABI, struct size, and required function pointers.
- Stores the policy in `LoadedGeneration::serverPolicy`.

### `src/host/host-runtime.h` / `src/host/host-runtime.cpp`

- Added barrier-aware policy calls:
  - startup;
  - world load;
  - configure;
  - create/destroy player;
  - make UDP connection id;
  - poll reload;
  - auth identity;
  - projectile perf;
  - clear runtime objects;
  - coordinator ICE done.

### `src/game/server-policy-module.cpp` (new)

- New game-owned server policy owner.
- `serverPolicyStartup`: loads vehicle/weapon/NPC/community/gamemode/mode-pack/
  role/monster/behavior/profile/map-pool config, body template, resolves map and
  max players, emits `server.policy.startup`.
- `serverPolicyWorldLoad`: loads the headless world, applies authored geometry,
  seeds the authored cache, builds NPC collision, emits `server.policy.world`.
- `serverPolicyConfigure`: performs duel or community map/mode/weapon/match
  setup, spawns the arena ball, spawns startup NPCs, emits
  `server.policy.configure`.
- `serverPolicyPollReload`: owns the game-owned config reload block and NPC
  difficulty refresh.
- `serverPolicyAuthIdentity`, `serverPolicyProjectilePerf`,
  `serverPolicyClearRuntimeObjects`, `serverPolicyCoordinatorIceDone`,
  `serverPolicyMakeUdpConnectionId`, `serverPolicyCreatePlayer`,
  `serverPolicyDestroyPlayer`, `serverPolicyShutdown`.
- Export: `GetMimitaGameServerPolicyV1`.

### `src/network/server.cpp`

- Dedicated server startup now uses the policy for config, map resolution,
  world/collision, mode setup, arena ball, and startup NPCs.
- Listen server startup uses the same policy.
- Replaced resident `serverTimestamp()` calls with a local resident timestamp
  helper.
- Replaced resident `makeUdpConnectionId` with a policy call.
- Replaced resident `AuthSystem` persistence identity reads with policy auth
  identity.
- Replaced the resident config poll block with `serverPolicyPollReload`.
- Replaced resident `clearRuntimeObjects` and `coordinatorIceDone` calls with
  policy calls.
- Replaced `Player mirrorPlayer` construction with a policy-created opaque
  pointer.
- Removed the now-unused resident `communityMapPool` and `gamemodeDefaultMap`
  helpers.
- `isServerHost` now uses the resident hosted-room session instead of the
  game-owned GUI listen-server accessor.

### `src/network/server.h`

- `ListenServerState::mirrorPlayer` changed from `std::unique_ptr<Player>` to an
  opaque `void*`, owned through the policy create/destroy calls.

### `build.py`

- Added `MIMITA_CUTOVER_MIGRATION=1` or
  `build/hot-reload/migration-lane.enable` as the audited migration lane.
- Writes `migration_lane` into `build/link-inputs-<mode>.json`.
- Runs the cold gate report-only in the migration lane and writes
  `build/hot-reload/migration-lane.json`.
- The strict final gate path remains unchanged when the marker is absent.

### Final strict flip

- Removed `build/hot-reload/migration-lane.enable`.
- Removed the migration-lane branch from `build.py`; the normal build now
  always runs `build_target_gate.py --require-cold-zero`.
- Removed migration-only bridge sources and marker exceptions from
  `devscripts/module_ownership.py`; the EXE source set is cold-only.
- Strict gate result: `exe_sources=46`, `game_sources_in_exe=0`,
  `test_sources=0`, `cold_undefined=0`, `clean=true`.
- The strict cold EXE launched and ran the dedicated server through game
  generation `270`.

### `devscripts/build_target_gate.py`

- Reads the cold-link trial report.
- Reports `cold_undefined` and `cold_trial_error`.
- `--require-cold-zero` makes the final gate require `undefined_symbol_count=0`.

### `src/debug/structured-log.cpp`

- Removed the resident logger's dependency on the game-owned `Debug`
  facade for console mirroring.
- Config load/parse errors and the init/hot-reload notices now use
  `std::fprintf(stderr, ...)`, keeping the cold logging sink resident.
- Removed the now-unused `debug-log.h` include.

### `src/game/game-cli.cpp`

- Removed the remaining synthetic diagnostic/test commands:
  `--gamemode-config-validate`, `--actor-triangle-spike`, `--ice-join-test`,
  `--replay-export-fbo-test`, and `-exportdiagnostic`.
- Removed the bootstrap duplicate of `forceMainMenu`; the game-owned
  `src/game/main-menu-command-module.cpp` remains the owner.
- Removed the bad-connection preset block from the production `--ice-connect`
  diagnostic.
- Kept `--versioninfo`, the retired ICE-command rejection, and `--ice-connect`.

### `src/main.cpp`

- Removed the post-hot-path legacy `Engine`/`gameInit`/`engineTick` fallback.
- Normal clients now fail closed with `game.client.hot-required` when no
  frame-owning generation is available; there is no resident gameplay fallback.
- Kept the bounded `--export-replay` subprocess path and `--udp-echo`
  diagnostic as their own entries.
- Removed early `LocalProfileSystem`/`AuthSystem` initialization from the cold
  entry; the game generation now initializes identity from `GameStartupV1`.
- Passes `launchOptions.sessionToken` into the game startup handoff.

### `src/network/server-ice.cpp`

- Moved ICE connection-id allocation, legacy endpoint mapping, and actor-preset
  clearing behind `GameServerPolicyV1` calls.
- Replaced the game-owned session-removal logger call with an equivalent
  resident `network.player.session.removed` event.

### `src/game/game-export-module.cpp` (new)

- Added the optional `GetMimitaGameExportV1` entry.
- Runs the bounded replay export inside the generation after host generation
  load, so `main.cpp` no longer calls `gameInit`, `runExportSubprocess`,
  `getReplayExportJob`, or reads `gReplayExportVerbose`.

### `src/game/game-module.cpp`

- `gameBootstrap` initializes `LocalProfileSystem` and `AuthSystem` from the
  startup session token before creating the whole-game entry.
- The lazy `gameTick` bootstrap path does the same with an empty token.
- `kEventStartupConnect` falls back to `AuthSystem::displayName()` when the
  launcher did not supply a player name.

### `src/network/server.cpp`

- Removed the resident `setServerAudioMode(true)` call; dedicated-server audio
  mode is now applied by `serverPolicyStartup`.

### `config/source-owners.json`

- Moved `src/network/net_mode.cpp` to `resident` because only the cold
  bootstrap calls launch parsing/usage.
- Kept `src/network/udp-echo.cpp` game-owned; moving it resident immediately
  added eight cold references, so the change was reverted.

### Migration coexistence helpers

- `src/host/host-image-decode.cpp` now renames every public `stb_image`
  entry point, not only `stbi_load`/`stbi_image_free`/`stbi_failure_reason`, so
  it can coexist with the game-owned `stb_image_impl.cpp` in the migration EXE.
- `src/host/host-debug-facade.cpp` marks its Debug symbols weak before
  including the shared facade, so the game-owned `debug-log.cpp` wins in the
  migration EXE while the strict cold EXE keeps the resident facade.
- `src/host/host-asset-path.cpp` remains host-prefixed and coexists with the
  game-owned `path_utils.cpp`.

### `devscripts/module_ownership.py`

- Normal builds use the cold-only EXE source set from the source manifest.
- When the migration marker exists, the legacy full source set is used so a
  development EXE can link.
- Migration-only bridge sources: GL resource, render, resident state, file, job,
  and socket bridges.

## Contract / ABI changes

- Contract: optional `GetMimitaGameServerPolicyV1` export.
- Version: `kGameServerPolicyAbiVersion = 1`.
- Compatibility: old hosts ignore the export; new hosts require it for server
  startup. `GameServerV1` layout is unchanged.
- Reverse: remove the policy export usage and restore the previous server
  startup code.

## State / schema changes

- No state-domain schema change in this session.
- The server policy operates on resident-owned player/NPC/world containers via
  opaque pointers.

## Diagnostics

- Owner/category: `server.policy.*` (Network).
- Input: startup/config/world/configure requests.
- Output: map, path, max players, triangle/spawn counts, NPC collision count,
  startup NPC count, arena ball status.
- Rejection: policy returns a bounded error string; resident startup returns
  non-zero and logs the failure.
- Rate limiting: one event per startup/world/configure phase.

## Validation

- Build: game generation `261`, `build/game/generation-261/game-g261.dll`,
  code hash `b36d71f756f00c2b2b8b4a47c9ced430893368defaededa47632c99dfea13514`.
- EXE: audited migration artifact `2026-11-16T15:20:55.910858Z`,
  SHA-256 `7eda0dc0694219ddb61eaab0c853d520fc00ae4d16858dac4b96a25d518960e8`.
- Latest run: `20261116_180054`, server PID `6212`, client PID `25896`,
  strict cold EXE `2026-11-16T17:59:12.782763Z`, game generation `270`.
- Strict gate: `clean=true`, `exe_sources=46`, `cold_core_sources=45`,
  `game_sources_in_exe=0`, `test_sources=0`, `cold_undefined=0`.
- Server journal:
  `C:\mimita-v9\logs\2026-11-16\20261116_180054\server-01\events.jsonl`.
- Client journal:
  `C:\mimita-v9\logs\2026-11-16\20261116_180054\client-01\events.jsonl`.
- Earlier policy run: `20261116_152129`, server PID `33604`, client PID
  `26640`.
- Cold trial:
  - before: `198` undefined (`resident=158`, `bootstrap=56`);
  - after server policy: `139` undefined (`resident=96`, `bootstrap=56`);
  - after structured-log cleanup: `138` undefined (`resident=95`,
    `bootstrap=56`);
  - after CLI/test cleanup and host facades: `91` undefined
    (`resident=74`, `bootstrap=19`);
  - after main fallback removal and audio policy move: `40` undefined
    (`resident=35`, `bootstrap=5`);
  - after ICE/main/export policy moves: `6` undefined, all in
    `src/hot-reload/module-host.cpp` (`resident=6`, `bootstrap=0`);
  - final: `0` undefined; cold trial links; strict gate green.
  - `src/network/server.cpp` references: `67` before, `0` after.
  - `src/game/game-cli.cpp` references: `33` before, `0` after.
  - `src/renderer/renderer.cpp` references: `10` before, `0` after.
  - `src/main.cpp` references: `24` before, `5` after.
  - `src/duel/duel-queue.cpp` and `src/audio/audio.cpp` references are now
    cleared in the latest generated trial; concurrent work also owns those
    changes.

## Runtime evidence

- `server.policy.startup` at sequence `64`: map `funworld3`, path
  `assets/maps/funworld3.glb`, mode `payload`, max players `999`, NPC count `19`.
- `server.policy.world` at sequence `96`: map loaded, `44016` triangles,
  `7` spawn points, `44016` NPC collision triangles.
- `server.policy.configure` at sequence `120`: `19` startup NPCs, arena ball not
  present.
- `server.startup.stage` records configuration, world load, bind, map config,
  and room registration.
- `network.room.registered`: room `L4R8G8Y`, `startup_npc_count=19`.
- Client `network.connection-state.changed`: `from=Connecting`, `to=Connected`.
- `game.hotness.summary`: `dll=2649`, `resident=0`, `violations=0`,
  `server_path=dll`, `server_failures=0` (strict cold EXE run
  `20261116_180054`, `dll=2501`).
- `packet.route` and `render.path` events are present in the client journal.
- Hot-client gate passed in the same run.

## Not proven

- Same-PID server config/gamemode code change with rollback.
- Real generation-to-generation state migration; `player_core` is now v3 but a
  live migration/rollback proof is still required.
- The module catalog still has 8 entries; module DLL loading must be folded and
  deleted even though the cold link trial is zero.
- `MIMITA_GAME_DISABLE` and legacy fallback comments remain in the resident
  host/server paths.
- Audit `unresolved_game_blockers` is not proven zero; current generated audit
  counts still include `RUNTIME_PROOF_REQUIRED` and `STATIC_STATE_REVIEW`.
- The four unknown packet IDs (`PACKET_PROFILE`, `PACKET_ENTITY_SPAWN`,
  `PACKET_ENTITY_DESPAWN`, `PACKET_RESPAWN_REQUEST`) need tombstone/retention
  classification.
- Human visual acceptance.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed; a local server/client connected.
- Still unverified: same-PID server policy change/rollback and the remaining
  Cutover-1 criteria.

## Agent handoff context

- Owner: opencode
- Focus: Cutover-1 server policy
- Files to check:
  - `src/game/server-policy-module.cpp`
  - `src/network/server.cpp`
  - `src/host/host-runtime.cpp`
  - `src/host/abi/game-server-abi.h`
  - `build.py`
  - `devscripts/module_ownership.py`
- Do not redo: the server startup/config/world/gamemode policy split, the CLI
  selftest removal, or the resident Debug/asset-path/stb coexistence fixes.
- Next exact step: continue with the remaining cold clusters
  (`duel-queue.cpp`, `main.cpp`, audio, ICE), then state migration and the final
  strict flip. The same-PID server config/gamemode proof remains open.

## Automatic evidence

- Branch/base/final: `checkpoint/one-checkout-condensation-20261111` /
  `b928c7f6042a8e77d8427e90511df7d225d26fe6` / uncommitted.
- Build identity: strict cold EXE
  `2026-11-16T17:59:12.782763Z:11827cbf2f8526f89e0d330a28953725e2bde93d5029e3f7e85c0676d8e142ff`;
  game generation `270`.
- Latest measured cold trial revision:
  `3356d253d841c83de64d3aa28caebeec98d31940343625ddaf5fd9e43f0ebccb`
  (`0` undefined; trial link succeeds).
- Latest server journal:
  `C:\mimita-v9\logs\2026-11-16\20261116_180054\server-01\events.jsonl`.
- Latest client journal:
  `C:\mimita-v9\logs\2026-11-16\20261116_180054\client-01\events.jsonl`.

## How to reverse

- Restore the previous resident server startup and remove the policy export.
- Delete `src/game/server-policy-module.cpp`.
- Restore `ListenServerState::mirrorPlayer` to `std::unique_ptr<Player>`.
- Revert `build.py`, `devscripts/module_ownership.py`,
  `devscripts/build_target_gate.py`, `src/game/game-cli.cpp`, and
  `config/source-owners.json` to the pre-session versions.
- Restore the bootstrap `forceMainMenu` only if the game-owned
  `main-menu-command-module.cpp` path is also reverted.
- Keep `src/host/host-debug-facade.cpp` and
  `src/host/host-image-decode.cpp` for the strict cold EXE; they are now part
  of the permanent cold facade, not a migration-only exception.
- Reverse only files touched by this session. Preserve the concurrent work and
  the pre-existing analytics/blender-ai changes.
