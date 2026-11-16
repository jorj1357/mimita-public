# Host cold-link decoupling

## Task

- Task ID: 20261116-host-cold-link-decoupling
- Session ID: 20261116-host-cold-link-decoupling
- Owner: opencode
- Focus: ordinary host/game source split, cold EXE link reduction
- Status: partial; strict cold link is not zero
- Date, time, timezone: 2026-11-16T11:12:11Z
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Final commit: uncommitted work on top of the concurrent cutover changes

## Pre-existing changes

- `build.py`, `config/source-owners.json`, `devscripts/module_ownership.py`,
  `src/game/game-cli.cpp`, and the server-policy split are concurrent work by
  another session. They were not authored or reverted here.
- `config/analytics.json` and `devscripts/blender-ai` were already modified.
- Game generation 261/262, the migration EXE lane, and the server policy ABI
  are not part of this session.

## Requested behavior

Continue the user-selected full host/game split and reduce the strict cold EXE
link demand without breaking the audited migration EXE.

## Exact implementation changes

### `src/renderer/renderer.cpp`

- Removed the renderer's GLFW cursor callback that cast the window user pointer
  to game-owned `Camera` and called `Camera::updateMouse`.
- Removed `gl-debug` macros from renderer raw-GL calls. The resident renderer now
  uses direct GL calls and owns only window/device behavior.
- Uses host-owned asset path and image decode helpers instead of game-owned
  `path_utils.cpp` and `stb_image_impl.cpp`.
- Registers/clears the resident window in `host-window-registry` on create and
  shutdown.

### New resident host support

- `src/host/host-window-registry.h/.cpp`: resident GLFW window handle used by
  host fallback services.
- `src/host/host-asset-path.h/.cpp`: host-prefixed executable-directory and
  asset lookup; weak legacy `resolveAssetPath` alias.
- `src/host/host-image-decode.h/.cpp`: host-prefixed stb_image entry points so
  the migration EXE can also link the game-owned stb implementation.
- `src/host/host-debug-facade.cpp`: resident Debug facade with weak symbols so
  the game-owned `debug-log.cpp` wins in the migration EXE.
- `src/host/host-actor-body.cpp`: weak resident `ActorBodyState::reset` default
  for the host server entity adapter.
- `src/host/host-perf-fallback.cpp`: weak resident Perf state/scope and
  watchdog stop facade.
- `src/host/host-persistence-fallback.cpp`: weak default persistence rewards.
- `src/host/host-persistence-http.cpp`: weak resident WinHTTP persistence batch
  transport.
- `src/host/host-socket-fallback.cpp`: weak resident UDP socket primitives.

### `src/host/window-host-fallback.cpp`

- Uses `mimita::host::residentWindow()` instead of game-owned
  `InputRouter::window()`.

### `src/hot-reload/module-host.cpp`

- Module host text lines now emit `module.host.log` through `StructuredLogger`
  instead of calling game-owned `Terminal`.
- Removed game-owned `NotificationSystem` calls from hot-reload success/failure
  notices; the existing `module.notice.shown` event remains the owner record.

### Duel/matchmaking cutover

- Removed the resident `duel-queue.cpp` owner override; the real `DuelQueue`
  now compiles into the whole-game DLL.
- Deleted `src/game/duel-bridge-module.cpp`, which existed only to forward
  `DuelQueue` calls through `GamemodeCapabilityV1`.
- Removed `initializeHostDuel` calls from `game-module.cpp` and the NPC probe.
- Removed the resident `duelInvoke` implementation and the `Gamemode`
  capability registration from `host-runtime.cpp`.
- Added `DuelQueue::~DuelQueue()` to join its poll thread before generation
  unload.
- Game code now calls the real `DuelQueue` directly; the queue, polling,
  history, leaderboard, notifications, and map/network handoff are generation-owned.


### Audio cutover

- Removed the resident `audio.cpp`/`music-manager.cpp` owner override; the real
  `AudioManager`, `MusicManager`, spatial policy, config, replay-sound capture,
  and device workers now compile into the whole-game DLL.
- Deleted `src/game/audio-bridge-module.cpp` and removed
  `initializeHostAudio` from `game-module.cpp` and the NPC probe.
- Removed the resident `audioInvoke` implementation and `Audio` capability
  registration from `host-runtime.cpp`.
- Added `shutdownAudio()` and call it from `shutdownWholeGameEntry()` before
  generation unload; `MusicManager::~MusicManager()` now calls `shutdown()`.
- `server.cpp` no longer calls game-owned `setServerAudioMode`; server policy
  owns that startup decision.
- Note: audio device lifetime is now generation-owned. If the final cold core
  must retain the audio device, this needs a later device/policy split.

### Authored-world rebuild cutover

- Removed the resident `server-authored-rebuild.cpp` owner; the real worker and
  its generation-owned thread now compile into the game DLL.
- Deleted `src/game/authored-rebuild-bridge-module.cpp` and removed
  `initializeHostAuthoredRebuild`.
- Removed the resident authored-rebuild worker functions and capability from
  `host-runtime.cpp`.
- The worker destructor already joins its thread on generation unload.

### ICE/coordinator and bad-connection fallbacks

- Added `src/host/host-coordinator-fallback.cpp`: a weak host copy of the
  coordinator HTTP client for the cold ICE diagnostic path. Game-owned strong
  symbols win in the migration EXE; weak pragmas precede the include.
