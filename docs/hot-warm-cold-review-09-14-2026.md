SUPERSEDED BY C:\mimita-priv-v8\docs\hot-warm-cold-review-9-14-2026-v2.md
SUPERSEDED BY C:\mimita-priv-v8\docs\hot-warm-cold-review-9-14-2026-v2.md
SUPERSEDED BY C:\mimita-priv-v8\docs\hot-warm-cold-review-9-14-2026-v2.md

The repo is currently at **Level 2, partially approaching Level 3**.

MiMITA can keep the EXE and world alive while rebuilding and activating a replaceable DLL. The GenericRuntime can register systems, events, commands, schemas, capabilities, and domains by ID. However, most gameplay still runs through cold EXE call sites, dynamic schemas are only byte storage, migrations are registered but not executed, and multiplayer activation is only a protocol seed.

The strongest evidence is in [live-code-development.md](C:/mimita-priv-v8/docs/features/live-code-development/live-code-development.md), [generic-runtime.cpp](C:/mimita-priv-v8/src/hot-reload/generic-runtime.cpp), and [hot-kernel-next-steps.md](C:/mimita-priv-v8/docs/architecture/live-development/hot-kernel-next-steps.md).

## 1. Maturity levels

| Level | Status | Evidence |
|---|---|---|
| 0 Cold | Still present | Most gameplay, rendering, networking, assets, and engine code remains in the EXE |
| 1 Hot implementation | **PROVEN / PARTIAL** | Immutable DLL generations, background build, ABI/self-test, safe-tick activation, rollback |
| 2 Hot source tree | **PARTIAL** | Glob-based `hot-modules.json`, project tree, dependency graph, add/delete/rename model |
| 3 Hot system creation | **PARTIAL / DATA MODEL** | Generic system descriptors and runtime domains exist; only a small number are actually integrated |
| 4 Hot state/schema creation | **DATA MODEL ONLY** | Dynamic byte components and schema descriptors exist; inspector, replication, migration execution, and policy enforcement are incomplete |
| 5 Hot subsystem replacement | **NOT IMPLEMENTED** | Kernel still directly owns and calls most named subsystems |
| 6 Live multiplayer development | **DATA MODEL ONLY** | `logicalCodeHash`, `platformPackageHash`, `moduleSetHash`, READY, SWITCH, and `switchTick` fields exist, but agreement and synchronized activation do not |
| 7 In-world development | **NOT IMPLEMENTED** | `modecreate` is a hot editor behavior, but there is no in-world source editor, diagnostics, collaboration, permissions, or shared publish flow |

The current branch also has pre-existing uncommitted edits in movement, `game-api.h`, `game-cli.cpp`, the live-runtime documentation, and movement self-test files. I preserved them and did not modify anything.

## 2. Subsystem audit

