# NPC generic health + lifecycle: authoritative component, generic damage, replication

- EST timestamp: 2026-09-14 19:42:57 EDT (UTC 2026-09-14T23:42:57Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--npc-entity-selftest` 11/11 + full
  suite 15/15)

## NPC ownership audit (before)

| Field / state | Authoritative owner | Mirror | Network path | Cold callers |
|---|---|---|---|---|
| NPC identity / EntityId | `Npc::id` -> `ServerNpc::entityId` -> `Ecs::ensure(Server,Npc,id)` | ECS identity | snapshot entity list + typed lifecycle | server-npcs, server-gamemode |
| NPC health | `Npc::body.currentHp` after `syncServerNpcDamageToNpc` copies `ServerNpc.health` | `ServerNpc.health` (rebuild) + ECS `HealthComponent` (slice mirror) | snapshot `health` field | server-projectiles (cold NPC damage), server-damage (generic) |
| NPC transform/velocity | `Npc::body` | `ServerNpc` | snapshot + interpolation | server-npcs, multiplayer-* |
| NPC AI/target/weapons | `Npc` fields | `ServerNpc` wep state | snapshot | npc/*, server-npcs |
| NPC death/respawn | `Npc::body.dead/respawnTimer` driven by `ServerNpc.health<=0` | `ServerNpc` | snapshot | server-npcs |

## What moved (this pass)
One real ownership slice: **NPC health**, plus NPC entity **lifecycle marking**.

- New `network/actor-health.{h,cpp}`: `ActorHealthState` is a **dynamic
  component** (id `gameHash("ActorHealthState")`, `networkPolicy = ALL`) that is
  the authoritative health for an NPC/monster-like entity. The typed
  `HealthComponent` and `ServerNpc.health` become projections.
- `finalizeServerNpcSpawn` (the single NPC spawn boundary) now ensures the NPC
  entity, initialises `ActorHealthState` from max HP, mirrors it into
  `HealthComponent`, and marks the entity for generic lifecycle replication
  (`serverReplicateEntity`).
- `rebuildServerNpcMap` projects `ServerNpc.health` **from the component**; the
  existing `syncServerNpcDamageToNpc` then drives the simulated body from
  `ServerNpc.health`, so the authority chain is component -> mirror -> body.
- `serverApplyEntityDamage` (generic `damage.apply`) mutated the ServerNpc struct
  directly before. It now resolves the generic entity: for NPCs it mutates the
  authoritative `ActorHealthState`, projects `ServerNpc.health` + typed
  `HealthComponent`, and on the alive->dead transition emits the generic
  `actor.killed` runtime event (no NPC-specific callback). Non-actor damageable
  entities use the same component when present, else the typed `HealthComponent`.
- `EntityRegistry::destroy` appends to a generic destroyed-id log;
  `dynamicReplicationCollectServer` emits generic `DESTROY` from it. NPC entity
  destruction therefore uses the generic entity lifecycle, not an NPC packet.

## 3–4. Health authority + generic damage
`damage.apply(npcEntity, fact)` -> `ActorHealthState` -> projection -> death
fact. No `if victim is NPC { mutate ServerNpc.health }` ownership remains; the
ServerNpc write is explicitly a bridge.

## 6. Client replication
NPC health replicates through the existing generic dynamic-component path
(schema descriptor + CREATE + upsert/remove in one envelope). No NPC packet and
no new health field in a packet.

## 7–8. Evidence
`--npc-entity-selftest` PASS 11/11:
- runtime NPC-like entity created after startup owns authoritative
  `ActorHealthState`;
- `damage.apply` mutated generic component health (100 -> 75);
- health replicated to a simulated client store (schema + CREATE + value);
- lethal damage -> dead component;
- generic `DESTROY` emitted and the client removed the entity;
- stale update after DESTROY cannot resurrect it;
- a second runtime monster-like entity uses the same path (no `MonsterType`).
Full suite PASS 15/15. `HotProjectileState`/`ActorHealthState` schemas are
replicated generically.

## 11. Dead typed lifecycle code
Not deleted yet. `ServerNpc` health/lifecycle remain **compatibility
projections** (classification A). No two permanent lifecycle owners were added;
the component/log are authoritative, the struct is a mirror. Deleting the typed
NPC health write paths is a follow-up once transform/AI slices move.

## Status labels
- SELFTEST PROVEN: authority, generic damage, replication encode/decode/apply,
  destroy, stale rejection, migration.
- COMPILED INTEGRATION: NPC spawn boundary initialises the component and marks
  the entity; snapshot projects from the component; damage path uses it.
- LIVE MULTIPLAYER PROVEN: no (no two-client harness run).
- HUMAN VERIFICATION NEEDED: in-game NPC health/death behaviour parity.

## Honest limits
- Transform/velocity/AI/weapons remain typed (explicitly out of scope).
- The cold projectile explosion still writes `ServerNpc.health` directly; that
  path should route through `damage.apply` next (or be removed with the legacy
  projectile cleanup owner).
- `ActorHealthState` and the typed `HealthComponent` coexist during migration.

## Files changed
`src/network/actor-health.{h,cpp}` (new), `src/network/npc-entity-selftest.{h,cpp}`
(new), `src/network/server-damage.cpp`, `src/network/server-npcs.cpp`,
`src/network/dynamic-replication.cpp`, `src/ecs/entity-registry.{h,cpp}`,
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Route the remaining cold NPC damage (projectile explosion) through
`damage.apply`; then the next typed NPC state slice (transform/velocity is
highest leverage but touches interpolation/prediction — alternatively
team/role or AI state); then the generic authoritative `gameplay.60` boundary.