- Added `src/host/host-badconn-fallback.cpp`: weak host copies of the
  bad-connection simulator/config/helpers for the cold ICE diagnostic path.
- Added `src/host/host-snapshot-fallback.cpp`: weak host snapshot parsing for
  the cold ICE diagnostic path.
- Added `src/host/host-server-ice-fallback.cpp`: weak ICE connection ids,
  legacy endpoints, session-removal event, and actor-preset clear fallback.
- Moved `src/network/udp-echo.cpp` to resident ownership as a bounded transport
  diagnostic.
- These host fallbacks are diagnostic-path coverage; normal clients use the
  generation-owned coordinator/badconn/snapshot implementations.


### player_core schema v3 and live migration

- `config/hot-reload/state-schema.json`: `player_core` moved from v1 to v3.
  v2 added `migration_epoch`; v3 removed the packed `flags` field and added
  `migration_revision` so the final record size is distinct from both older
  records.
- Regenerated `src/host/generated/resident-state-v1.h` and
  `config/hot-reload/state-ledger.json`.
- Updated `HostServerEntityAdapter` read/write for v3.
- Added `src/game/player-core-migration.h/.cpp`:
  - `state.migration.prepare`, `.commit`, `.reject` owner events;
  - v1 → v3 and v2 → v3 field-preserving migrations;
  - poison v2 value for atomic rejected-migration proof;
  - seed helpers for v1/v2;
  - `ensurePlayerCoreState()` called from `gameInitSubsystems`.
- Added `StateObjectId::PlayerCore`.
- Added dev commands:
  - `state.player-core-seed <1|2> [reject]`
  - `state.player-core-migrate`

## Live player_core evidence

- Run `20261116_180054`, client PID `25896`.
- Journal:
  `logs/2026-11-16/20261116_180054/client-01/events.jsonl`.
- Generation 271: v1 (136 bytes) → v3 (152 bytes), `state.migration.commit`.
- Generation 272: v2 (144 bytes) → v3 (152 bytes), `state.migration.commit`.
- Generation 273: poisoned v2 (144 bytes) → v3 rejected; old resident state
  retained (`state.migration.reject` plus bootstrap failure event).
- Same PID across all three activations.
- Added `state.player-core-migrate-v2` for the explicit v1→v2 add-field stage.
  Client-02 PID `26540` in the same run recorded
  `state.migration.commit` `old_size=136`, `new_size=144`,
  reason `player_core v1 migrated to v2` (seq 120912). The v1→v3 and v2→v3
  generation commits above exercise the same add/remove transformation across
  activations.

## Refreshed generated evidence

- `build/hot-reload/cold-link-trial.json`: `undefined_symbol_count=0`.
- `build/hot-reload/build-target-gate.json`: `exe=46`, `cold=45`,
  `game_in_exe=0`, `test_sources=0`, `cold_undefined=0`.
- `build/hot-reload/exe-cutover.json`: `linked_game_sources=0`.
- `build/hot-reload/cap-render.json`: `game_sources=633`,
  `raw_gl_sources=0`, `clean=true`.
- `build/hot-reload/audit.json` and `audit-classified.json` regenerated:
  `UNRESOLVED=0`; `NON_GAME=130`, `RESIDENT_ADAPTER_REQUIRED=216`,
  `RESIDENT_HOST=45`, `RUNTIME_PROOF_REQUIRED=277`,
  `STATIC_CLOSABLE_CANDIDATE=5`, `STATIC_CLOSED=64`,
  `STATIC_STATE_REVIEW=71`.
- The historical figures `42 raw GL sources` and `audit must_move=570` are
  stale. Freshness rules are recorded in
  `docs/specs/hotreload/hotreload.md`; current evidence is recorded in
  `docs/handoffs/hotness-cutover-direct-plan-20261116.md`.


## Build evidence

- Normal migration EXE build published:
  `2026-11-16T17:59:12.782763Z`,
  `.dev/builds/2026-11-16T17-59-12.782763Z`.
- Whole-game generation 274 published after the migration command hook:
  `build/game/generation-274/game-g274.dll`.
- Strict cold link trial: `undefined_symbol_count=0` at source revision
  `0998a6378dbff6c9c38b21e44af75d20d3363510df3cb063e667e75671acc6a6`.
- Strict build target gate: `exe=46`, `cold=45`, `game_in_exe=0`,
  `test_sources=0`, `cold_undefined=0`.
- EXE cutover gate: `linked_game_sources=0`.
- Cold undefined count during this session: `138 -> ... -> 6 -> 0`.
  Intermediate counts include concurrent manifest, server-policy, and
  module-host service work.

## Runtime evidence

- No new runtime behavior was accepted in this session.
- The running client instance was reported frozen before these changes; no
  hot-client launch or `events.jsonl` acceptance is claimed.
- The normal migration EXE build is build evidence only, not gameplay proof.

## Remaining cold link clusters

The strict cold trial now reports zero undefined symbols. The final
module-host service bridge was completed concurrently by another session; this
changelog records the player_core migration and the regenerated gate evidence,
not authorship of that bridge.

The regenerated strict gate now reports `game_in_exe=0` and `cold_undefined=0`.

## Next step

Run a fresh connected hot-client gate (the migration run later entered
`ReconnectFailed` during repeated swaps), then complete the remaining
Cutover-1 evidence: same-PID server config/gamemode change with rollback,
packet unknown-ID review, audit review-class closure, and human visual/gameplay
acceptance.