| Subsystem | Current owner/path | State | Add source live? | New type/system live? | Main blocker |
|---|---|---|---|---|---|
| Movement | `src/physics/movement`, `src/hot-reload/modules/movement-system.cpp` | WARM, transitioning HOT | Yes for registered hot module | Yes for generic system | Player/server movement still has legacy paths and parity is not proven |
| Ragdoll | `src/ragdoll/*` | COLD | No | No | Solver, bodies, and presentation are EXE-owned |
| Weapons | `src/combat/*`, `src/network/server-attack.cpp` | COLD/WARM | Config only | No | Weapon behavior and selection use concrete classes and switches |
| Projectiles | `src/combat/projectile-simulation.cpp`, `src/network/server-projectiles.cpp` | WARM | Rocket behavior policy partly hot | No complete projectile type | Kernel owns spawn, simulation, collision, authority, and reconciliation |
| Damage | `src/network/server-damage-policy.*`, hot rocket policy | WARM | Damage policy partly yes | No full damage behavior | Candidate receives narrow policy inputs instead of owning generic resolution |
| Inventory/tools | `src/combat/weapon-system.*`, terminal commands | COLD | No | No | Fixed weapon/runtime structures and command ownership |
| Gamemodes | `src/gamemode/*`, `src/network/server-gamemode.*`, `src/game/gamemode-manager.*` | COLD | JSON tuning only | No | Mode selection, score, teams, respawn, and timers remain named EXE logic |
| NPC behavior | `src/npc/*`, hot actor module | WARM | NPC decisions partly yes | Behavior table incomplete | NPC execution still calls legacy movement/combat paths |
| Transport | `src/network/udp-transport.*`, ICE | COLD kernel | No | No | Correctly belongs mostly in the permanent kernel |
| Batching/prediction/interpolation | `src/network/multiplayer-*.cpp` | COLD | No | No | Policy is directly embedded in multiplayer code |
| Lag simulation | `src/network/badconn/*` | COLD | No | No | Concrete packet simulator and packet switches |
| ECS storage | `src/ecs/*` | WARM | Generic storage exists | Dynamic storage exists | Typed legacy structures remain authoritative |
| Physics/collision | `src/physics/*`, `src/map/map-loader-collision.cpp` | COLD kernel | No | No | Query mechanism is kernel-owned; policy is still mixed into callers |
| Constraints | `src/network/server-constraints.*`, physics constraints | COLD | No | No | Fixed codecs and concrete constraint handling |
| `modecreate`/editor | `src/hot-reload/modules/editor-behavior.cpp`, `src/editor/*` | WARM | Editor behavior is hot | Generic editor types partly | UI, inspector, and persistent editing remain EXE-owned |
| Terminal commands | `src/terminal/*`, `src/devtools/*` | WARM | Generic hot commands exist | Yes for commands | Most existing commands are statically registered |
| GUI/UI | `src/gui/*` | COLD/WARM | Some presentation policy hot | No general UI system | Rendering, widgets, layout, and input are EXE-owned |
| Renderer | `src/render/*`, `src/renderer/*` | COLD | No | No | OpenGL objects and draw traversal belong to EXE |
| Shaders | shader files and renderer caches | COLD/WARM | Some config reload | No generic resource reload | No generation-stamped shader provider |
| Textures | `src/map/texture_manager.*`, loaders | WARM | Asset loading exists | No generic replacement | GPU cache ownership and invalidation |
| GLB/models | `src/map/map-loader-gltf*`, `src/entities/player-loader.cpp` | COLD/WARM | Async loading, not code hot reload | No | Loaded GPU resources and world bindings persist through cold owners |
| Audio/SFX/music | `src/audio/audio.*` | WARM | JSON/config and notification sound paths | No | Audio objects and playback remain EXE-owned |
| Maps/world | `src/world/*`, `src/map/*` | COLD/WARM | Candidate world loading exists | No live in-place map editing | World geometry and collision are not resource-generation based |
| Config JSON | `src/config/*`, `config/*` | HOT for selected settings | Usually yes | Not applicable | Coverage is inconsistent and runtime paths differ |
| Replay | `src/replay/*` | COLD | Replay data survives reload | No | Replay clock, capture, export, and rendering are static |
| Progression/backend | `src/persistence/*`, server/backend paths | COLD | No | No | External persistence and authority boundaries |
| Resource registry | `src/project/resource-registry.*` | DATA MODEL | Registry primitives exist | Not connected broadly | Asset tree and loaders are not unified |
| Package registry | `src/project/package-registry.*` | DATA MODEL | Package metadata exists | Partial | Dependency acquisition and promotion are incomplete |
| Capability registry | `src/project/capabilities.*`, GenericRuntime | WARM | Generic IDs work | Yes by ID | Enforcement is not applied at every call site |
| Event registry | GenericRuntime event descriptors | WARM | New event IDs can register | Yes descriptor-level | Event queue and schema-driven payload ownership remain incomplete |
| Schema registry | `src/project/state-schema.*`, dynamic components | DATA MODEL | Schema descriptors register | Partial | No complete runtime schema lifecycle |
| State migration | `src/project/state-schema.*` | DATA MODEL | Migration functions register | No live migration | Activation does not execute migration transactions |
| Project watcher | `src/project/project-watcher.*` | WARM | Source tree watcher exists | Yes for source changes | Full asset/config integration is missing |
| Dependency graph | `src/project/dependency-graph.*` | WARM | `.d` files are consumed | Yes for source units | Package-level activation graph is incomplete |
| Build worker | `build_game_dll.py`, hot reload worker | HOT | Yes | Candidate packages | Only DLL scope; ordinary EXE changes remain cold |
| Cross-platform loader | project dependency/deployment model | DATA MODEL ONLY | No complete path | No | Download, platform build, verification, sandboxing, and promotion are absent |

