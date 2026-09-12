# Entity/component migration (vertical slice)

Date: 2026-09-12
Status: in progress, vertical slice only
Related: `docs/architecture/ecs-entity-etc/ecs.md`,
`docs/features/live-code-development/live-code-development.md`

## Before

Player, NPC, server player/NPC mirrors, and projectiles were separate
specialized structures with separate identity spaces:

```text
Player / ServerPlayer        -> playerId
Npc / ServerNpc              -> Npc::id, ServerNpc::entityId
RocketLauncherState::Rocket  -> pointer-derived ownerId
ServerProjectile / NetworkProjectile -> projectileId / fireSerial
```

Control was decided by type checks (`isPlayer`/`isNpc`), and the local rocket
owner was a cast pointer: `ownerId = (uint32_t)(uintptr_t)(&owner)`.

## Transitional state (this slice)

```text
specialized legacy state
      | adapters (src/ecs/actor-entities.*)
EntityId + components (src/ecs/entity-registry.*)
      |
shared systems and the hot actor module
```

`EntityId` is a stable 64-bit value packed as
`generation(16) | realm(4) | domain(8) | legacyId(32)`. It reuses the existing
repository ids so no parallel identifier is introduced:

| Domain | Legacy id source | Entity realm |
|---|---|---|
| Player | `playerId` / local player | `Server`, `Local`, `ClientPredicted` |
| Npc | `Npc::id` / `ServerNpc.entityId` | `Server` |
| Projectile | `ServerProjectile::id`, provisional client id, local rocket serial | `Server`, `ClientPredicted` |

The registry (`EntityRegistry::instance()`) stores components sparsely per type;
adding a component never changes an entity's identity.

### Components introduced

`EntityIdentity`, `Transform`, `Velocity`, `Body`, `Health`, `ControlSource`,
`NetworkAuthority`, `MovementIntent`, `AimIntent`, `FireIntent`,
`WeaponInventory`, `Projectile`, `Owner`, `Damage`, `Collider`.

`ControlSource` is the data replacement for `isPlayer`/`isNpc`:
`LocalHuman`, `ServerNpc`, `RemoteNetwork`, `Replay`, `Scripted`.

### Temporary duplicated state

| Concern | Authoritative now | Mirrored into components | Sync direction | Planned deletion point |
|---|---|---|---|---|
| Actor identity | `Player`/`Npc`/`ServerPlayer`/`ServerNpc` | `EntityIdentity`, `ControlSource`, `NetworkAuthority` | legacy -> component | when spawn/lifecycle creates entities directly |
| Actor transform/health | legacy structs | `Transform`, `Velocity`, `Health`, `Body` | legacy -> component | when movement/damage systems read components |
| NPC intent | `InputState` in `npc.cpp` | `MovementIntent`, `AimIntent` | component <- hot decision | when the movement kernel consumes `MovementIntent` |
| Rocket identity/owner | `ServerProjectile`/`RocketLauncherState::Rocket` | `EntityId`, `Owner`, `Projectile`, `Collider` | legacy -> component (owner set once, then component is canonical for attribution) | when the projectile system is entity-driven end to end |

The rocket `Owner` component is already canonical for attribution: damage
evidence and the explosion path resolve the firing entity through it, not
through the old pointer cast.

## Destination

```text
Entity
+ Components
+ Intent (HumanInputSystem / NpcBrainSystem / NetworkInputSystem / ReplayInputSystem)
+ Systems (MovementSystem, WeaponSystem, ProjectileSystem, DamageSystem)

"actor" is an emergent capability, not a root class or type.
```

Systems query required components, not names. The same entity model covers local
players, remote players, NPCs, replay bodies, and projectiles.

## What is entity-driven after this slice

- Stable `EntityId` for local player, server players, NPCs, and rockets.
- `ControlSource` decides who may be hot-driven (never `RemoteNetwork`/`Replay`).
- Rocket owner attribution through `Owner(EntityId)`.
- Journal evidence: `entity_registered`, `entity_destroyed`,
  `control_source_set`, `rocket_entity_spawned`, `damage_applied`.

## What remains legacy

- `Player`, `Npc`, `ServerPlayer`, `ServerNpc`, `ServerProjectile` still own the
  authoritative gameplay fields.
- Movement, weapons, damage, death, and replication still run through those
  structs; components mirror them for the migrated slice.
- The client-predicted rocket entity is registered but not yet reconciled on
  authoritative adoption; predicted entities may outlive their network
  projectile until a sweep is added.
- No full-world ECS conversion, dynamic schemas, or cross-platform determinism.

## Verification

`mimita.exe --entity-slice-selftest` checks distinct stable ids, required
components, differing control sources, rocket owner resolution, deterministic
projectile simulation, the health/death component path, and identity survival
across a DLL reload cycle. No tuned gameplay constant is pinned.
