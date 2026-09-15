# Executive summary

MiMITA is at a real but incomplete Level 2/early Level 3 architecture:

- Local hot DLL generations, safe-tick activation, rollback, generic systems, capabilities, dynamic schemas, relationships, hot projectile paths, and runtime gamemode registration exist.
- The running world can survive some behavior replacement.
- The target “edit arbitrary source/content and safely update the existing multiplayer world” does not yet work generally.

The largest remaining cold walls are:

1. Authoritative projectile compatibility and legacy producers.
2. Gamemode lifecycle, teams, objectives, and client presentation.
3. Typed player/NPC/entity state still being authoritative in many paths.
4. Client prediction, interpolation, reconciliation, and packet-policy code.
5. Renderer/presentation/resource ownership.
6. Entity creation/destruction replication.
7. Multiplayer package distribution, trust, synchronized switching, and rollback.
8. Procedural world streaming/persistence/navigation infrastructure.
9. Schema-driven editor authoring.
10. Static command/resource registration.

No implementation code was changed. Existing worktree edits were preserved.

## Current architecture map

```text
Cold process/kernel
 ├─ process, memory, sockets, transport, packet framing
 ├─ EntityId and legacy Player/NPC storage
 ├─ fixed tick and physics/collision mechanisms
 ├─ renderer/OpenGL/GPU resources
 └─ legacy gameplay call sites

Hot loader
 ├─ source watcher and hash detection
 ├─ background DLL build
 ├─ generation staging and ABI/self-test
 ├─ safe-tick activation
 └─ rollback / last-good generation

GenericRuntime
 ├─ systems
 ├─ events
 ├─ commands
 ├─ capabilities
 ├─ component schemas
 ├─ migrations
 └─ mode domains

Hot gameplay
 ├─ actor decisions
 ├─ selected movement behavior
 ├─ tool-use and projectile-impact policy
 ├─ canonical hot projectile system
 ├─ dynamic components and relationships
 └─ partial gamemode policy

Networking
 ├─ generic dynamic component/relationship envelope
 ├─ legacy typed player/NPC packets
 ├─ legacy prediction/interpolation/reconciliation
 └─ unfinished READY/switch-tick protocol
```

## Hot / warm / cold audit

| Subsystem | Status | Current owner | Main cold dependency |
|---|---|---|---|
| Hot loader/generations | HOT | `hot-reload-system.cpp` | Generic loader is operational locally |
| Source watcher | WARM | `project-watcher.cpp`, `hot-reload-system.cpp` | Build graph is still coarse and DLL-wide |
| Dependency graph | WARM | `project/dependency-graph.*` | Package invalidation is incomplete |
| GenericRuntime | HOT/WARM | `generic-runtime.cpp` | Descriptor ABI and activation integration remain fixed |
| Capabilities | WARM | `live-behavior.cpp`, `generic-runtime.cpp` | Enforcement is not universal |
| Dynamic components | HOT for lifecycle/migration | `ecs/dynamic-components.*` | Typed legacy components remain authoritative |
| Relationships | HOT for dynamic edges | `ecs/relationship-store.*` | Entity lifecycle/network visibility is incomplete |
| Entity lifecycle | WARM | `entity-registry.*`, dynamic lifecycle | Generic entity create/destroy replication missing |
| Behavior bindings | HOT for selected events | `live-behavior.cpp` | Many concrete paths bypass bindings |
| Systems/scheduling | WARM | `GenericRuntime::runDomain` | Most gameplay still called from cold orchestration |
| Movement | WARM/HOT transition | `movement-system.cpp`, server movement | Full parity and all callers not migrated |
| Physics/collision | COLD mechanism, WARM policy | `physics/*`, map collision | Policy is mixed into cold callers |
| Damage | WARM | `server-damage-policy.*`, hot capability | Full generic resolution not universal |
| Weapons/tools | WARM | `src/combat/*`, `server-attack.cpp` | Ammo, hitscan, melee, selection, and state remain concrete |
| Projectiles | WARM | hot projectile system plus `server-projectiles.cpp` | Legacy container and compatibility paths remain |
| Inventory/equipment | WARM | `server-weapon-state.cpp`, relationships | Most weapon state is still string/typed legacy state |
| NPC/AI | WARM | `src/npc/*`, server NPC code | Execution and combat still use legacy paths |
| Navigation | COLD | world/map/navigation code | No generic streamed navigation generation |
| Gamemodes | WARM | `server-gamemode.cpp`, hot mode packages | Phase mechanism, teams, objectives, client UI remain cold |
| Networking transport | COLD, legitimate kernel | UDP/ICE/reliable transport | Correctly belongs in kernel |
| Replication | WARM | dynamic replication plus typed packets | Players/NPCs/entities still use fixed packet schemas |
| Prediction/interpolation | COLD | `multiplayer-*` | Policy is directly compiled into client |
| Reconciliation/lag compensation | COLD/WARM | network/server combat code | No generic hot policy boundary |
| Rendering | COLD | `src/render/*`, `src/renderer/*` | Concrete Player/NPC traversal and GPU ownership |
| Shaders/materials/assets | WARM | render/resource loaders | Reload exists selectively, not generation-wide |
| Audio/effects | WARM | audio/effect systems | Routing and resource ownership remain static |
| UI/HUD | WARM/COLD | `src/gui/*`, render paths | Only selected policy/config is hot |
| Terminal commands | WARM | `Terminal`, `GenericRuntime` | Existing commands are statically registered |
| Maps/world loading | COLD/WARM | `src/map/*`, `src/world/*` | No generic in-place region lifecycle |
| Procedural generation | COLD | world/map code | Missing region identity, streaming, persistence, migration |
| Persistence | COLD | `src/persistence/*` | External/backend authority and schemas are static |
| Replay | COLD | `src/replay/*` | Capture/export/render policy is fixed |
| Editor | WARM | editor plus hot `modecreate` | Inspector is not fully schema-generated |
| Prefab/composition | COLD | no complete generic owner | No reusable composition persistence system |
| Schema migration | HOT for dynamic components | dynamic store | Typed state migration remains separate |
| Failure rollback | HOT locally | hot loader | No multiplayer rollback protocol |
| Package distribution | COLD | no complete runtime owner | No signed/distributed peer package flow |
| Security/trust | COLD | deployment/runtime boundary | Arbitrary native peer code is unsafe |

