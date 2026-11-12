# C++ audit batch: pure NPC and physics helpers (confirmed static closures)

- Review date: 2026-11-12
- Scope: active EXE-owned NPC and physics helper translation units on the probe/movement path. These are rows closed by a full confirmation read, not by lexical absence alone.
- Source identity: hashes are from `docs/handoffs/cpp-source-inventory.json` at revision `733` rows. Re-review if a hash changes.
- Evidence class: static source review only. No build, test, or runtime launch was performed.

## Disposition summary

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/npc/npc-targeting.cpp` | `cc463556e4b9b2bf40c4fb336966ab5817621b7dc914cc65101888e979c8599f` | `GAME_DLL_READY` (static) |
| `src/npc/npc-movement-context.cpp` | `ad99f2ef8a510a7b20b9812b7b203ee3e60967c4c61547d8f0a80e29877e1c02` | `GAME_DLL_READY` (static) |
| `src/npc/npc-navigation-settings.cpp` | `4b7131291aca0006187f113eaddd13a1b20534c2e31744ed66b747130397bf0a` | `GAME_DLL_READY` (static) |

`GAME_DLL_READY` here means the file has no persistent state, callback, job, thread, or external resource that could re-enter it after unload. It does not prove the whole-game loader/barrier; that remains the probe runtime proof.

## Per-source record

### `src/npc/npc-targeting.cpp`

1. **Identity and build ownership:** 178 lines; current owner `exe`; proposed owner `game`; active in the canonical EXE build; no alternate target. Only the pure policy and a self-test are defined.
2. **Dependencies:** `src/npc/npc-targeting.h` (line 3) and `<limits>` (line 5). Callers: `src/network/server-npcs.cpp` (`npcTargetingIsHostile`, many sites) and the probe module (`selectNpcTargetId`). Both callers are game-owned.
3. **Persistent state:** none. No file-scope static, extern, mutable global, singleton, or thread-local. The only storage is ordinary function-local values in `nearestCandidate` (`:53-71`).
4. **Async and callbacks:** none. No `std::thread`, `std::async`, job queue, or callback registration. `npcTargetingSelfTest` (`:101`) is a synchronous pure check called only by tests/tools.
5. **External resources:** none. No GL, audio, socket, file handle, timer, or OS handle.
6. **ABI and memory ownership:** the public functions take caller-owned values and return by value (`NpcTargetingPolicy`, `NpcTargetCandidate`, `std::vector` in the pure helper). STL is internal to the game side; the host boundary uses the plain `NpcTargetDecisionV1` from `src/host/abi/npc-target-probe-abi.h`. No pointer, allocator-owned object, or exception crosses the proposed boundary.
7. **Disposition and proof:** `GAME_DLL_READY` (static). Safe unload is warranted because no callback, job, thread, or stored function pointer can target this code. The probe exercises the same policy through the plain ABI.

### `src/npc/npc-movement-context.cpp`

1. **Identity and build ownership:** 52 lines; current owner `exe`; proposed owner `game`; active in the canonical EXE build.
2. **Dependencies:** `src/npc/npc-movement-context.h` (line 3). Callers: `src/npc/npc.cpp` (`npcMovementExecutorFromString` at `:154`, `npcMovementExecutorName` at `:1175,1265,1444`) and the self-test. Callers are game-owned.
3. **Persistent state:** none. No static, extern, global, singleton, or thread-local.
4. **Async and callbacks:** none. `npcMovementExecutorSelfTest` (`:24`) is synchronous and tool-only.
5. **External resources:** none.
6. **ABI and memory ownership:** enum/string conversion and a plain context struct; values are caller-owned. `std::string` is internal to the game side; no boundary object is stored.
7. **Disposition and proof:** `GAME_DLL_READY` (static). No re-entry target survives unload.

### `src/npc/npc-navigation-settings.cpp`

1. **Identity and build ownership:** 190 lines; current owner `exe`; proposed owner `game`; active in the canonical EXE build.
2. **Dependencies:** `src/npc/npc-navigation-settings.h` (line 9), `<algorithm>`, `<cmath>`, `<initializer_list>` (`:11-13`). Callers: `src/gamemode/match-roles.cpp` (`parseNpcNavigationSettings` at `:221`), `src/npc/npc.cpp`/`npc-navigator.cpp` (settings reads), and `src/game/game-cli.cpp` (self-test). All are game-owned.
3. **Persistent state:** none. The anonymous namespace (`:15-72`) holds pure helpers only; no static data.
4. **Async and callbacks:** none. `npcNavigationSettingsSelfTest` (`:121`) is synchronous and tool-only.
5. **External resources:** none. The file does not read files or the world; it parses a caller-provided `nlohmann::json`.
6. **ABI and memory ownership:** `nlohmann::json` and `NpcNavigationSettings` stay inside the game side. A resident caller, if any is introduced later, must pass plain validated values rather than the JSON object across the ABI.
7. **Disposition and proof:** `GAME_DLL_READY` (static). No re-entry target survives unload.

## Second confirmation batch: pure policy/surface helpers

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/npc/npc-movement-policy.cpp` | `21e886970b688a363d8d57fa0459cc837e6dd3f7884f7cb0ac9007cc948d3bbc` | `GAME_DLL_READY` (static) |
| `src/npc/npc-nav-request.cpp` | `0cf32df00f2879fed2e9c9fcaab571867d7f9716614d0a587900cc267560c88c` | `GAME_DLL_READY` (static) |
| `src/npc/npc-surface.cpp` | `2e50ec6ffa7debf6538a50573dbe460ae682df0b979282ef45f4d2e4aaf746ee` | `GAME_DLL_READY` (static) |