## 3. Representative file map

### HOT today

- `src/hot-reload/modules/actor-behavior.cpp`
- `src/hot-reload/modules/presentation.cpp`
- `src/hot-reload/modules/editor-behavior.cpp`
- `src/hot-reload/modules/generic-demo-package.cpp`
- `src/hot-reload/modules/banana-events.cpp`
- `src/hot-reload/modules/banana-system-renamed.cpp`
- `src/hot-reload/modules/rocket-behavior.cpp`

These are hot only when their behavior is reached through the GenericRuntime or legacy hot module table.

### WARM

- `src/hot-reload/modules/movement-system.cpp`
- `src/ecs/dynamic-components.cpp`
- `src/project/project-tree.cpp`
- `src/project/dependency-graph.cpp`
- `src/project/state-schema.cpp`
- `src/network/server-damage-policy.cpp`
- selected JSON configuration loaders
- `src/live-code/live-editor.cpp`

They have hot infrastructure or partial routing, but important behavior remains outside the boundary.

### COLD

- `src/combat/weapon-system.cpp`
- `src/combat/weapon-fire*.cpp`
- `src/network/server-projectiles.cpp`
- `src/network/multiplayer-interpolation.cpp`
- `src/network/multiplayer-packets.cpp`
- `src/npc/npc.cpp`
- `src/ragdoll/ragdoll-solver.cpp`
- `src/render/*`
- `src/renderer/*`
- `src/audio/audio.cpp`
- `src/replay/*`
- `src/persistence/*`
- `src/world/*`
- most `src/gui/*`

Editing these files while the game runs does not change the running game.

The important distinction is that a file can be compiled into a candidate DLL while still being ineffective if no active call reaches it through GenericRuntime.

## 4. GenericRuntime audit

The GenericRuntime currently supports:

- system registration by `{id, domainId, priority, invoke}`;
- deterministic priority/ID ordering;
- generic event dispatch by event ID and schema hash;
- generic event emission;
- command registration and dispatch;
- component schema registration;
- capability providers and requirements;
- kernel capability IDs;
- multiple runtime domains;
- package manifest hashing;
- atomic descriptor staging before activation.

The actual implementation is in [generic-runtime.cpp](C:/mimita-priv-v8/src/hot-reload/generic-runtime.cpp:27).

Current proof status:

| Claim | Assessment |
|---|---|
| Unknown system after startup | **Source-supported, self-test documented; live execution not independently run in this audit** |
| Add/rename/delete hot source | **Data model and glob watcher present; end-to-end live proof still required** |
| Failed compile preserves last good | **Implemented in loader path; runtime proof is documented, not re-run here** |
| New event type | **Descriptor-level supported** |
| Arbitrary capability ID | **Supported by GenericRuntime; enforcement remains incomplete** |
| New schema descriptor | **Supported** |
| Schema migration | **Registration exists; activation execution is missing** |
| Atomic package replacement | **Partial**; generic registration commits atomically, but broader subsystem/state activation is incomplete |
| Dependency graph invalidation | **Primitive exists; package activation integration incomplete** |

Future concepts:

| Concept | Result |
|---|---|
| New gamemode | **YES AFTER EXISTING CAPABILITIES ARE EXPOSED** |
| New NPC behavior | **YES NOW for decision-level behavior; full execution needs component/entity capabilities** |
| New editor behavior | **YES NOW for registered editor behavior** |
| New weapon behavior | **YES AFTER component, effect, inventory, and authority capabilities are exposed** |
| New tool | **YES AFTER existing command/system/component capabilities are connected** |
| New projectile behavior | **YES AFTER generic projectile state, collision, spawn, and event capabilities** |
| New networking policy | **YES AFTER transport and packet-policy capabilities are exposed** |
| New vehicle behavior | **NO for a complete vehicle today; needs general multi-body, relationship, constraint, and replication primitives** |

## 5. Cold call-site debt

Highest-leverage cold call sites include:

| Call site | Why cold | Generic replacement |
|---|---|---|
| `src/server/npcs.cpp:1224`, `physicsMainUpdate(...)` | NPC movement directly invokes concrete physics policy | `movement` system consuming actor components and intents |
| `src/npc/npc.cpp` render/update paths | NPC remains a specialized owner | Actor systems plus presentation events |
| `src/network/server.cpp:715` | Generic movement runs, but the kernel still controls the fixed gameplay hook | Reserved `gameplay.60` domain with stable context |
| `src/network/server-projectiles.cpp` | Projectile spawn, collision, damage, and reconciliation are coupled | Projectile systems plus generic collision/damage events |
| `src/network/server-attack.cpp` | Hitscan policy is a named server call path | Weapon/action system using capability-based ray queries |
| `src/network/server-melee.cpp` | Melee rules remain static | Generic contact/action event |
| `src/network/multiplayer-interpolation.cpp` | Interpolation policy is directly implemented in the client | Hot `network.render` policy system |
| `src/game/duel-state.cpp` and `src/network/server-gamemode.cpp` | Mode phase and rules use switches and concrete ownership | Generic match systems consuming events/resources |
| `src/render/render-player.cpp` and shadow renderers | Rendering traverses concrete `Player`/NPC types | Resource/entity presentation systems |
| `src/audio/audio.cpp` | Audio event routing is static | Generic sound/resource event consumer |

The first migration should remove a kernel call site, not merely add another hot module beside it.

## 6. Enum and switch debt

Acceptable permanent switches:

- packet framing and transport protocol;
- GLTF accessor/component decoding;
- low-level error/result decoding;
- platform and hardware mechanisms;
- bounded serialization formats.

Migration debt:

- `src/ecs/components.h` control-source mapping;
- editor operation handling;
- gamemode phase and result handling;
- weapon policy branches;
- network packet policy selection;
- NPC state selection;
- effect type rendering;
- persistence weapon mappings.

Bad future-concept enumerations:

- adding a new `GamemodeType`;
- adding a new weapon enum branch for every weapon;
- adding a new NPC archetype switch;
- adding a new editor operation enum for every tool;
- adding a new packet policy branch for every networking policy;
- adding a new component enum for every user-created schema.

These should become package IDs, event IDs, schema IDs, component data, relationships, and registered systems.

## 7. ABI debt

The main remaining ABI hazards are in [game-api.h](C:/mimita-priv-v8/src/hot-reload/game-api.h):

- `GameAPI` uses `structSize == sizeof(GameAPI)` in `hot-reload-system.cpp:498`;
- fixed `GameSharedStateV1`;
- fixed context structs and callback fields;
- fixed component payload structs;
- versioned but compile-time layouts;
- typed callbacks such as movement, editor, and gameplay contexts;
- adding fields can require API version or layout changes.

The package descriptor is more extensible because it uses pointer/count arrays for systems, events, schemas, migrations, and capabilities. However, it still depends on the fixed `GamePackageDescriptorV1` ABI.

Recommended direction:

- retain a small stable ABI envelope;
- move ordinary gameplay data into schema-hashed byte payloads;
- use descriptor tables and capability IDs;
- add explicit size/version negotiation;
- avoid subsystem-specific callback fields;
- move behavior dispatch to `{behaviorId, schemaHash, state envelope}`;
- keep only lifetime, memory, tick, entity, query, and capability mechanisms in the kernel.

`game-api.h` should eventually stop growing for ordinary gameplay features.

## 8. Dynamic components

Current readiness is **data model only**.

`DynamicComponentStore` supports:

- runtime schema registration;
- byte-blob read/write;
- attach/remove/has;
- entity cleanup.

It does not yet provide the full requested lifecycle:

- no runtime inspector/edit surface;
- no schema migration during activation;
- no schema version enforcement in storage;
- no network serialization implementation based on `networkPolicy`;
- no copy/paste policy integration;
- no deterministic iteration guarantee;
- no complete hot behavior binding to arbitrary dynamic components;
- no authoritative entity enumeration/spawn/destroy capability for hot systems.

Smallest path:

1. Add schema version and schema hash checks to storage.
2. Add migration execution during candidate staging.
3. Add generic `findEntities`, `attach`, `remove`, `read`, and `write` capabilities.
4. Add inspector enumeration and edit commands.
5. Add copy/paste policy handling.
6. Add deterministic serialization and replication.
7. Add per-entity behavior bindings.
8. Prove creation, edit, hot consumption, migration, and safe failure in one runtime test.

## 9. Gamemode readiness

A new gamemode cannot currently be created and switched entirely hot.

Cold dependencies include:

- gamemode selection;
- server match lifecycle;
- phase transitions;
- team and role ownership;
- respawn;
- scoring;
- timers;
- network announcements;
- UI result display;
- mode-specific switches.

A generic gamemode should instead be composed from:

- a mode package ID;
- match and actor components;
- generic events such as `ActorKilled`, `ObjectiveCaptured`, and `RespawnRequested`;
- systems for scoring, timers, teams, roles, and respawn;
- configuration resources;
- UI presentation events;
- capabilities for actor queries, entity state, and authoritative result application.

The first hot gamemode should not attempt to replace the entire match manager. It should prove a package-defined scoring and win-condition system operating on existing match state.

## 10. Vehicles as a falsification test

Vehicles expose the current architectural gaps.

A vehicle could eventually be composed from entities, components, constraints, relationships, input intents, physics capabilities, and hot systems. That would require:

- multi-body rigid-body ownership;
- wheel and suspension constraints;
- torque and drivetrain state;
- seats and passenger ownership;
- parent-space transforms;
- destruction relationships;
- authoritative replication;
- prediction/reconciliation policy.

If implementing a vehicle requires adding `VehicleManager`, `VehicleType`, vehicle packet branches, and vehicle-specific kernel slots, the generic architecture has failed that test.

Today, the architecture has the entity/component direction but lacks the general relationship, multi-body constraint, authoritative mutation, and replication primitives.

## 11. Networking

Permanent transport kernel:

- sockets;
- ICE;
- reliable channel;
- framing;
- connection lifetime;
- authority boundary;
- packet I/O.

Potentially hot policy:

- snapshot selection;
- batching;
- send rate;
- interpolation;
- prediction;
- reconciliation;
- lag simulation;
- packet prioritization;
- network presentation policy.

Current state is mostly cold. `CodeGenerationPacket` fields and manifest hashing exist, but the live network policy is still implemented directly in `src/network`.

The smallest useful networking migration is a hot snapshot/interpolation policy system operating on an existing immutable snapshot stream. It should not replace transport or packet framing.

## 12. Multiplayer code distribution

Current sequence is incomplete:

```text
source change
→ logical hash
→ platform package hash
→ module set hash
→ candidate registration
→ READY
→ switchTick
```

Existing:

- logical and platform hash fields;
- module set hash;
- generation reporting;
- READY/SWITCH phase values;
- server announcement fields;
- per-player tracking;
- manifest hash.

