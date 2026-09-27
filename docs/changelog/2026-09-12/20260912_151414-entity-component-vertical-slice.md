# Entity/component vertical slice: player, NPC, and rocket as entities

- EST timestamp: 2026-09-12 11:14:14 EDT (UTC 2026-09-12T15:14:14Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Task

Begin the migration from separate player/NPC/projectile structures toward a
universal entity + component model, preserving gameplay, networking, server
authority, prediction/reconciliation, and hot reload. Build the smallest
vertical slice (local human player, one NPC, rocket) without a giant rewrite.

Decisions confirmed before implementation:

- first proof path: networked host/client 1v1 vs NPC;
- participation depth: identity + control source + intent + rocket owner;
- `EntityId`: domain-tagged 64-bit reusing legacy ids;
- registry location: new `src/ecs/` module.

## What was implemented

### 1. Entity core (`src/ecs/`)
- `entity-types.h`: stable `EntityId` packed as
  `generation(16) | realm(4) | domain(8) | legacyId(32)`, reusing existing
  player/NPC/projectile ids.
- `components.h`: `EntityIdentity`, `Transform`, `Velocity`, `Body`, `Health`,
  `ControlSource`, `NetworkAuthority`, `MovementIntent`, `AimIntent`,
  `FireIntent`, `WeaponInventory`, `Projectile`, `Owner`, `Damage`, `Collider`.
- `entity-registry.h/.cpp`: sparse per-component storage keyed by `EntityId`;
  adding a component never changes identity. `create` is idempotent so ids stay
  stable across hot reloads. Registration/destroy is journaled.

### 2. Adapters (`src/ecs/actor-entities.*`)
- `ensure`, `ensureLocalPlayerEntity`, `setControlSource`, `setAuthority`,
  `setTransform`, `setVelocity`, `setHealth`, `setBody`, `setMovementIntent`,
  `setAimIntent`, `setWeaponInventory`, `spawnRocket`, `setRocketMotion`,
  `despawn`, `journalDamage`.
- `ControlSource` is the data replacement for `isPlayer`/`isNpc`.

### 3. Server integration
- `src/network/server-players.cpp` `simulatePlayer`: registers/syncs the server
  player entity (identity, authority, transform, velocity, health).
- `src/npc/npc.cpp` `applyLiveActorBehavior`: maintains the NPC entity and all
  capability components, builds `ActorStateV1` from `Transform`/`Velocity`/
  `Health` components, and writes the hot decision back as `MovementIntent`/
  `AimIntent`. Runs for the server-authoritative NPC path.
- `src/network/server-projectiles.cpp`: registers the authoritative rocket
  entity (owner = shooter player entity) at spawn and destroys it on explode and
  on dead-owner cancel.
- `src/network/server-npcs.cpp`: registers NPC-fired rocket entities owned by
  the NPC entity.

### 4. Client integration
- `src/network/multiplayer-projectiles.cpp` `mpPredictProjectileAttack`:
  registers the client-predicted rocket entity owned by the local human player
  entity, keyed by the provisional projectile id.

### 5. Rocket owner replacement (real behavior)
- `src/combat/weapon-rocket-launcher.h/.cpp`: `Rocket` now carries `entityId`
  and `ownerEntity`; `fire()` takes an owner `EntityId`/realm and registers the
  entity. `doExplosion` updates the victim NPC's `Health` component and journals
  `damage_applied` with the owner entity. `update()`/`clear()` despawn entities.
- Local fire (`src/combat/weapon-system.cpp`) and NPC fire
  (`src/npc/npc-combat.cpp`) pass their owner entities.

### 6. Structured evidence
- Journal events: `entity_registered`, `entity_destroyed`,
  `control_source_set`, `rocket_entity_spawned` (with `owner_entity_id`),
  `damage_applied`. Behind the existing live journal; event-driven, not
  per-frame.

### 7. Tests
- New `mimita.exe --entity-slice-selftest` (`src/ecs/entity-slice-selftest.*`):
  distinct stable ids, required components, differing control sources, rocket
  owner resolution, deterministic projectile simulation, health/death path,
  identity survival across a DLL reload cycle, and journal evidence.
- Running `hotreload`-era self-test still passes.
- New terminal `entity_list` (`src/terminal/entity-commands.*`) reports live
  entities, domains, legacy ids, control sources, and health.

### 8. Documentation
- `docs/architecture/ecs-entity-etc/ecs-migration.md`: Before / Transitional /
  Destination, the temporary-duplication table (authoritative, mirrored, sync
  direction, deletion point), and what remains legacy.
- `docs/ROUTER.md`: added an entity/component route row.

## Evidence

`python build_agent.py` -> `Status: SUCCESS`.

`mimita.exe --entity-slice-selftest` -> `PASS`, including:

```text
[ok] distinct stable player/npc entity ids
[ok] human and npc control sources differ
[ok] rocket entity created
[ok] rocket owner resolves to player entity
[ok] identical projectile input is deterministic
[ok] projectile lifetime path completes
[ok] victim health/death component path works
[ok] entity identity survives DLL reload cycle
[ok] entity journal evidence written
```

`mimita.exe --live-code-selftest` -> `PASS` (no regression).

Journal excerpt (`logs/features/live-code/2026-09-12/live_events_*.jsonl`):

```text
entity_registered player   local  entity_id=3302829850625 legacy_id=1
control_source_set human   entity_id=3302829850625
entity_registered npc      server entity_id=8589934634    legacy_id=42
control_source_set ai      entity_id=8589934634
rocket_entity_spawned      entity_id=12884901895 owner_entity_id=3302829850625
damage_applied             entity_id=8589934634 owner_entity_id=3302829850625 amount=80
```

## What is entity-driven vs legacy

Entity-driven now: stable `EntityId` for local player, server players, NPCs,
and rockets; `ControlSource` gating; rocket owner attribution through
`Owner(EntityId)`; NPC movement/aim intent stored as components; entity journal
evidence.

Still legacy: `Player`/`Npc`/`ServerPlayer`/`ServerNpc`/`ServerProjectile` own
the authoritative fields; movement, weapons, damage, death, and replication run
through them; components mirror for the migrated slice. Client-predicted rocket
entities are not yet reconciled on authoritative adoption.

## Live-edit status

The two running `mimita.exe` instances exited before the live in-game hot-edit
proof could be observed (no `mimita` processes remained). The EXE-side changes
require launching the freshly built `mimita.exe`; the hot-module behavior
(`actor-behavior.cpp`, `presentation.cpp`, `game-api.h`) remains live-reloadable
without restarting. A live 1v1 hot-edit proof is still required from human
review.

## Files

New: `src/ecs/entity-types.h`, `src/ecs/components.h`,
`src/ecs/entity-registry.h/.cpp`, `src/ecs/actor-entities.h/.cpp`,
`src/ecs/entity-slice-selftest.h/.cpp`, `src/terminal/entity-commands.h/.cpp`,
`docs/architecture/ecs-entity-etc/ecs-migration.md`.

Changed: `src/npc/npc.cpp`, `src/npc/npc-combat.cpp`,
`src/combat/weapon-rocket-launcher.h/.cpp`, `src/combat/weapon-system.cpp`,
`src/network/server-players.cpp`, `src/network/server-projectiles.cpp`,
`src/network/server-npcs.cpp`, `src/network/multiplayer-projectiles.cpp`,
`src/sim/simulate-tick.cpp`, `src/game/game-cli.cpp`,
`src/main-systems.cpp`, `docs/ROUTER.md`.

## Human review still required

- Launch the new `mimita.exe`, run the networked 1v1 vs NPC with a rocket
  launcher, and confirm movement/aim/shoot/damage/kill, networking,
  authority/prediction/reconciliation, and hot reload all still work.
- Observe `entity_list` in a live match and confirm player/NPC/rocket entities
  with control sources.
- Perform a live hot-edit of `actor-behavior.cpp` and confirm activation with a
  successful reload.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