- **`npc-movement-policy.cpp`:** pure actor-preset policy schema and queries (`parseNpcMovementPolicy` `:66`, health/retreat queries `:111-125`, allowance queries `:152-182`). No static, extern, thread, callback, or resource. Callers are game-owned (`npc.cpp`, `npc-states.cpp`, `npc-traversal.cpp`).
- **`npc-nav-request.cpp`:** pure `NavigationRequest` → `NpcGoal` adapter (`:8`) and segment classification (`:19`). No state, async, or resource; the navigator remains the pathfinding owner.
- **`npc-surface.cpp`:** pure surface/capability naming (`:12,24`), walkable-normal check (`:38`), and surface classification (`:43`). No state, async, or resource.

## Adapter findings from the same read pass

These rows have no unload blocker, but they call a resident service or hold a host-owned pointer, so they need a named adapter before they can be `GAME_DLL_READY`:

| Source | Blocker |
|---|---|
| `src/npc/npc-states.cpp` | `NpcDifficultyConfig::instance()` config owner |
| `src/npc/npc-traversal.cpp` | `ModuleHost::instance()` policy + `MovementJsonConfig::instance()` |
| `src/npc/npc-state-machine.cpp` | `ModuleHost::instance().npcBehaviorHoldPursuit` |
| `src/npc/npc-perception.cpp` | `ModuleHost::instance()` perception calls |
| `src/npc/npc-utility-bridge.cpp` | `ModuleHost::instance()` behavior calls |
| `src/npc/npc-world-query.cpp` | `thread_local const World* gWorld` host pointer |
| `src/npc/team-brain.cpp` | `StructuredLogger::instance()` host logging |

The adapter is not a design failure: the existing subsystem modules already use the `ModuleV1` host-service pattern. These files can either use the host service table or stay resident with a thin bridge.

