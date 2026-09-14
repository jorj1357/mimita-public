# Generic damage across entity types + generic item containment/equip

- EST timestamp: 2026-09-14 17:36:32 EDT (UTC 2026-09-14T21:36:32Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--hot-combat-selftest` + full suite)

## Success criteria addressed
- `damage.apply` is now generic over **players, NPCs, and non-actor damageable
  entities** through one capability; no `damagePlayer`/`damageNpc`.
- Item containment/equip is relationship-owned with the **same item EntityId**
  surviving inventory -> equip -> drop -> pickup -> re-equip.
- Lifecycle behavior bindings (`on.equip`/`on.unequip`/`on.drop`/`on.pickup`)
  dispatch through the existing per-entity binding path, no central switch.

## 1. Generic damage
`serverApplyEntityDamage` (`network/server-damage.cpp`):
- Player victim: existing shared actor boundary (attacker/no-attacker paths).
- **NPC victim**: authoritative health on the `ServerNpc` mirror, with the shared
  kill owner (`serverGamemodeRecordKill`). One capability, no NPC-specific slot.
- **Non-actor damageable entity**: authoritative `HealthComponent` state, so
  destructibles/future concepts take damage with no actor plumbing.

## 2. Item state / maps
Runtime tools keep their item state as dynamic components on the tool entity
(the canonical hot projectile path added by the gameplay agent likewise stores
per-projectile state as a dynamic component). The legacy weapon ammo/cooldown/
reload maps remain as a **bridge for registered weapons only** and are no longer
required for runtime tools.

## 3. Containment / equip relationships
`network/server-attack.cpp` adds:
- `relationship.contains-item` (Actor -> Item) and
  `relationship.equips-item` (Actor -> Item).
- `serverItemEquip` / `serverItemUnequip` / `serverItemDrop` / `serverItemPickup`
  / `serverItemContains`. The item entity is never destroyed/recreated across the
  lifecycle; equip clears any previous equipped edge (one equipped item).
- Server commands `invequip` / `invpickup` / `invdrop` / `invunequip`
  (player actions, before the host gate).
- `serverEquipRuntimeTool` now routes through the generic item relations.

## 4. Behavior bindings
Each lifecycle transition calls `LiveBehavior::runBehaviorBindings(itemEntity,
<on.equip|on.unequip|on.drop|on.pickup>)`. `on.primary-use` already dispatches
per-entity. No per-event-type kernel switch.

## Evidence
- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --hot-combat-selftest` -> **PASS**, including new checks:
  `damage.apply` on an NPC victim; `damage.apply` on a non-actor damageable
  entity; item contained after equip; not contained after drop; and the same
  item `EntityId` survives the full lifecycle.
- Full self-test suite PASS (13/13).

## Files changed
`src/network/server-damage.cpp`, `src/network/server-attack.cpp`,
`src/network/server-packet-chat.cpp`, `src/network/server.h`,
`src/network/hot-combat-selftest.cpp`; docs + this changelog.

## Honest limitations
- Legacy weapon slot/string maps and `ServerPlayer.runtimeToolId`/
  `equippedToolEntity` remain as bridges for registered weapons and the input
  path; they are not yet deleted. Runtime tools no longer depend on them.
- NPC damage updates the network mirror + kill owner but does not yet broadcast
  a dedicated NPC damage event from the generic path.
- Generic dynamic-component replication (priority 7) is **not** implemented this
  pass; it remains the next major owner.
- No live single-process/visual acceptance; tree co-edited by the gameplay
  agent.

## Next
Generic dynamic-component replication (schema/hash/networkPolicy over the
existing reliable gameplay-event channel, no new packet type), then generic
projectile/tool state replication, then hot network policy.
