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

## Build evidence

- Normal audited migration EXE build published after the final cold reduction:
  `2026-11-16T17:21:34.920460Z`,
  `.dev/builds/2026-11-16T17-21-34.920460Z`.
- Whole-game generation 266 published:
  `build/game/generation-266/game-g266.dll`.
- Strict cold link trial:
  `build/hot-reload/cold-link-trial.json`
  - `undefined_symbol_count=6`
  - `link_return=1`
  - revision `b756746f925a8a6e27ff37f8e15746381e07c0cf012399f112a841b7bd12bc13`
- Cold undefined count during this session: `138 -> 114 -> 112 -> 100 -> 102
  -> 94 -> 91 -> 59 -> 40 -> 35 -> 25 -> 19 -> 6`. Intermediate counts include
  concurrent manifest and server-policy changes.

## Runtime evidence

- No new runtime behavior was accepted in this session.
- The running client instance was reported frozen before these changes; no
  hot-client launch or `events.jsonl` acceptance is claimed.
- The normal migration EXE build is build evidence only, not gameplay proof.

## Remaining cold link clusters

The strict cold trial now reports only 6 undefined symbols, all in the resident
module-host fallback:

- `MapConfigRegistry::instance()`;
- `MimitaWorldGen::WorldGenLibrary::instance()`;
- `MimitaWorldGen::WorldGenLibrary::loadMode(...)`;
- `npcWorldRaycastBlocked(...)`;
- `npcWorldLineOfSightBlocked(...)`;
- `setNpcWorldQueryWorld(...)`.

These are the map editor/worldgen/NPC-perception module-host services. They
should be replaced by game-owned capability calls or removed when the remaining
subsystem modules are folded; no host degradation is claimed.

No strict cold gate or `game_in_exe=0` claim is made.

## Next step

Finish the last module-host capability boundary:
`MapConfigRegistry` revision/map data, `WorldGenLibrary` mode data, and the NPC
world query should be provided by the active generation. Then rerun
`python devscripts/dev-loop.py --request cold-link-trial` and require zero before
the strict EXE flip. Runtime acceptance for audio-device ownership, ICE
diagnostics, and generation unload remains pending.
