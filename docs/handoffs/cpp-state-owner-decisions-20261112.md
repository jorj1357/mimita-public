# State-owner decisions (Pass B, 2026-11-12)

**Status:** per-symbol static map for the 40 `STATIC_STATE_REVIEW` rows.
`CONFIRMED` means the confirmation read decided the lifetime; `PROPOSED` means
the default is reasonable but the owner path still needs a trace; `NEEDS_READ`
is a blocking unknown. This is not runtime evidence.

- Machine table: `docs\handoffs\cpp-state-owner-decisions-20261112.csv`
- Rows reviewed: 40
- Symbols recorded: 145

## Decision counts

| Decision | Symbols |
|---|---:|
| NO_STATE | 88 |
| RESIDENT_OWNER | 21 |
| GENERATION_SCOPED_RESET | 20 |
| FREEZE_IN_GAME_DLL | 11 |
| ADAPTER_INVENTORY | 5 |

## State counts

| State | Symbols |
|---|---:|
| CONFIRMED | 145 |

## Decision meanings

- `NO_STATE`: static helper; nothing to move.
- `FREEZE_IN_GAME_DLL`: immutable const/constexpr; ship as module data.
- `GENERATION_SCOPED_RESET`: cache/timer/transient state; reset in `initialize`.
- `RESIDENT_OWNER`: durable state or process-wide hook; must live in the host.
- `RESIDENT_OWNER_OR_RESET`: file-scope state that needs a decision.
- `ADAPTER_INVENTORY`: extern dependency; map to a capability or resident owner.

## Per-file symbols

### `external/recastnavigation/Detour/Source/DetourAlloc.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 27 | `dtFreeDefault` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 32 | `sAllocFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide allocator hook; caller-set |
| 33 | `sFreeFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide allocator hook; caller-set |

### `external/recastnavigation/Detour/Source/DetourAssert.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 23 | `sAssertFailFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide assert hook; caller-set |

### `external/recastnavigation/Detour/Source/DetourNavMeshBuilder.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 30 | `MESH_NULL_IDX` | STATIC_VARIABLE | FREEZE_IN_GAME_DLL | CONFIRMED | sentinel constant |
| 40 | `compareItemX` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 51 | `compareItemY` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 62 | `compareItemZ` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 73 | `calcExtends` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 113 | `subdivide` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 171 | `createBVTree` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 243 | `classifyOffMeshPoint` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 245 | `XP` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |
| 246 | `ZP` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |
| 247 | `XM` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |
| 248 | `ZM` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |

### `external/recastnavigation/DetourTileCache/Source/DetourTileCache.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 26 | `contains` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `external/recastnavigation/Recast/Source/RecastAlloc.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 21 | `rcAllocDefault` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 26 | `rcFreeDefault` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 31 | `sRecastAllocFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide allocator hook; caller-set |
| 32 | `sRecastFreeFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide allocator hook; caller-set |

### `external/recastnavigation/Recast/Source/RecastAssert.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 21 | `sRecastAssertFailFunc` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | process-wide assert hook; caller-set |

### `external/recastnavigation/Recast/Source/RecastContour.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 28 | `getCornerHeight` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 103 | `walkContour` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 186 | `distancePtSeg` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 209 | `simplifyContour` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 453 | `calcAreaOfPolygon2D` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 495 | `intersectProp` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 507 | `between` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 519 | `intersect` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 530 | `vequal` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 535 | `intersectSegContour` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 555 | `inCone` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 570 | `removeDegenerateSegments` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 596 | `mergeContours` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 660 | `findLeftMostVertex` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 678 | `compareHoles` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 700 | `compareDiagDist` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 712 | `mergeRegionHoles` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `external/recastnavigation/Recast/Source/RecastMesh.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 33 | `buildMeshAdjacency` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 125 | `VERTEX_BUCKET_COUNT` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |
| 136 | `addVertex` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 191 | `intersectProp` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 203 | `between` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 215 | `intersect` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 227 | `vequal` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 234 | `diagonalie` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 261 | `inCone` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 278 | `diagonal` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 284 | `diagonalieLoose` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 309 | `inConeLoose` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 324 | `diagonalLoose` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 330 | `triangulate` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 444 | `countPolyVerts` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 458 | `getPolyMergeValue` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 522 | `mergePolyVerts` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 542 | `pushFront` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 549 | `pushBack` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 555 | `canRemoveVertex` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 655 | `removeVertex` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `external/recastnavigation/Recast/Source/RecastRasterization.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 31 | `overlapBounds` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 44 | `allocSpan` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 84 | `freeSpan` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 105 | `addSpan` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 232 | `dividePoly` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 313 | `rasterizeTri` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/camera.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 147 | `logTimer` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | transient debug throttle |

### `src/combat/death-system-bodies.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 15 | `gRenderer` | EXTERN | ADAPTER_INVENTORY | CONFIRMED | host renderer service |

### `src/combat/pellet-pattern.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 21 | `nextSpreadRand` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 27 | `spreadUnit` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/combat/weapon-execution.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 20 | `finiteVec3` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 25 | `safeNormalize` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 32 | `closestPointOnSegment` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 44 | `rayAabb` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 76 | `sweptPointSphere` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/competitive/competitive-match.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 9 | `gCompetitiveMatchActive` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | match/session state |
| 10 | `gCompetitiveOpponentMmr` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | match/session state |

