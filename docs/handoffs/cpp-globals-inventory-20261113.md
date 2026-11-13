# Global variable inventory for the game-DLL boundary (2026-11-13)

**Status:** inventory plus link-closure bridge implemented 2026-11-13.
The compatibility bridge removes all 36 undefined global references. It is not
yet the final accessor conversion: pointer values are refreshed once after
bootstrap initialization, scalar flags are copies, and the game-local singleton
objects still need reconciliation before cutover.

## Why globals block the link

The remaining 36 game-global symbols are defined in the **bootstrap** translation
unit `src/main.cpp`, not in the game DLL. The game DLL can call resident services,
but it cannot resolve arbitrary EXE global-variable symbols.

Definitions: `src/main.cpp:337-395`.

Most pointer globals are initialized in `src/main-systems.cpp` after
`gameInit` and after static locals are constructed, around
`src/main-systems.cpp:725-748`.

The pointers therefore cannot simply be copied into the game DLL at
`initializeHost*` time: the bootstrap has not set them yet.

## Classes of globals

### Resident singleton objects
- `DuelManager gDuelManager`
- `GamemodeManager gGamemodeManager`
- `FramePacer gFramePacer`

These are process-owned objects, not per-generation values.

### Core world/actor pointers
- `Player* gpPlayer`
- `Camera* gpCamera`
- `World* gpWorld`
- `NpcSystem* gpNpcSystem`
- `WeaponSystem* gpWeapons`
- `MimitaNet::MultiplayerContext* gpMpContext`
- `GameState* gpGameState`
- `ChatHistory* gpChatHistory`
- `bool* gpWorldLoaded`

These should become stable handles/accessors in `CAP_ENTITY`/`CAP_WORLD`, not
duplicated game globals.

### Mode/map/session pointers
- `std::string* gpActiveGameMode`
- `std::string* gpActiveMapPath`
- `DuelConfig* gpDuelConfig`
- `bool* gpEditorMode`
- `int* gpSelectedEditorObject`
- `bool* gpFreecamEnabled`
- `glm::vec3* gpDeathPosition`

These are resident session/editor state. Use bounded get/set operations.

### Replay pointers and collections
- `ReplayRingBuffer* gpReplayRecorder`
- `ReplayPlayer* gpReplayPlayer`
- `ReplayFactory* gpReplayFactory`
- `ReplayBrowser* gpReplayBrowser`
- `ReplayTimeline* gpReplayTimeline`
- `ReplayCameraMgr* gpReplayCameraMgr`
- `std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels`
- `std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels`
- `std::unordered_map<std::string, ActorChatState>* gpReplayChatStates`
- `std::vector<std::string>* gpReplayClipsCache`

Replay data is transient/tool state. Do not pass the containers through the ABI.
Provide bounded replay accessors or keep replay code resident until its owner is
decided.

### Bind/debug scalars
- `std::unordered_map<int, std::string>* gpCommandBinds`
- `std::unordered_map<int, bool>* gpBindPrev`
- `bool gReplayExportRenderMode`
- `bool gMainmenuDebug`
- `bool gReplayCinematicMode`
- `bool gNetPresentationDebug`
- `bool gNetDebugEntities`
- `bool gRoomCodeShow`

Debug flags should use `CAP_CONFIG`/`CAP_GUI` read/write operations. Binds should
use a bounded registry query, not direct container access.

## Recommended design

1. Add a **resident global registry** in the host with stable IDs and typed
   get/set operations.
2. Initialize it from `main-systems.cpp` at the same points that currently assign
   the pointer globals.
3. Replace game-side `extern` variable use with host accessor calls by category:
   - `CAP_ENTITY`/`CAP_WORLD`: player, world, NPC, multiplayer/session handles.
   - `CAP_REGISTRY`: weapons, gamemode, duel config, map/mode identifiers.
   - `CAP_GUI`/`CAP_CONFIG`: debug flags and editor toggles.
   - replay accessors: bounded record queries or a dedicated replay service.
4. Do not define duplicate game-DLL globals. A duplicate pointer set is exactly
   the shared-state bug the barrier is meant to prevent.
5. Keep pointer validity per call; game code must not retain host pointers across
   a generation swap.

## Suggested migration order

1. `gpWorld`, `gpPlayer`, `gpNpcSystem`, `gpMpContext`, `gpWorldLoaded`.
2. `gpGameState`, `gpChatHistory`, `gpWeapons`, `gpDuelConfig`.
3. `gpActiveGameMode`, `gpActiveMapPath`, editor/freecam/death-position.
4. Replay pointers/collections.
5. Debug flags and bind maps.

The first five unlock a large share of NPC, networking, collision, and gameplay
call sites.


## Implemented link-closure bridge (2026-11-13)

- ABI: `src/host/abi/global-capability.h` (`GlobalCapabilityV1`, stable ids,
  pointer/scalar get).
- Host: `src/host/host-runtime.cpp` exposes the resident values under
  `HostCapabilityId::Globals`.
- Game: `src/game/global-bridge-module.cpp` defines the game-DLL symbols and
  refreshes pointer/scalar values in `syncHostGlobals()`.
- Timing: `engineTickSetup()` calls `syncHostGlobals()` once, after
  `main-systems.cpp` has assigned the bootstrap pointers.
- Result: 36 global refs -> 0. After the later replay/audio/RenderDevice/
  diagnostics bridges, the whole-game link is 40 undefined / 25 unique with
  0 duplicate definitions.
- Not yet proven: runtime pointer continuity across a generation swap, direct
  writes to copied scalar flags, and whether `DuelManager`,
  `GamemodeManager`, and `FramePacer` should become host handles or remain
  game-owned in the final whole-game architecture.

The bridge is a compatibility seam, not permission to keep duplicate state.
Convert direct global use to bounded capability calls or make the pointer cache
explicitly refreshable per generation before cutover.