## Scenario results

| Scenario | Result | First cold wall |
|---|---|---|
| A. Networked rocket edit | PARTIALLY WORKS | Legacy client prediction/render and multiplayer generation agreement |
| B. Grenade edit | PARTIALLY WORKS | Legacy compatibility paths and incomplete two-client proof |
| C. Gamemode edit | PARTIALLY WORKS | Phase mechanism, teams/objectives, client UI, typed match state |
| D. Brand-new gamemode | DOES NOT WORK fully | Selection/network/schema distribution and client activation |
| E. Wireframe/presentation fix | PARTIALLY WORKS | Renderer/render-pass/GPU ownership in the EXE |
| F. New terminal command | PARTIALLY WORKS | Generic commands work, but most discovery remains startup registration |
| G. New monster | DOES NOT WORK fully | Entity replication, AI/execution, navigation, animation, and resources |
| H. Infinite dungeon | DOES NOT WORK | No generic region streaming/persistence/reconstruction architecture |
| I. General entity editor | PARTIALLY WORKS | Inspector/edit/copy/paste/composition system is not fully schema-driven |

## Important findings

### 1. Local hot reload is real

`src/hot-reload/hot-reload-system.cpp` provides:

- hashed source detection;
- background candidate builds;
- unique generation DLLs;
- ABI and self-test validation;
- safe-tick activation;
- rollback;
- cold-boundary reporting.

This proves local code replacement, not whole-engine hot replacement.

### 2. Generic runtime registration is strong but not sufficient

`src/hot-reload/generic-runtime.cpp` supports runtime registration of systems, events, commands, schemas, capabilities, migrations, and mode descriptors.

The remaining issue is not primarily registration. It is that cold orchestration still decides when and how most gameplay happens.

### 3. Dynamic replication is not yet general entity replication

`src/network/dynamic-replication.cpp` can distribute unknown component schemas and relationships through one generic envelope.

However:

- arbitrary entity create/destroy is not replicated;
- typed players/NPCs still use fixed snapshots;
- real two-client runtime proof is missing;
- client presentation does not automatically consume arbitrary dynamic entities.

### 4. Projectile migration is the highest-leverage current pass

The canonical hot projectile path now exists for rocket, grenade, and test projectile behavior. But `server-projectiles.cpp` and compatibility producers still remain.

Until the legacy authoritative path is deleted or reduced to a proven compatibility adapter, Scenario A is not fully true.

### 5. Gamemode migration is incomplete

Hot FFA/TDM package policy and lifecycle policy exist, but `server-gamemode.cpp` still owns significant phase, objective, team, and transition mechanisms. A new mode cannot yet independently provide all required server/client behavior without touching cold code.

### 6. Rendering is largely still cold

A presentation module can change selected formatting and effect parameters, but editing `render-player.cpp`, render traversal, wireframe paths, shaders, materials, or GPU resource ownership does not generally update the live client without cold code involvement.

### 7. Terminal commands are only partially generic

`GenericRuntime` can register and invoke commands, but the ordinary command architecture is still dominated by startup functions such as `registerWeaponCommands()` and many static `Terminal::registerCommand()` calls.

A truly new command can be hot only if it enters through the generic runtime path.

### 8. Procedural-world infrastructure is missing

The repository does not yet provide a complete generic system for:

- `WorldSeed`;
- deterministic named RNG streams;
- stable `RegionId`/`ChunkId`;
- streamed load/unload;
- generated-base plus persistent delta;
- generator versioning;
- explicit migration/regeneration policy;
- navigation/collision/render generation per region;
- network relevance;
- editor overrides.

This is a separate architectural gap, not merely a missing dungeon feature.

## Top architectural blockers