### `src/debug/debug-visuals-render.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 17 | `gTriVerts` | EXTERN | ADAPTER_INVENTORY | CONFIRMED | debug draw buffer -> CAP_DEBUG_VIS |
| 18 | `gOverlayTriVerts` | EXTERN | ADAPTER_INVENTORY | CONFIRMED | debug draw buffer -> CAP_DEBUG_VIS |
| 19 | `gSolidTriVerts` | EXTERN | ADAPTER_INVENTORY | CONFIRMED | debug draw buffer -> CAP_DEBUG_VIS |
| 22 | `gCameraWorldOverlay` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | transient overlay state |
| 23 | `gCameraHudOverlay` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | transient overlay state |
| 159 | `emitFilledSphere` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 211 | `impactBasis` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 414 | `drawCrosshairRect` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/debug/entity-visibility.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 15 | `gMaster` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | debug visibility setting |
| 16 | `gCategories` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | debug visibility settings |

### `src/debug/transform-debug.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 7 | `dbg` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | debug state; reset on activation |

### `src/devtools/dev-commands.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 5 | `sInstance` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | command registry; resident tool service |

### `src/effects/zombie-tower-effects.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 5 | `effects` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | effect state; reset on activation |

### `src/fister/sim/Ragdoll.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 91 | `links` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |

### `src/game/probe/npc-target-probe-module.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 139 | `api` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |

### `src/gamemode/capability-registry.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 15 | `registry` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | capability catalog; reload from data |

### `src/gui/gui-coord.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 6 | `sys` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | coordinate config; reload from data |

### `src/gui/hud/match-timer.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 17 | `timer` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | match timer must survive a swap |

### `src/gui/menus/menu-avatar-preview-debug.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 8 | `readV3` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 141 | `printSec` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 146 | `printSec` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 151 | `printSec` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 156 | `printSec3` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 162 | `printChanges` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/gui/ui-tooltip.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 11 | `gTooltipMap` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | UI asset cache; reset and reload |
| 12 | `gLastHovered` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | transient hover state |
| 13 | `gHoverStart` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | transient hover timer |
| 14 | `gFadingText` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | transient fade state |
| 15 | `gFadeStart` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | transient fade timer |

### `src/impact/boolean-mesh.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 328 | `gSessionMutex` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | guards resident sessions |
| 329 | `gSessions` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | live boolean sessions keyed by stable id |
| 330 | `gSessionClock` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | session id clock; resident with sessions |

### `src/impact/destructible-geometry.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 39 | `capsuleDistance` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 146 | `gNextSessionId` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | session id clock |
| 293 | `system` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | destructible world geometry owner |

### `src/impact/destructible-mesh-loader.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 189 | `gCacheMutex` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | guards resettable cache |
| 190 | `gCache` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | parsed mesh cache; first access rebuilds |
| 193 | `gModelCache` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | parsed GLB cache; first access rebuilds |

### `src/map/map-loader-gltf.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 21 | `gTextures` | EXTERN | ADAPTER_INVENTORY | CONFIRMED | host texture store -> CAP_RESOURCES |

### `src/network/badconn/badconn-registry.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 34 | `handlers` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |

### `src/network/server-contact.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 303 | `seen` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | bounded dedupe cache |
| 304 | `order` | STATIC_VARIABLE | GENERATION_SCOPED_RESET | CONFIRMED | bounded dedupe cache |

### `src/npc/npc-nav-graph.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 23 | `gMeshMinZ` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | derived from world; recompute on activation |
| 24 | `gMeshMaxZ` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | derived from world; recompute on activation |
| 106 | `graph` | STATIC_VARIABLE | RESIDENT_OWNER | CONFIRMED | nav graph; validates against current world |
| 226 | `DX` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |
| 227 | `DY` | IMMUTABLE | FREEZE_IN_GAME_DLL | CONFIRMED | const/constexpr value |

### `src/physics/movement/physics-collision-glb-sweep.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 60 | `pointSegmentDistanceSq` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 101 | `sweepSpherePoint` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/physics/movement/physics-collision-stress.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 16 | `addStressTriangle` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 30 | `addStressQuad` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 36 | `addStressFloor` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 45 | `addStressWedge` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 65 | `addStressCone` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 93 | `runCollisionStressCase` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/replay/replay.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 8 | `gActiveReplayRecorder` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | active capture must survive a code swap |
| 9 | `gReplayCaptureEnabled` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | capture policy must survive a code swap |
| 10 | `gReplayFactoryNotifyFn` | FILE_GLOBAL | RESIDENT_OWNER | CONFIRMED | callback target; host-owned registration |

### `src/ui/hitmarker.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 18 | `gHitmarkerTimer` | FILE_GLOBAL | GENERATION_SCOPED_RESET | CONFIRMED | transient UI timer |
| 20 | `hitmarkerDuration` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 25 | `hitmarkerSize` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/website/api-client.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 308 | `parseUserInfo` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 336 | `jsonInt` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 345 | `jsonLong` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 354 | `jsonFloat` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 363 | `parseStats` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

### `src/world/world.cpp`

| Line | Symbol | Kind | Decision | State | Reason |
|---:|---|---|---|---|---|
| 30 | `chunkCoord` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |
| 40 | `eulerXYZDegToMat3` | STATIC_FUNCTION | NO_STATE | CONFIRMED | static helper; no persistent state |

## Exit criteria

- Every `NEEDS_READ` symbol is traced and given a decision.
- Every `RESIDENT_OWNER` symbol has a resident record or service owner.
- Every `GENERATION_SCOPED_RESET` symbol has a reset site in `initialize`.
- Every `ADAPTER_INVENTORY` symbol appears in Pass C.
- No symbol relies on a per-file runtime test; the barrier rule covers it.
