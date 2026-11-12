# World-query inventory (2026-11-12)

**Status:** static scan of ray, closest-point, sphere/capsule, AABB, sweep,
navmesh, and broadphase candidate queries. It is the hot world-query inventory,
not a correctness or performance proof.

- Sites: 2656
- Files with query work: 176
- Machine table: `docs/handoffs/cpp-world-query-inventory-20261112.csv`

## Hot world-query target

The resident host owns the world geometry, broadphase, and stable entity
handles. `game.dll` composes new queries from generic primitives: ray, sphere,
capsule, box, closest-point, and broadphase candidates. A genuinely new query
primitive is a one-time capability add; a new composition is hot.

## Sites by category

| Category | Sites |
|---|---:|
| broadphase | 1299 |
| sphere_capsule | 542 |
| sweep | 424 |
| aabb_box | 248 |
| ray_triangle | 42 |
| ray_aabb | 39 |
| navmesh | 33 |
| closest_point | 29 |

## Files by query kind

| File | ray-tri | ray-aabb | closest | sphere/capsule | aabb | sweep | navmesh | broadphase |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `src/network/server-gamemode.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 116 |
| `src/network/server-npcs.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 4 | 94 |
| `src/physics/movement/physics-collision-stress.cpp` | 0 | 0 | 0 | 19 | 14 | 49 | 0 | 6 |
| `src/procedural/infinite-world-runtime.cpp` | 0 | 0 | 0 | 0 | 3 | 0 | 0 | 81 |
| `src/physics/movement/physics-collision-mesh.cpp` | 0 | 0 | 6 | 5 | 21 | 29 | 0 | 20 |
| `src/physics/movement/actor-triangle-solver.cpp` | 0 | 0 | 0 | 5 | 18 | 17 | 0 | 29 |
| `src/physics/movement/physics-collision-glb-setup.cpp` | 0 | 0 | 0 | 3 | 21 | 29 | 0 | 12 |
| `src/physics/movement/physics-collision.cpp` | 3 | 9 | 0 | 4 | 19 | 20 | 0 | 2 |
| `src/npc/npc-navigator.cpp` | 3 | 0 | 0 | 7 | 1 | 0 | 15 | 29 |
| `src/physics/movement/physics-collision-body.cpp` | 0 | 0 | 0 | 21 | 6 | 18 | 0 | 8 |
| `src/combat/weapon-fire-raycast.cpp` | 1 | 10 | 0 | 4 | 1 | 35 | 0 | 0 |
| `src/hot-reload/module-host.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 51 |
| `src/npc/npc-navigation.cpp` | 2 | 0 | 0 | 3 | 2 | 0 | 0 | 43 |
| `src/combat/weapon-collision-config.cpp` | 0 | 0 | 0 | 45 | 0 | 4 | 0 | 0 |
| `src/npc/recast-navigation.cpp` | 0 | 0 | 0 | 0 | 2 | 0 | 13 | 33 |
| `src/physics/movement/physics-collision-glb-main.cpp` | 0 | 0 | 0 | 11 | 0 | 18 | 0 | 19 |
| `src/physics/physical-entity.cpp` | 0 | 0 | 0 | 7 | 16 | 5 | 0 | 15 |
| `src/combat/weapon-system.cpp` | 2 | 2 | 0 | 26 | 2 | 1 | 0 | 9 |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` | 0 | 0 | 0 | 3 | 5 | 20 | 0 | 14 |
| `src/npc/npc-behavior-module.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 36 |
| `src/npc/npc.cpp` | 0 | 0 | 0 | 1 | 2 | 0 | 0 | 32 |
| `src/combat/projectile-simulation.cpp` | 0 | 0 | 6 | 9 | 0 | 16 | 0 | 2 |
| `src/network/server-packets.cpp` | 0 | 8 | 0 | 4 | 8 | 5 | 0 | 8 |
| `src/network/multiplayer-projectiles.cpp` | 1 | 0 | 0 | 27 | 1 | 0 | 0 | 3 |
| `src/config/player-visuals-config.cpp` | 0 | 0 | 0 | 26 | 0 | 0 | 0 | 3 |
| `src/network/server-players.cpp` | 1 | 0 | 0 | 10 | 3 | 0 | 0 | 13 |
| `src/physics/movement/actor-final-pose.cpp` | 2 | 0 | 0 | 0 | 3 | 0 | 0 | 21 |
| `src/world/world-gltf-loader.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 26 |
| `src/mapeditor/map-editor.cpp` | 0 | 4 | 0 | 0 | 7 | 0 | 0 | 13 |
| `src/network/server-world-destruction.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 24 |
| `src/physics/movement/physics-collision-dispatch.cpp` | 0 | 0 | 0 | 7 | 6 | 8 | 0 | 3 |
| `src/combat/weapon-swordsword.cpp` | 0 | 0 | 0 | 15 | 2 | 2 | 0 | 4 |
| `src/map/map-loader-collision.cpp` | 0 | 0 | 0 | 1 | 15 | 0 | 0 | 7 |
| `src/npc/npc-perception.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 23 |
| `src/gamemode/map-config.cpp` | 0 | 0 | 0 | 12 | 0 | 0 | 0 | 10 |
| `src/physics/movement/physics-collision-glb-body.cpp` | 0 | 0 | 0 | 8 | 0 | 9 | 0 | 4 |
| `src/physics/movement/physics-collision-glb-contact.cpp` | 0 | 0 | 5 | 7 | 0 | 1 | 0 | 8 |
| `src/combat/weapon-execution.cpp` | 0 | 2 | 2 | 7 | 1 | 8 | 0 | 0 |
| `src/network/server-projectiles.cpp` | 0 | 0 | 0 | 2 | 12 | 5 | 0 | 1 |
| `src/render/render-player.cpp` | 0 | 0 | 0 | 20 | 0 | 0 | 0 | 0 |
| `src/video/outro.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 20 |
| `src/config/actor-collision-config.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 18 |
| `src/impact/destructible-selftest.cpp` | 3 | 0 | 0 | 9 | 0 | 7 | 0 | 0 |
| `src/network/server-infinite-collision.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 19 |
| `src/combat/weapon-quick-hit.cpp` | 0 | 0 | 0 | 15 | 0 | 3 | 0 | 0 |
| `src/map/map-static-union.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 18 |
| `src/physics/physical-body.cpp` | 0 | 0 | 0 | 3 | 2 | 4 | 0 | 8 |
| `src/entities/player-animation.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 16 |
| `src/game/spawn-utils.cpp` | 3 | 0 | 2 | 0 | 2 | 0 | 0 | 9 |
| `src/network/server-melee-actions.cpp` | 0 | 0 | 0 | 2 | 0 | 6 | 0 | 8 |
| `src/npc/npc-perception-module.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 16 |
| `src/npc/npc-state-machine.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 16 |
| `src/physics/movement/actor-pair-collision.cpp` | 0 | 0 | 0 | 3 | 0 | 8 | 0 | 4 |
| `src/ragdoll/ragdoll-mode.cpp` | 0 | 0 | 0 | 9 | 1 | 2 | 0 | 3 |
| `src/vehicles/vehicle-system.cpp` | 2 | 0 | 0 | 0 | 2 | 0 | 0 | 11 |
| `src/hot-reload/hot-reload-system.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 14 |
| `src/map/map-authored-geometry.cpp` | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 12 |
| `src/npc/npc-targeting.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 14 |
| `src/perf/perf.cpp` | 0 | 0 | 0 | 0 | 6 | 6 | 0 | 2 |
| `src/physics/movement/physics-collision-block.cpp` | 0 | 0 | 0 | 1 | 11 | 2 | 0 | 0 |
| `src/combat/explosion-fx.cpp` | 0 | 0 | 0 | 13 | 0 | 0 | 0 | 0 |
| `src/effects/effect-part-blood.cpp` | 0 | 0 | 0 | 10 | 3 | 0 | 0 | 0 |
| `src/npc/npc-spawn.cpp` | 0 | 0 | 0 | 13 | 0 | 0 | 0 | 0 |
| `src/procedural/procedural-world.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 13 |
| `src/combat/client-collision-world-view.cpp` | 0 | 0 | 0 | 0 | 7 | 5 | 0 | 0 |
| `src/game/probe/npc-target-probe-module.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 12 |
| `src/gamemode/match-roles.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 12 |
| `src/impact/impact-system.cpp` | 0 | 0 | 0 | 3 | 3 | 2 | 0 | 4 |
| `src/actor/player-ai.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 |
| `src/combat/melee-client.cpp` | 0 | 0 | 0 | 2 | 0 | 9 | 0 | 0 |
| `src/effects/hit-effects-spawn.cpp` | 0 | 0 | 0 | 11 | 0 | 0 | 0 | 0 |
| `src/engine/engine-tick-render.cpp` | 0 | 0 | 0 | 11 | 0 | 0 | 0 | 0 |
| `src/game/game-cli.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 |
| `src/network/movement-validation.cpp` | 2 | 0 | 0 | 0 | 1 | 2 | 0 | 6 |
| `src/perf/perf-spike.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 11 |
| `src/world/streamed-world-layer.cpp` | 0 | 0 | 0 | 0 | 7 | 0 | 0 | 4 |
| `src/combat/weapon-spyknife.cpp` | 0 | 0 | 0 | 3 | 0 | 7 | 0 | 0 |
| `src/host/host-runtime.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 10 |
| `src/network/server-attack.cpp` | 0 | 0 | 0 | 4 | 0 | 6 | 0 | 0 |
| `src/physics/movement/physics-collision-glb-sweep.cpp` | 0 | 0 | 2 | 0 | 0 | 8 | 0 | 0 |
| `src/worldgen/worldgen-library.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 10 |
| `src/network/multiplayer-packets.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 9 |
| `src/network/server-contact.cpp` | 0 | 0 | 5 | 0 | 1 | 0 | 0 | 3 |
| `src/combat/weapon-viewmodel.cpp` | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 |
| `src/network/server-ice.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 8 |
| `src/network/server-physical-contact.cpp` | 0 | 0 | 0 | 8 | 0 | 0 | 0 | 0 |
| `src/npc/npc-avatar.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 8 |
| `src/render/dynamic-light.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 8 |
| `src/avatar/character-manifest.cpp` | 0 | 0 | 0 | 7 | 0 | 0 | 0 | 0 |
| `src/combat/weapon-godball-movement.cpp` | 0 | 0 | 0 | 0 | 0 | 7 | 0 | 0 |
| `src/mapeditor/map-editor-commands.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 7 |
| `src/network/multiplayer-interpolation.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 5 |
| `src/network/vehicle-networking.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 5 |
| `src/physics/movement/physics-collision-glb-safety.cpp` | 0 | 0 | 1 | 2 | 0 | 2 | 0 | 2 |
| `src/audio/audio-acoustics.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 4 |
| `src/combat/revolver-system.cpp` | 3 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| `src/combat/weapon-fire-hit.cpp` | 0 | 0 | 0 | 1 | 0 | 5 | 0 | 0 |
| `src/combat/weapon-hafs.cpp` | 0 | 0 | 0 | 3 | 0 | 3 | 0 | 0 |
| `src/entities/player-loader.cpp` | 0 | 0 | 0 | 6 | 0 | 0 | 0 | 0 |
| `src/impact/boolean-mesh.cpp` | 0 | 0 | 0 | 6 | 0 | 0 | 0 | 0 |
| `src/killfeed/killfeed.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |
| `src/network/ice/ice-agent.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |
| `src/npc/npc-behavior.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |
| `src/render/render-world.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 6 |
| `src/sim/simulate-tick.cpp` | 0 | 0 | 0 | 2 | 0 | 1 | 0 | 3 |
| `src/combat/weapon-json-config.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| `src/debug/debug-visuals-scene.cpp` | 0 | 0 | 0 | 5 | 0 | 0 | 0 | 0 |
| `src/network/server-authored-rebuild.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| `src/npc/npc-combat.cpp` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 4 |
| `src/npc/npc-nav-graph.cpp` | 3 | 0 | 0 | 1 | 1 | 0 | 0 | 0 |
| `src/perf/perf-overlay.cpp` | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 3 |
| `src/physics/movement/actor-collision-mesh.cpp` | 0 | 0 | 0 | 2 | 0 | 3 | 0 | 0 |
| `src/physics/ray-utils.cpp` | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `src/pobjects/persistent-physics.cpp` | 0 | 0 | 0 | 1 | 1 | 0 | 0 | 3 |
| `src/replay/replay-export-mf.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5 |
| `src/world/world.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 4 |
| `src/combat/projectile-render.cpp` | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| `src/combat/weapon-godball-physics.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |
| `src/config/weapon-hitfx-config.cpp` | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| `src/effects/hit-effects.cpp` | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| `src/game/objective-state.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| `src/network/destruction-replication-selftest.cpp` | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| `src/network/multiplayer-physical-entities.cpp` | 0 | 0 | 0 | 4 | 0 | 0 | 0 | 0 |
| `src/network/server-packet-handlers.cpp` | 1 | 1 | 0 | 1 | 0 | 1 | 0 | 0 |
| `src/physics/movement/physics-collision-core.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |
| `src/physics/physics-mini.cpp` | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 0 |
| `src/replay/replay-recorder.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 4 |
| `src/audio/audio.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/combat/weapon-config.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/debug/debug-visuals-lines.cpp` | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
| `src/devtools/scenario-tracks.cpp` | 0 | 0 | 0 | 0 | 0 | 3 | 0 | 0 |
| `src/entities/player-render.cpp` | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
| `src/entities/player.cpp` | 0 | 0 | 0 | 1 | 0 | 2 | 0 | 0 |
| `src/gui/hud/chat-bubble.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/gui/hud/chat-window.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/network/ice/ice-test.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/npc/npc-utility.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/npc/team-brain.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/perf/perf-commands.cpp` | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 1 |
| `src/physics/movement/actor-triangle-spike.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/terminal/terminal-debug-toggles.cpp` | 0 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
| `src/worldgen/infinite-worldgen-module.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| `src/combat/shot-profiler.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 1 |
| `src/combat/weapon-grenade-launcher.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |
| `src/devtools/scenario-recorder.cpp` | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |
| `src/devtools/scenario-server-recorder.cpp` | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 |
| `src/gamemode/arena-ball.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |
| `src/gamemode/map-edit-applier.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| `src/gamemode/payload-runtime.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |
| `src/network/coordinator-client.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| `src/network/multiplayer-reconcile.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |
| `src/network/server-world.cpp` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 1 |
| `src/npc/npc-movement-policy.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| `src/npc/npc-navigation-settings.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| `src/renderer/renderer.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 |
| `src/terminal/weapon-commands.cpp` | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 |
| `src/avatar/avatar.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/combat/area-effect.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/combat/grenade-registry.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/combat/weapon-melee-config.cpp` | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| `src/combat/weapon-rocket-launcher.cpp` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `src/effects/hitfx-commands.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/engine/engine-tick-camera.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/engine/engine-tick-combat.cpp` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `src/gamemode/mode-pack-registry.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |
| `src/navigation/navigation-policy-module.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| `src/network/disagreement-visuals.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/network/multiplayer-tick.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/network/physical-entity-replication.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/network/server-body-template.cpp` | 0 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| `src/network/server-physical-contact-actors.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/physics/movement/movement-step.cpp` | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| `src/shadow/shadow-render.cpp` | 0 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| `src/terminal/map-entity-commands.cpp` | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 |
| `src/terminal/player-commands.cpp` | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `src/terminal/weapon-bench-commands.cpp` | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |

## Required before query code moves

- A bounded `CAP_WORLD` query descriptor (shape, mask, max results) with plain results.
- Read-only triangle/entity span access for compositions the host does not natively expose.
- A stable-handle mapping for actors and world objects.
- Per-file classification of query callers that mutate host state; those use a capability write, not a reference.