Missing:

- package acquisition/download;
- platform-specific build distribution;
- signature and trust validation;
- dependency verification and promotion;
- schema compatibility negotiation;
- actual delayed activation at `switchTick`;
- all-peer readiness barrier;
- state migration agreement;
- rollback if one peer fails after agreement;
- protocol compatibility policy;
- sandboxing for untrusted code.

These are separate concerns and should remain separate:

1. code distribution;
2. activation agreement;
3. state migration agreement;
4. protocol compatibility.

## 13. Asset hot development

Current asset behavior is mixed:

- JSON configuration: often hot;
- GLB/model loading: asynchronous, but not generally live replacement;
- PNG/textures: cache-backed, no universal generation replacement;
- shaders: renderer-owned and cold;
- SFX/music: playback and resource ownership remain cold;
- fonts/UI textures: static initialization and caches;
- maps/worlds: candidate loading exists, but no in-place live world replacement.

The correct general mechanism is a generation-stamped `ResourceProvider`:

```text
path
→ content hash
→ resource descriptor
→ async load
→ validate
→ swap resource handle
→ retire previous generation
```

The same mechanism should serve models, textures, shaders, fonts, audio, and world resources. Avoid one reload hack per asset type.

## 14. In-world/shared editor

`modecreate` already demonstrates hot editor behavior and generic command registration. It does not yet constitute shared development.

The future editor should call the existing project surface:

```text
in-world editor
→ ProjectControl
→ ProjectTree / ChangeSet
→ existing watcher
→ existing dependency graph
→ existing build worker
→ candidate generation
→ validation
→ activation / rollback
```

Required pieces:

- file and package editing;
- syntax diagnostics;
- diff and review;
- generation history;
- rollback;
- collaborative change proposals;
- permissions and server authority;
- signed package promotion;
- sandboxing for untrusted code.

It must remain a client of the existing project/build pipeline.

## 15. Cold-build elimination roadmap

| Reason | Eliminate? | Priority |
|---|---|---|
| Gameplay policy in EXE | Yes | Highest |
| Movement path duplication | Yes | Highest |
| Weapon/projectile behavior in concrete classes | Yes | High |
| Gamemode branches | Yes | High |
| Dynamic component creation | Yes | High |
| Network policy in client/server files | Yes | High |
| Asset cache replacement | Yes | Medium |
| Rendering mechanism | Partly | Medium |
| Stable ABI/kernel changes | No, except rarely | Permanent cold |
| OS/window/OpenGL/transport provider | No | Permanent cold |
| Platform loader and sandbox | No for the kernel; package tools may evolve separately | Low |
| External backend/persistence mechanism | Usually no | Low |

Realistically, “no cold builds” means zero cold builds for ordinary gameplay, systems, tools, modes, behaviors, schemas, and assets. Kernel, ABI, OS, hardware, and platform-provider evolution may remain cold.

## 16. Highest-leverage next migrations

| Rank | Migration | Leverage |
|---:|---|---|
| 1 | Make movement the only/default movement owner | Removes a high-frequency gameplay cold seam and proves full system replacement |
| 2 | Add generic component/entity capabilities | Unlocks real hot gameplay ownership |
| 3 | Execute schema migrations during activation | Makes stateful replacement safe |
| 4 | Move projectile simulation policy hot | Exercises fixed tick, collision, damage, prediction, and reconciliation |
| 5 | Add per-entity behavior bindings | Allows different live behaviors without global switches |
| 6 | Convert gamemode scoring/win rules to systems/events | Removes broad mode-specific cold logic |
| 7 | Add deterministic dynamic-component serialization | Unlocks replication, replay, and world hashing |
| 8 | Move snapshot/interpolation policy hot | Proves hot networking policy while retaining transport |
| 9 | Add generic resource-provider reload | Makes asset iteration live |
| 10 | Complete READY/SWITCH activation | Enables connected-session code evolution |

