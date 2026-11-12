# Capability map (Pass C, 2026-11-12)

**Status:** static mapping from resident-service calls to proposed host capabilities.
This is the seam inventory for `HostCapabilitiesV1`, not an implementation.

- Machine table: `docs/handoffs/cpp-capability-map-20261112.csv`
- Adapter rows: 182

## Method

1. Take every `RESIDENT_ADAPTER_REQUIRED` row.
2. Read its named services from `resident-service-dependencies-20261112.json`.
3. Map each service to a capability ID and operation.
4. A row with multiple services lists every capability it needs.
5. `UNMAPPED` means a new capability or a resident bridge is required.

## Capability retirement counts

| Capability | Rows | Seed operations |
|---|---:|---|
| CAP_LOG | 104 | event/text (bounded JSON) |
| CAP_TERMINAL | 62 | emit, state get/set |
| CAP_REGISTRY | 35 | weapon/gamemode/map/vehicle/character/profile get |
| CAP_DEBUG_VIS | 21 | line/box/label queue |
| CAP_ENTITY | 21 | read/write by stable handle |
| CAP_PERF | 20 | scope begin/end, counter |
| CAP_CONFIG | 19 | get config snapshot by id/revision |
| CAP_MODULES | 17 | existing subsystem policy call (bridge) |
| CAP_PHYSICS | 11 | constraint/vehicle query by handle |
| CAP_GUI | 7 | element/text read/write |
| CAP_AUTH | 5 | identity/status read |
| CAP_WORLD | 3 | broadphase candidates + read-only geometry spans |
| CAP_RESOURCES | 2 | allocate/release per generation |
| CAP_GAMEMODE | 1 | match state read |
| CAP_PERSISTENCE | 1 | submit/flush |
| CAP_RENDER | 1 | generic draw command stream |
| CAP_TRANSPORT | 1 | opaque send/recv and session read |

## Seed HostCapabilitiesV1

Each capability is a versioned plain-data struct with `version` and `structSize`.

| Capability | Non-negotiable operations |
|---|---|
| `CAP_LOG` | bounded UTF-8 JSON event + text; category/level/tick/event name |
| `CAP_TERMINAL` | emit text, read/write terminal state (no GUI ownership) |
| `CAP_GUI` | read layout element/text, write text/visibility by stable id |
| `CAP_DEBUG_VIS` | submit bounded line/box/label records to the host draw queue |
| `CAP_PERF` | begin/end scope, counters; host aggregates |
| `CAP_REGISTRY` | lookups by stable id returning plain bounded records |
| `CAP_CONFIG` | revisioned config snapshot by id |
| `CAP_PERSISTENCE` | bounded submit/flush request/result |
| `CAP_ENTITY` | read/write bounded entity snapshots by handle |
| `CAP_PHYSICS` | constraint/vehicle queries by handle, plain results |
| `CAP_TRANSPORT` | send/recv opaque `{channel,typeId,bytes}`; session state read |
| `CAP_RENDER` | generic draw command stream + shader/resource handles |
| `CAP_WORLD` | broadphase candidates + read-only triangle/entity spans |
| `CAP_JOBS` | submit/cancel/drain/join, generation-stamped |
| `CAP_CALLBACKS` | register/unregister with generation token |
| `CAP_RESOURCES` | allocate/release GPU/audio/net resources per generation |

`CAP_RENDER`, `CAP_WORLD`, `CAP_JOBS`, `CAP_CALLBACKS`, and `CAP_RESOURCES`
have no adapter rows yet because their current call sites are classified as
runtime-proof or state rows; they are still mandatory for the hot targets.

## Full row table