1. Remove the remaining authoritative projectile/container architecture.
2. Move typed player/NPC/entity state toward generic replicated components.
3. Create one generic authoritative gameplay/system tick boundary.
4. Hot-migrate prediction, interpolation, reconciliation, and snapshot policy.
5. Finish generic entity create/destroy replication.
6. Complete generic match/team/objective/respawn capabilities.
7. Replace static resource/render ownership with generation-aware resource providers.
8. Finish schema-driven editor inspection and mutation.
9. Build region/stream/persistence primitives for procedural worlds.
10. Implement multiplayer package identity, distribution, readiness, synchronized switching, migration, rollback, trust, and incompatible-peer handling.

## Legacy bridges to delete eventually

Do not delete them until callers are proven migrated:

- `server-projectiles.cpp` authoritative container path;
- per-weapon network policy and `NETWORK_WEAPON_*` branches;
- typed `ServerPlayer`/`ServerNpc` gameplay state where dynamic state is authoritative;
- concrete gamemode phase/objective branches;
- static weapon/inventory scratch state;
- static command registration paths;
- concrete Player/NPC render traversal;
- fixed player/NPC replication schemas;
- client-only prediction/interpolation policy embedded in `multiplayer-*`;
- typed editor operation/component handling.

Some switches are legitimate kernel mechanisms: packet framing, transport, platform APIs, GLTF decoding, and bounded serialization formats.

## Missing generic primitives

Only the following primitives appear broadly justified:

1. Generic entity create/destroy replication.
2. Stable schema/entity/relationship version envelopes.
3. Atomic package dependency activation.
4. Generic authoritative gameplay context/query/mutation boundary.
5. Generation-aware resource provider and GPU-handle swap.
6. Generic snapshot/prediction/interpolation policy interface.
7. Region identity/stream/persistence primitive.
8. Schema-driven editor reflection and composition persistence.
9. Multiplayer READY/switch-tick/migration/rollback protocol.
10. Capability trust, signing, sandboxing, and peer compatibility policy.

Avoid adding `WeaponManager`, `MonsterManager`, `GamemodeManager`, or feature-specific kernel callbacks.

## Ideal final flow

```text
Input
 → generic action/tool intent
 → authoritative capability validation
 → hot systems consume entity/components/relationships
 → world mutation through generic capabilities
 → generic event/state records
 → replicated schema/entity/relationship envelopes
 → client hot prediction/reconciliation/presentation policy
 → generation-aware renderer/resources

Hot reload
 → source hash
 → dependency closure
 → candidate package build
 → ABI/schema/capability validation
 → state migration transaction
 → peer READY
 → shared switch tick
 → activate atomically
 → rollback on failure
```

## Migration plan

1. Finish canonical projectile migration and remove normal legacy producers.
2. Add a real two-client rocket/grenade trace with generation/hash/tick evidence.
3. Add generic entity create/destroy replication.
4. Migrate one complete typed gameplay state, such as projectile ownership, into schema state.
5. Introduce a generic `gameplay.60` authoritative system boundary.
6. Move prediction/interpolation policy behind that boundary.
7. Complete generic match/team/respawn capabilities.
8. Make editor inspection and mutation schema-driven.
9. Introduce region/chunk identity and generated-base delta persistence.
10. Add package distribution and synchronized multiplayer activation.
11. Migrate rendering/resource handles to generation-aware providers.
12. Delete each legacy bridge only after caller coverage and runtime evidence exist.

## Falsification tests

- Edit rocket source while server and two clients run; prove same session, entity IDs, packet continuity, generation switch, and remote visual result.
- Edit grenade source during an active round; prove existing grenade state survives.
- Add a new mode package without editing the EXE; select and run it on server and client.
- Add a new component after startup; replicate it, inspect it, edit it, and remove it.
- Create/destroy an arbitrary entity and prove both clients converge.
- Add a new command after startup and discover/invoke it without startup registration.
- Add a monster composition without a monster enum or packet branch.
- Change procedural generation and prove only newly generated regions use the new generator.
- Edit wireframe code and prove live GPU output changes.
- Force compile, validation, migration, and peer activation failures; prove last-good behavior remains active.

## Next 10 architecture passes

1. Projectile legacy-path deletion and two-client proof.
2. Entity create/destroy replication.
3. Typed player/NPC state migration slice.
4. Generic authoritative gameplay tick boundary.
5. Prediction/interpolation/reconciliation policy boundary.
6. Match/team/respawn genericization.
7. Schema-driven editor inspector and composition persistence.
8. Resource/GPU generation provider.
9. Procedural region/stream/persistence primitives.
10. Multiplayer distribution, trust, synchronized activation, and rollback.

## First pass

The single highest-leverage first migration is:

> Complete the authoritative projectile migration, prove it with a real two-client rocket/grenade live-edit trace, then remove the remaining normal legacy projectile producers and per-type policy branches.

This removes a genuine cold gameplay owner while preserving the low-level transport, collision, and physics mechanisms that belong in the kernel.

Human/runtime verification is still required for all live multiplayer, visual, rollback, and session-preservation claims.