## Physics confirmation batch: pure mesh integrator

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/physics/mesh-mass-properties.cpp` | `9ed0642f0cdefd2d01e8dbe8d07b67090c16cf08ea803355e48ebb0caaeeb622` | `GAME_DLL_READY` (static) |

- **Persistent state:** none; only function-local doubles/vectors.
- **Async/callbacks/threads:** none.
- **External resources:** none.
- **ABI:** takes `const std::vector<CollisionTriangle>&` and returns a plain struct. Callers `src/impact/destructible-geometry.cpp` and the self-test are game-owned. A resident caller would need a plain span adapter.
- **Disposition:** `GAME_DLL_READY` (static).

## Physics adapter findings

These have no unload blocker, but they mutate host-owned `Player` state or call resident config/debug services, so they need a host-state/debug adapter:

`physics-down-dash.cpp`, `physics-freeze.cpp`, `physics-friction.cpp`, `physics-gravity.cpp`, `physics-ground-return.cpp`, `physics-jump.cpp`, `physics-walk.cpp`, `movement-conversion.cpp`, `physics-collision-glb-safety.cpp`, `ray-utils.cpp`.

## Resident service dependency map

`devscripts/audit_triage.py` now emits `docs/handoffs/resident-service-dependencies-20261112.json`: for each named resident service, the files that call it. It currently covers `StructuredLogger`, `ModuleHost`, `HotReloadSystem`, `LogManager`, `AnalyticsManager`, `LocalProfileSystem`, `Perf`, `serverGamemodeState`, `GamemodeRegistry`, `WeaponRegistry`, `SizeScalingConfig`, `MovementJsonConfig`, `NpcDifficultyConfig`, `BehaviorProfileRegistry`, `ActorCollisionConfig`, and `Debug::log*`. This is the seam inventory: each entry is a host capability the ABI must expose (or a file that stays resident with a bridge).

## Terminal/combat confirmation batch: pure combat helpers

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/combat/attack-timeline.cpp` | `6c8b715e16fd67ba07a22234f90bf4fa9a89b3d73e826913bd6c09c7f2ffd159` | `GAME_DLL_READY` (static) |
| `src/combat/spyknife-damage.cpp` | `5db4abbb2071f1b98830fd45b97a6ba76882ec4c876fc7adaeac71ac75a529f8` | `GAME_DLL_READY` (static) |

- **`attack-timeline.cpp`:** per-instance startup/active/recovery phase timeline; no global, static, async, or resource. Callers are game-owned.
- **`spyknife-damage.cpp`:** pure damage/knockback evaluation over caller-owned `WeaponDefinition` and `ImpactMetrics`; no state or resource.

Terminal command files were reclassified as adapters because they call `Terminal::instance()`, include `terminal/terminal-state.h`, or read `gpMpContext`; `shot-profiler.cpp` is an adapter because of the global `gShotProfiler` pointer.

## Resident service dependency map

`devscripts/audit_triage.py` emits `docs/handoffs/resident-service-dependencies-20261112.json`. After adding terminal/auth/global/gui/map-registry/persistence/character/physics/vehicle detection it covers 398 sources:

`Debug::log*` 195, `StructuredLogger` 161, `Terminal` 100, `TerminalState` 87, `WeaponRegistry` 52, `DebugVis` 48, `GuiLayoutManager` 42, `ModuleHost` 37, `Perf` 35, `PhysicalEntitySystem` 30, `GamemodeRegistry` 25, `MapConfigRegistry` 25, `AuthSystem` 22, `gpMpContext` 21, `serverGamemodeState` 20, `VehicleSystem` 17, `SizeScalingConfig` 16, `NpcDifficultyConfig` 15, `MovementJsonConfig` 13, `AnalyticsManager` 10, `BehaviorProfileRegistry` 9, `VehicleConfigRegistry` 7.

This is the seam inventory: each service is a host capability the ABI must expose, or a file that stays resident with a thin bridge. Logging and terminal state are the two largest seams.

## GUI/impact confirmation batch

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/gui/hud/chat-history.cpp` | `86ec447b79835a7238d00d0816f81ebfe5920ddb53631221c923a323ca566922` | `GAME_DLL_READY` (static) |
| `src/gui/hud/ui-tick-clock.cpp` | `c336389b6b20272cb46402dcdceda3231570f23da840a349677f6e8c796dc969` | `GAME_DLL_READY` (static) |

- **`chat-history.cpp`:** per-instance deque methods; no global, async, or resource. Chat history is transient UI state, so a reload may reset it unless a resident chat-state owner is added later.
- **`ui-tick-clock.cpp`:** per-instance steady-clock accumulator; no global, async, or resource; transient UI timing state.

Other findings in this area:

- `gui-editor-tools.cpp` calls `GuiLayoutManager::instance()` → resident adapter.
- `boolean-mesh.cpp` has three file-scope globals (`gSessionMutex`, `gSessions`, `gSessionClock`) → state review; needs a resident session owner or generation reset.
- `destructible-mesh-loader.cpp` has three file-scope caches (`gCacheMutex`, `gCache`, `gModelCache`) → state review; needs a resident cache owner or generation reset.
- `shot-profiler.cpp` and `weapon-godball.cpp` remain adapter rows.

## Entities/gamemode confirmation batch

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/entities/aim-fov.cpp` | `15c9017ac42e76a6387612ace33ddf271a0e3f6e4df7af5326e3e80002026fc9` | `GAME_DLL_READY` (static) |
| `src/entities/animation-rotation.cpp` | `66e04147b06d61f280d4a050f2dd8c2b6ce513079d1d953ffc2a752181986836` | `GAME_DLL_READY` (static) |
| `src/gamemode/action-graph.cpp` | `b85a44ac9640309c22fbfd124103fe87421b28e6c8be8fd00e70e22a0ec81c9a` | `GAME_DLL_READY` (static) |
| `src/gamemode/disaster-runtime.cpp` | `dc52da42d30b7394a6f0615ff3bcc815052193a34de22a9ddaec59b5b3389d12` | `GAME_DLL_READY` (static) |
| `src/gamemode/payload-runtime.cpp` | `961d4af1844df1742446c2dff299f0b7a6146330dc988f67869b159bcb2a76d0` | `GAME_DLL_READY` (static) |