| Row | Lines | Services | Capabilities |
|---|---:|---|---|
| `src/actor/player-ai.cpp` | 344 | StructuredLogger,Terminal,TerminalState,WeaponRegistry | CAP_LOG,CAP_TERMINAL,CAP_REGISTRY |
| `src/audio/audio-acoustics.cpp` | 139 |  | CAP_WORLD |
| `src/audio/audio-commands.cpp` | 82 | Terminal | CAP_TERMINAL |
| `src/audio/music-commands.cpp` | 235 | DebugLog,Terminal | CAP_LOG,CAP_TERMINAL |
| `src/auth/auth-controller.cpp` | 292 | AuthSystem,DebugLog | CAP_AUTH,CAP_LOG |
| `src/avatar/avatar-picker.cpp` | 58 | CharacterRegistry | CAP_REGISTRY |
| `src/camera/camera-commands.cpp` | 126 | Terminal,TerminalState | CAP_TERMINAL |
| `src/combat/actor-preset-weapons.cpp` | 296 | WeaponRegistry | CAP_REGISTRY |
| `src/combat/client-collision-world-view.cpp` | 223 | PhysicalEntitySystem | CAP_ENTITY |
| `src/combat/explosion-fx.cpp` | 219 | StructuredLogger,WeaponRegistry | CAP_LOG,CAP_REGISTRY |
| `src/combat/grenade-registry.cpp` | 181 | DebugLog,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_REGISTRY |
| `src/combat/projectile-simulation.cpp` | 679 |  | CAP_PHYSICS |
| `src/combat/shot-profiler.cpp` | 151 |  | CAP_PERF |
| `src/combat/weapon-attachment.cpp` | 81 | SizeScalingConfig | CAP_CONFIG |
| `src/combat/weapon-config.cpp` | 145 | Terminal | CAP_TERMINAL |
| `src/combat/weapon-contact-local.cpp` | 119 | WeaponRegistry | CAP_REGISTRY |
| `src/combat/weapon-data.cpp` | 1125 | DebugLog,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_REGISTRY |
| `src/combat/weapon-fire-effects.cpp` | 208 | DebugLog,SizeScalingConfig,StructuredLogger,Terminal | CAP_LOG,CAP_CONFIG,CAP_TERMINAL |
| `src/combat/weapon-fire-hit.cpp` | 491 | DebugLog,Perf | CAP_LOG,CAP_PERF |
| `src/combat/weapon-fire-raycast.cpp` | 567 | DebugLog | CAP_LOG |
| `src/combat/weapon-godball-physics.cpp` | 226 | DebugVis | CAP_DEBUG_VIS |
| `src/combat/weapon-godball.cpp` | 157 |  | CAP_ENTITY |
| `src/combat/weapon-grenade-launcher.cpp` | 230 | DebugLog,SizeScalingConfig,StructuredLogger | CAP_LOG,CAP_CONFIG |
| `src/combat/weapon-hafs.cpp` | 302 | DebugLog | CAP_LOG |
| `src/combat/weapon-hit.cpp` | 78 | DebugLog | CAP_LOG |
| `src/combat/weapon-manager.cpp` | 122 | Terminal,WeaponRegistry | CAP_TERMINAL,CAP_REGISTRY |
| `src/combat/weapon-quick-hit.cpp` | 363 | DebugLog,DebugVis,HotReloadSystem | CAP_LOG,CAP_DEBUG_VIS,CAP_MODULES |
| `src/combat/weapon-registry.cpp` | 80 | WeaponRegistry | CAP_REGISTRY |
| `src/combat/weapon-rocket-launcher.cpp` | 674 | DebugLog,DebugVis,SizeScalingConfig,StructuredLogger | CAP_LOG,CAP_DEBUG_VIS,CAP_CONFIG |
| `src/combat/weapon-swordsword.cpp` | 647 | DebugLog | CAP_LOG |
| `src/combat/weapon-system-equip.cpp` | 176 | DebugLog,WeaponRegistry | CAP_LOG,CAP_REGISTRY |
| `src/competitive/competitive-commands.cpp` | 134 | Terminal | CAP_TERMINAL |
| `src/crosshair/crosshair-commands.cpp` | 103 | Terminal | CAP_TERMINAL |
| `src/crosshair/crosshair-render.cpp` | 120 | StructuredLogger | CAP_LOG |
| `src/debug/debug-log.cpp` | 191 | DebugLog,StructuredLogger | CAP_LOG |
| `src/debug/transform-debug-commands.cpp` | 60 | Terminal | CAP_TERMINAL |
| `src/devtools/dev-npc-selection.cpp` | 144 | DebugVis | CAP_DEBUG_VIS |
| `src/devtools/dev-overlay-commands.cpp` | 78 | DebugLog,Terminal,TerminalState | CAP_LOG,CAP_TERMINAL |
| `src/devtools/dev-teleport.cpp` | 115 | DebugLog | CAP_LOG |
| `src/devtools/npc-spawn-commands.cpp` | 159 | TerminalState | CAP_TERMINAL |
| `src/devtools/scenario-commands.cpp` | 257 | StructuredLogger,Terminal,TerminalState | CAP_LOG,CAP_TERMINAL |
| `src/devtools/terminal-builtins.cpp` | 107 | Terminal | CAP_TERMINAL |
| `src/devtools/terminal-render.cpp` | 248 |  | CAP_TERMINAL |
| `src/effects/effect-part-blood.cpp` | 677 | DebugLog,StructuredLogger | CAP_LOG |
| `src/effects/effect-part-debris.cpp` | 47 | SizeScalingConfig | CAP_CONFIG |
| `src/effects/effect-part-particles.cpp` | 144 | DebugLog | CAP_LOG |
| `src/effects/effect-part-system.cpp` | 154 | DebugLog,HotReloadSystem | CAP_LOG,CAP_MODULES |
| `src/effects/effect-part.cpp` | 839 | DebugLog,SizeScalingConfig,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_CONFIG,CAP_REGISTRY |
| `src/effects/hit-effects-render.cpp` | 180 | DebugVis | CAP_DEBUG_VIS |
| `src/effects/hit-effects-spawn.cpp` | 407 | DebugLog,StructuredLogger | CAP_LOG |
| `src/effects/hit-effects.cpp` | 294 | DebugLog,Perf,WeaponRegistry | CAP_LOG,CAP_PERF,CAP_REGISTRY |
| `src/effects/hitfx-commands.cpp` | 331 | DebugLog,StructuredLogger,Terminal | CAP_LOG,CAP_TERMINAL |
| `src/effects/rain-system.cpp` | 55 | DebugVis | CAP_DEBUG_VIS |
| `src/engine/engine-frame.cpp` | 23 |  | CAP_RENDER |
| `src/engine/engine-tick-audio.cpp` | 13 | Perf,TerminalState | CAP_PERF,CAP_TERMINAL |
| `src/engine/engine-tick-combat.cpp` | 644 | DebugLog,Perf,PhysicalEntitySystem,StructuredLogger,Terminal,TerminalState | CAP_LOG,CAP_PERF,CAP_ENTITY,CAP_TERMINAL |
| `src/engine/engine-tick-state.cpp` | 512 | AuthSystem,DebugLog,StructuredLogger,Terminal,TerminalState | CAP_AUTH,CAP_LOG,CAP_TERMINAL |
| `src/engine/engine-tick-ui-game-hud.cpp` | 645 | DebugLog,DebugVis,GamemodeRegistry,GuiLayoutManager,StructuredLogger,TerminalState,WeaponRegistry | CAP_LOG,CAP_DEBUG_VIS,CAP_REGISTRY,CAP_GUI,CAP_TERMINAL |
| `src/engine/engine-tick-ui-hud.cpp` | 96 | TerminalState | CAP_TERMINAL |
| `src/engine/engine-tick-ui-overlays.cpp` | 1276 | DebugVis,GamemodeRegistry,GuiLayoutManager,MapConfigRegistry,Perf,StructuredLogger,TerminalState | CAP_DEBUG_VIS,CAP_REGISTRY,CAP_GUI,CAP_PERF,CAP_LOG,CAP_TERMINAL |
| `src/engine/engine-tick-ui.cpp` | 199 | DebugLog,DebugVis,Perf,TerminalState | CAP_LOG,CAP_DEBUG_VIS,CAP_PERF,CAP_TERMINAL |
| `src/entities/aim-commands.cpp` | 109 | Terminal | CAP_TERMINAL |
| `src/entities/player-animation.cpp` | 1643 | DebugLog,GamemodeRegistry,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_REGISTRY |
| `src/entities/player.cpp` | 531 | DebugLog,StructuredLogger | CAP_LOG |
| `src/fister/fister-port.cpp` | 141 | DebugLog,TerminalState | CAP_LOG,CAP_TERMINAL |
| `src/game/bomb-tag-state.cpp` | 60 | TerminalState | CAP_TERMINAL |
| `src/game/duel-state.cpp` | 211 | DebugLog | CAP_LOG |
| `src/game/gamemode-manager.cpp` | 370 | DebugLog,DebugVis,GamemodeRegistry,GuiLayoutManager,MapConfigRegistry,TerminalState | CAP_LOG,CAP_DEBUG_VIS,CAP_REGISTRY,CAP_GUI,CAP_TERMINAL |
| `src/game/spawn-override.cpp` | 36 | DebugLog | CAP_LOG |
| `src/gamemode/arena-ball.cpp` | 133 | PhysicalEntitySystem,StructuredLogger | CAP_ENTITY,CAP_LOG |
| `src/gamemode/gamemode-runtime-resolver.cpp` | 188 | DebugLog | CAP_LOG |
| `src/gamemode/map-edit-applier.cpp` | 388 | MapConfigRegistry | CAP_REGISTRY |
| `src/gui/gui-editor-commands.cpp` | 208 | GuiLayoutManager,Terminal | CAP_GUI,CAP_TERMINAL |
| `src/gui/gui-editor-tools.cpp` | 103 | GuiLayoutManager | CAP_GUI |
| `src/gui/hud/chat-bubble.cpp` | 309 | DebugVis,GuiLayoutManager | CAP_DEBUG_VIS,CAP_GUI |
| `src/gui/hud/match-leaderboard.cpp` | 238 | GuiLayoutManager | CAP_GUI |
| `src/gui/hud/player-nameplates.cpp` | 442 | DebugLog,DebugVis,StructuredLogger,TerminalState | CAP_LOG,CAP_DEBUG_VIS,CAP_TERMINAL |
| `src/impact/impact-system.cpp` | 811 | DebugLog,Perf,PhysicalEntitySystem,StructuredLogger | CAP_LOG,CAP_PERF,CAP_ENTITY |
| `src/interaction/interaction-commands.cpp` | 51 | Terminal,TerminalState | CAP_TERMINAL |
| `src/interaction/world-interaction-runtime.cpp` | 233 | MapConfigRegistry,StructuredLogger,Terminal,TerminalState | CAP_REGISTRY,CAP_LOG,CAP_TERMINAL |
| `src/killfeed/killfeed.cpp` | 330 | DebugLog,TerminalState,WeaponRegistry | CAP_LOG,CAP_TERMINAL,CAP_REGISTRY |
| `src/map/map-glb-authored.cpp` | 67 | StructuredLogger | CAP_LOG |
| `src/map/map-loader-collision.cpp` | 899 | StructuredLogger | CAP_LOG |
| `src/map/map-loader-gltf-accessors.cpp` | 223 |  | CAP_LOG |
| `src/map/map-loader-gltf-verts.cpp` | 90 |  | CAP_LOG |
| `src/map/map-static-union.cpp` | 1178 | StructuredLogger | CAP_LOG |
| `src/mapeditor/map-editor-commands.cpp` | 886 | MapConfigRegistry,ModuleHost,StructuredLogger,Terminal,TerminalState | CAP_REGISTRY,CAP_MODULES,CAP_LOG,CAP_TERMINAL |
| `src/mapeditor/world-edit-commands.cpp` | 494 | MapConfigRegistry,StructuredLogger,Terminal | CAP_REGISTRY,CAP_LOG,CAP_TERMINAL |
| `src/network/actor-lifecycle.cpp` | 36 | DebugLog | CAP_LOG |
| `src/network/community-match-client.cpp` | 1151 | AuthSystem,DebugLog,GamemodeRegistry,MovementJsonConfig,StructuredLogger,TerminalState | CAP_AUTH,CAP_LOG,CAP_REGISTRY,CAP_CONFIG,CAP_TERMINAL |
| `src/network/coordinator-client.cpp` | 738 | DebugLog | CAP_LOG |
| `src/network/movement-validation.cpp` | 867 | ModuleHost | CAP_MODULES |
| `src/network/remote-entity-lifecycle.cpp` | 61 |  | CAP_ENTITY |
| `src/network/server-infinite-collision.cpp` | 756 | MapConfigRegistry,ModuleHost,StructuredLogger | CAP_REGISTRY,CAP_MODULES,CAP_LOG |
| `src/network/server-world-destruction.cpp` | 511 | GamemodeRegistry,PhysicalEntitySystem,StructuredLogger,serverGamemodeState | CAP_REGISTRY,CAP_ENTITY,CAP_LOG,CAP_GAMEMODE |
| `src/notifications/notification-commands.cpp` | 193 | Terminal | CAP_TERMINAL |
| `src/npc/npc-combat.cpp` | 762 | DebugLog,ModuleHost,NpcDifficultyConfig,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_MODULES,CAP_CONFIG,CAP_REGISTRY |
| `src/npc/npc-navigation.cpp` | 467 | NpcDifficultyConfig | CAP_CONFIG |
| `src/npc/npc-navigator.cpp` | 1367 | DebugLog,MovementJsonConfig,NpcDifficultyConfig,Perf | CAP_LOG,CAP_CONFIG,CAP_PERF |
| `src/npc/npc-perception.cpp` | 414 | ModuleHost | CAP_MODULES |
| `src/npc/npc-spawn.cpp` | 425 | DebugLog,NpcDifficultyConfig,Perf,WeaponRegistry | CAP_LOG,CAP_CONFIG,CAP_PERF,CAP_REGISTRY |
| `src/npc/npc-state-machine.cpp` | 376 | ModuleHost | CAP_MODULES |
| `src/npc/npc-states.cpp` | 319 | NpcDifficultyConfig | CAP_CONFIG |
| `src/npc/npc-traversal.cpp` | 255 | DebugLog,ModuleHost,MovementJsonConfig | CAP_LOG,CAP_MODULES,CAP_CONFIG |
| `src/npc/npc-utility-bridge.cpp` | 322 | ModuleHost | CAP_MODULES |
| `src/npc/npc-world-query.cpp` | 57 |  | CAP_WORLD |
| `src/npc/npc.cpp` | 3843 | BehaviorProfileRegistry,DebugLog,DebugVis,ModuleHost,NpcDifficultyConfig,Perf,StructuredLogger,WeaponRegistry | CAP_REGISTRY,CAP_LOG,CAP_DEBUG_VIS,CAP_MODULES,CAP_CONFIG,CAP_PERF |
| `src/npc/recast-navigation.cpp` | 1465 | ModuleHost,Perf,StructuredLogger | CAP_MODULES,CAP_PERF,CAP_LOG |
| `src/npc/team-brain.cpp` | 747 | StructuredLogger | CAP_LOG |
| `src/perf/perf-allocator.cpp` | 62 |  | CAP_RESOURCES |
| `src/perf/perf-commands.cpp` | 453 | Perf,StructuredLogger,Terminal | CAP_PERF,CAP_LOG,CAP_TERMINAL |
| `src/perf/perf-overlay.cpp` | 482 | DebugLog,Perf | CAP_LOG,CAP_PERF |
| `src/perf/perf-spike.cpp` | 674 | DebugLog,Perf,StructuredLogger | CAP_LOG,CAP_PERF |
| `src/persistence/persistence-commands.cpp` | 37 | PersistenceQueue | CAP_PERSISTENCE |
| `src/physics/constraints/constraint-commands.cpp` | 416 | ConstraintSystem,PhysicalEntitySystem,StructuredLogger,Terminal,TerminalState | CAP_PHYSICS,CAP_ENTITY,CAP_LOG,CAP_TERMINAL |
| `src/physics/constraints/constraint-debug.cpp` | 108 | ConstraintSystem,DebugVis,PhysicalEntitySystem | CAP_PHYSICS,CAP_DEBUG_VIS,CAP_ENTITY |
| `src/physics/constraints/constraint-system.cpp` | 393 | ConstraintSystem,PhysicalEntitySystem | CAP_PHYSICS,CAP_ENTITY |
| `src/physics/movement/actor-collision-mesh.cpp` | 475 | WeaponRegistry | CAP_REGISTRY |
| `src/physics/movement/actor-final-pose.cpp` | 766 | ModuleHost | CAP_MODULES |
| `src/physics/movement/actor-pair-collision.cpp` | 988 | ModuleHost,StructuredLogger | CAP_MODULES,CAP_LOG |
| `src/physics/movement/actor-triangle-solver.cpp` | 2211 | ModuleHost,PhysicalEntitySystem,StructuredLogger | CAP_MODULES,CAP_ENTITY,CAP_LOG |
| `src/physics/movement/movement-conversion.cpp` | 328 | MovementJsonConfig,SizeScalingConfig | CAP_CONFIG |
| `src/physics/movement/movement-step.cpp` | 1894 | StructuredLogger | CAP_LOG |
| `src/physics/movement/physics-collision-block.cpp` | 367 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-collision-body.cpp` | 415 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-collision-core.cpp` | 578 | DebugLog,DebugVis,ModuleHost,StructuredLogger | CAP_LOG,CAP_DEBUG_VIS,CAP_MODULES |
| `src/physics/movement/physics-collision-dispatch.cpp` | 584 | DebugLog,DebugVis,Perf,StructuredLogger | CAP_LOG,CAP_DEBUG_VIS,CAP_PERF |
| `src/physics/movement/physics-collision-glb-body.cpp` | 288 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-collision-glb-contact.cpp` | 345 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-collision-glb-main.cpp` | 576 | DebugLog,DebugVis,Perf,StructuredLogger | CAP_LOG,CAP_DEBUG_VIS,CAP_PERF |
| `src/physics/movement/physics-collision-glb-safety.cpp` | 88 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-collision-glb-setup.cpp` | 444 | DebugLog,Perf | CAP_LOG,CAP_PERF |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` | 566 | DebugLog,DebugVis | CAP_LOG,CAP_DEBUG_VIS |
| `src/physics/movement/physics-collision-mesh.cpp` | 1031 | StructuredLogger | CAP_LOG |
| `src/physics/movement/physics-collision.cpp` | 962 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-dash.cpp` | 101 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-down-dash.cpp` | 35 |  | CAP_ENTITY |
| `src/physics/movement/physics-freeze.cpp` | 33 |  | CAP_ENTITY |
| `src/physics/movement/physics-friction.cpp` | 75 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-gravity.cpp` | 70 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-ground-return.cpp` | 74 | DebugLog | CAP_LOG |
| `src/physics/movement/physics-jump.cpp` | 109 | DebugLog,SizeScalingConfig | CAP_LOG,CAP_CONFIG |
| `src/physics/movement/physics-scratch.cpp` | 7 |  | CAP_RESOURCES |
| `src/physics/movement/physics-walk.cpp` | 76 | DebugLog,SizeScalingConfig | CAP_LOG,CAP_CONFIG |
| `src/physics/physical-body.cpp` | 485 |  | CAP_ENTITY |
| `src/physics/ray-utils.cpp` | 51 |  | CAP_WORLD |
| `src/physics/runtime-objects-commands.cpp` | 41 | Terminal,VehicleSystem | CAP_TERMINAL,CAP_PHYSICS |
| `src/physics/runtime-objects.cpp` | 47 | ConstraintSystem,PhysicalEntitySystem,StructuredLogger,VehicleSystem | CAP_PHYSICS,CAP_ENTITY,CAP_LOG |
| `src/pobjects/persistent-physics.cpp` | 530 | DebugLog,DebugVis,StructuredLogger,WeaponRegistry | CAP_LOG,CAP_DEBUG_VIS,CAP_REGISTRY |
| `src/procedural/procedural-world-client.cpp` | 265 | DebugVis,PhysicalEntitySystem | CAP_DEBUG_VIS,CAP_ENTITY |
| `src/ragdoll/ragdoll-commands.cpp` | 145 | Terminal,TerminalState | CAP_TERMINAL |
| `src/ragdoll/ragdoll-mode.cpp` | 3227 | DebugLog,DebugVis,StructuredLogger | CAP_LOG,CAP_DEBUG_VIS |
| `src/render/dynamic-light-commands.cpp` | 57 | Terminal | CAP_TERMINAL |
| `src/render/dynamic-light.cpp` | 240 | DebugLog | CAP_LOG |
| `src/render/lighting-commands.cpp` | 42 | Terminal | CAP_TERMINAL |
| `src/render/postfx-commands.cpp` | 91 | Terminal | CAP_TERMINAL |
| `src/render/world-box-mesh.cpp` | 86 | ModuleHost | CAP_MODULES |
| `src/replay/replay-camera-commands.cpp` | 153 | Terminal,TerminalState | CAP_TERMINAL |
| `src/replay/replay-commands-playback.cpp` | 273 | Terminal,TerminalState | CAP_TERMINAL |
| `src/replay/replay-player-interp.cpp` | 280 | DebugLog | CAP_LOG |
| `src/replay/replay-recorder-clips.cpp` | 114 |  | CAP_LOG |
| `src/replay/replay-recorder-events.cpp` | 90 | DebugLog,Perf | CAP_LOG,CAP_PERF |
| `src/shadow/shadow-commands.cpp` | 178 | DebugLog,Terminal | CAP_LOG,CAP_TERMINAL |
| `src/sim/simulate-tick.cpp` | 739 | ActorCollisionConfig,MapConfigRegistry,Perf,StructuredLogger,TerminalState,VehicleSystem,WeaponRegistry | CAP_CONFIG,CAP_REGISTRY,CAP_PERF,CAP_LOG,CAP_TERMINAL,CAP_PHYSICS |
| `src/terminal/admin-commands.cpp` | 92 | MultiplayerContextGlobal,Terminal,TerminalState | CAP_TRANSPORT,CAP_TERMINAL |
| `src/terminal/auth-commands.cpp` | 218 | AuthSystem,Terminal | CAP_AUTH,CAP_TERMINAL |
| `src/terminal/badconn-commands.cpp` | 188 | Terminal | CAP_TERMINAL |
| `src/terminal/crate-commands.cpp` | 357 | PhysicalEntitySystem,Terminal,TerminalState | CAP_ENTITY,CAP_TERMINAL |
| `src/terminal/duel-commands.cpp` | 72 | Terminal,TerminalState | CAP_TERMINAL |
| `src/terminal/editor-commands.cpp` | 69 | Terminal,TerminalState | CAP_TERMINAL |
| `src/terminal/movement-commands.cpp` | 265 | MovementJsonConfig,Terminal,TerminalState | CAP_CONFIG,CAP_TERMINAL |
| `src/terminal/terminal-config-commands.cpp` | 325 | Terminal | CAP_TERMINAL |
| `src/terminal/terminal-debug-toggles.cpp` | 481 | Terminal | CAP_TERMINAL |
| `src/terminal/terminal-help-commands.cpp` | 245 | Terminal | CAP_TERMINAL |
| `src/terminal/vip-commands.cpp` | 146 | AuthSystem,Terminal,TerminalState | CAP_AUTH,CAP_TERMINAL |
| `src/vehicles/vehicle-audio.cpp` | 103 | StructuredLogger | CAP_LOG |
| `src/vehicles/vehicle-commands.cpp` | 631 | PhysicalEntitySystem,StructuredLogger,Terminal,TerminalState,VehicleConfigRegistry,VehicleSystem | CAP_ENTITY,CAP_LOG,CAP_TERMINAL,CAP_REGISTRY,CAP_PHYSICS |
| `src/vehicles/vehicle-hud.cpp` | 112 | PhysicalEntitySystem,StructuredLogger,VehicleConfigRegistry,VehicleSystem | CAP_ENTITY,CAP_LOG,CAP_REGISTRY,CAP_PHYSICS |
| `src/vehicles/vehicle-render.cpp` | 95 | PhysicalEntitySystem,VehicleConfigRegistry,VehicleSystem | CAP_ENTITY,CAP_REGISTRY,CAP_PHYSICS |
| `src/vehicles/vehicle-system.cpp` | 2422 | DebugLog,PhysicalEntitySystem,StructuredLogger,TerminalState,VehicleConfigRegistry,VehicleSystem | CAP_LOG,CAP_ENTITY,CAP_TERMINAL,CAP_REGISTRY,CAP_PHYSICS |
| `src/video/video-commands.cpp` | 164 | StructuredLogger,Terminal,TerminalState | CAP_LOG,CAP_TERMINAL |
| `src/worldgen/worldgen-commands.cpp` | 550 | MapConfigRegistry,StructuredLogger,Terminal | CAP_REGISTRY,CAP_LOG,CAP_TERMINAL |

## Exit criteria for Pass C

- Every adapter row maps to at least one capability or is explicitly `UNMAPPED`.
- Every capability has a bounded plain-data operation list.
- No adapter row requires passing STL, exceptions, RTTI objects, or old-DLL pointers.
- The host capability table is additive and versioned; new behavior uses existing ops where practical.