## 17. Recommended next slice

Choose **A: make `movement.main` the only/default movement path and remove the legacy movement escape hatch after parity is proven**.

This has the highest leverage because movement is:

- executed every simulation tick;
- shared by players and NPCs;
- coupled to physics and collision;
- easy to falsify with deterministic tests;
- already partially routed through GenericRuntime;
- a direct test of whether a real gameplay subsystem can move behind the generic boundary.

The current implementation is explicitly transitional: `movement-system.cpp` says hot movement is opt-in and retains `GAME_MODE_FLAG_LEGACY_MOVEMENT`. That means the architecture is not yet proven by merely having the hot file.

## 18. Exact implementation scope

The coherent slice should include:

- identify every player, NPC, prediction, and server movement entry point;
- make one shared movement system consume actor components and intents;
- route the authoritative player and NPC movement ticks through that system;
- keep collision queries and fixed-tick scheduling in the kernel;
- remove duplicate movement policy from callers;
- preserve the existing generic `physics.moveCapsule` capability;
- move grounded, dash, jump, friction, air acceleration, and gravity state into stable component/state envelopes;
- remove the legacy movement flag only after parity;
- add deterministic source-to-output evidence;
- leave rendering, vehicles, full ECS conversion, and multiplayer switching out of scope.

## 19. Falsification test

The proof should be:

```text
start MiMITA once
record PID, active generation, world identity, actor IDs

run deterministic movement baseline
save movement.main with a visibly different acceleration value
confirm candidate generation builds and activates
confirm the same player/NPC entities continue moving in the same world
confirm no EXE relink, restart, reconnect, or respawn occurred

introduce a syntax error
confirm the old movement generation remains active
confirm the failure is journaled

fix the source
confirm a new generation activates

edit jump or air-strafe behavior
confirm the live result changes

run the same movement input sequence
confirm deterministic expected outputs

test player and NPC
confirm both use the same movement owner

rollback
confirm the previous behavior returns without restarting
```

The required evidence must separately show:

- source and dependency detection;
- candidate build;
- activation generation;
- movement system invocation;
- component input;
- collision result;
- component output;
- unchanged PID/entity/world identity;
- compile failure preserving the previous generation;
- rollback.

## 20. Do not build yet

Do not build vehicles, a second in-game build system, a bytecode sandbox, per-asset reload hacks, a full renderer rewrite, or multiplayer code distribution before one gameplay subsystem is fully removed from its cold call path.

The most valuable architectural proof is:

```text
edit ordinary movement C++
→ candidate validates
→ movement changes in the same running world
→ syntax failure keeps old behavior
→ rollback works
→ player and NPC share the path
```

That would convert the current GenericRuntime demonstration into evidence that
ordinary MiMITA development is actually becoming live.

## Update 2026-09-14 — generic dynamic entity/component lifecycle pass

Highest-leverage rows 2 and 3 above (generic component/entity capabilities;
schema migrations during activation) have now been implemented:

- `GameplayContextV1` v6 exposes `entity.create`, `entity.destroy`,
  `component.remove`, `component.enumerate`, `component.typesOnEntity`,
  `component.schema`, and `relationship.add/remove/query`.
- `DynamicComponentStore` owns schema versions, 64-bit migrations, an atomic
  activation-time migration, deterministic enumeration, and deterministic
  serialization/hash; a failed migration rejects the candidate and leaves the
  previous generation and bytes intact.
- New package entities are kernel-allocated in `EntityDomain::None`, so ordinary
  concepts need no new cold enum.
- `--dynamic-lifecycle-selftest` and the live hot probe
  `src/hot-reload/modules/banana-component.cpp` prove the path.

Section 8 (Dynamic components) readiness moves from **data model only** to
**WARM**: the inspector/edit UI, copy/paste integration, and replication remain
open. The maintained map now lives in
`docs/architecture/live-development/hot-cold-audit.md`.