- **`aim-fov.cpp`:** pure FOV easing/blend/application over caller values.
- **`animation-rotation.cpp`:** pure rotation encoding and shortest-arc SLERP math.
- **`action-graph.cpp`:** pure manifest-to-action scheduling queries; capability ids are constants, no registry call.
- **`disaster-runtime.cpp`:** pure deterministic disaster selection and rules.
- **`payload-runtime.cpp`:** pure deterministic payload runtime over a caller-owned `PayloadRuntime` data struct.
- **`map-edit-applier.cpp`:** adapter; calls `MapConfigRegistry::instance()`.

## Persistence/network/devtools confirmation batch

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/persistence/persistence-rewards.cpp` | `fb07eb087b3329d08eca25856d3df65b21cc588ac509d8587e9b3a87235a5dd6` | `GAME_DLL_READY` (static) |
| `src/persistence/persistence-level.cpp` | `557517040c4c25af2b08d53ea6acd85eafa08edcaf1722c694aea0335586d25b` | `GAME_DLL_READY` (static) |
| `src/actor/actor-intent.cpp` | `2c4affa3e4868326de8b5c12c2846bbe187f0ce768883af759ab8407c0da9e9c` | `GAME_DLL_READY` (static) |
| `src/network/projectile-terminal-dedupe.cpp` | `91ed8b0e013e5a008bf80496a67e7e4b063c4f7422426300bb51913048007382` | `GAME_DLL_READY` (static) |
| `src/analytics/analytics-events.cpp` | `d985d9faea2d3cd456b2c86784a6bced045336b8adaace463834b423e15cc469` | `GAME_DLL_READY` (static) |
| `src/network/chat-rate-limiter.cpp` | `2e33b4d854666c8d41ed3766e59adb6acdba52abab1a552d5caf0174073aa6a0` | `GAME_DLL_READY` (static) |
| `src/network/connection-health.cpp` | `38d5c401d28f913cf2d6de0a5530ab90a35cc75f1031e8b29231642fcf768127` | `GAME_DLL_READY` (static) |
| `src/devtools/command-search.cpp` | `7b3bbfd185294e39854f918d1e3dad08e2a135110f12905012c3993aa87d36fb` | `GAME_DLL_READY` (static) |

- `persistence-rewards` and `analytics-events` hold immutable `const`/`constexpr` tables only.
- `persistence-level`, `actor-intent`, `connection-health`, and `command-search` are pure math/adapter helpers over caller values.
- `projectile-terminal-dedupe` and `chat-rate-limiter` own only per-instance state, no global or async target.
- `persistence-commands.cpp` is an adapter (`PersistenceQueue::instance()`); `avatar-picker.cpp` is an adapter (`CharacterRegistry::instance()`).

## Map/badconn/loader confirmation batch

| Source | SHA-256 | Disposition |
|---|---|---|
| `src/map/map-loader-gltf-builder.cpp` | `bbee5d4eff531162da34f8be09e51bf1d01e0d81e8b162f302a8dae5483f87b6` | `GAME_DLL_READY` (static) |
| `src/map/map-loader-mesh.cpp` | `cadc4fa60d577bd955e0e4f4377fb211abc20ae8e442e81928790cddcadc1a07` | `GAME_DLL_READY` (static) |
| `src/network/badconn/badconn-reorder.cpp` | `7964e725f4d50fd67dda22d7fc47f2284482090f0afe98fb7ca236dc49c97b6c` | `GAME_DLL_READY` (static) |
| `src/network/badconn/badconn-loss.cpp` | `341f2cdbd01f219e0e5dd487fb5fe4c893513e4f0565a2f194f7e93206c7d508` | `GAME_DLL_READY` (static) |
| `src/network/badconn/badconn-blackout.cpp` | `69c059ee8f05bd7c46b2ed31348fe63f8adb570f8aac835181656d6a313cfff1` | `GAME_DLL_READY` (static) |
| `src/network/badconn/badconn-latency.cpp` | `9d4d977eb6939da51b5316b3121c9a5575693136d0030665218dcd5ebde12af3` | `GAME_DLL_READY` (static) |
| `src/map/map-catalog.cpp` | `bf893829cee2657d57c842cd251530e9b52c013ed4770b0ed6bf91a6fd1a5b47` | `GAME_DLL_READY` (static) |
| `src/tiny_obj_loader_declare.cpp` | `73129ee086acc9d8a977ea3504e3871925d35d8ae925348d238e6902d2ff31d3` | `GAME_DLL_READY` (static) |
| `src/tinygltf_declare.cpp` | `9c28c83aba7af06fd2a10f2d1bf7cf7856e9d2a309ff5a18e7ff728156689e6c` | `GAME_DLL_READY` (static) |
| `src/utils/stb_image_impl.cpp` | `a8e271a7f22e4ef7acca000992df73cd1f7f0fce9beb667a221e1b46a5f94224` | `GAME_DLL_READY` (static) |

- The badconn files are pure per-packet/preset functions over caller `BadConn*` state and RNG.
- The map helpers are pure mesh/catalog functions; the builder TU is empty.
- The tiny/stb TUs only instantiate header-library implementations; no host service or global target in them.
- `engine-frame.cpp` is an adapter to the resident renderer/device.
- `map-loader-gltf-verts.cpp` is an adapter to `Debug::logAuto` GLB logging.
- `perf-allocator.cpp` overrides global `operator new/delete` and must stay resident.

## Final static-candidate batch: Recast/Detour algorithms

All 11 active Recast/Detour translation units were closed by a confirmation scan: their `static` declarations are static helper functions, not mutable process state. None calls a host service, starts a thread, registers a callback, or holds a GL/socket handle. `rcAlloc`/`rcFree` are caller-set macros, not stored pointers in these files.

| Sources | Disposition |
|---|---|
| `external/recastnavigation/Detour/Source/{DetourCommon,DetourNavMesh,DetourNavMeshQuery,DetourNode}.cpp` | `GAME_DLL_READY` (static) |
| `external/recastnavigation/DetourTileCache/Source/DetourTileCacheBuilder.cpp` | `GAME_DLL_READY` (static) |
| `external/recastnavigation/Recast/Source/{Recast,RecastArea,RecastFilter,RecastLayers,RecastMeshDetail,RecastRegion}.cpp` | `GAME_DLL_READY` (static) |

Newly classified adapters in this pass:

- `src/physics/constraints/constraint-debug.cpp` → `ConstraintSystem`, `PhysicalEntitySystem`, `DebugVis`.
- `src/vehicles/vehicle-render.cpp` → `VehicleSystem`, `VehicleConfigRegistry`, `PhysicalEntitySystem`.

**Milestone:** `STATIC_CLOSABLE_CANDIDATE=0`. Every row that looked clean by lexical scan has now had a confirmation read; the remaining rows are explicitly state-review, adapter, runtime-proof, resident-host, or non-game.

## What this batch does not prove

- It does not prove the whole-game loader, barrier, swap, fallback, or rollback. That needs the probe runtime proof.
- It does not close the callers (`server-npcs.cpp`, `npc.cpp`, `match-roles.cpp`, physics step owners), which carry mutable state and need their own records.
- It does not cover any other row. The full triage is `docs/handoffs/cpp-audit-static-triage-20261112.md`; at this revision 56 rows are statically closed, 0 are `STATIC_CLOSABLE_CANDIDATE`, 40 are `STATIC_STATE_REVIEW`, 188 are `RESIDENT_ADAPTER_REQUIRED`, 291 are `RUNTIME_PROOF_REQUIRED`, 23 are resident host, and 135 are non-game.
