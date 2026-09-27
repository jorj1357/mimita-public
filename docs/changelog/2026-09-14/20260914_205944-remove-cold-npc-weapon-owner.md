# Remove the cold NPC weapon/attack owner (generic equipped-tool attack)

- EST timestamp: 2026-09-14 20:59:44 EDT (UTC 2026-09-15T00:59:44Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--gameplay-boundary-selftest` 11/11 +
  full suite 17/17)

## 1. NPC attack path audit
| Path | Owner | Class |
|---|---|---|
| `NpcSystem::updateOne` -> `NpcCombat::tryFire` (npc.cpp:1312) | cold NPC AI/weapon execution | A (was) -> now C (bypassed for migrated actors) |
| `NpcCombat::tryFire` weapon branches (hitscan/rocket/grenade/melee) | cold | C for projectile weapons |
| `broadcastNpcFiring` projectile branch -> `LiveBehavior::dispatchToolUse` | existing generic tool seam | A |
| `Npc.attackCooldown` / `Npc.body.weaponRuntimes` | cold NPC fire state | B (projection for migrated actors) |
| `Npc.body.equippedWeaponId` / `ServerNpc.equippedSlot` | cold weapon string | B (projection) |
| runtime tool entity path | generic | A |

## 2–4. Generic NPC tool ownership + action
- `actor-state` gained generic tool ownership: actor
  `--contains-item/equips-item-->` tool entity, with a replicated
  `ToolRefState { toolKey }` component on the tool entity.
  `actorStateEquipTool/GetEquippedTool/HasEquippedTool`.
- At `finalizeServerNpcSpawn`, NPC **projectile weapons** now get a generic tool
  entity equipped and set `Npc.hotToolOwned`; `NpcCombat::tryFire` is bypassed
  for those actors (exactly one attack owner).
- Hot `npc.combat-ai` (gameplay.60) resolves the equipped tool and, when in
  range + off cooldown, emits the generic `tool.primary-use` action with
  origin/direction. The existing DLL router dispatches to the hot tool behavior
  (`rocket-tool.cpp`), which spawns the canonical composition projectile
  (`HotProjectileState`). Tool-less monsters fall back to direct `damage.apply`.
- No `NpcFireRocket`/`NpcShootGun`/`NpcMeleeAttack`, no NPC-only tool router,
  no new `game-api.h` field.

## 7–8. One owner / cooldown
One AI decision produces one attack; fire-rate state lives on the tool entity
(`NpcAttackCooldown`), not a separate NPC-only timer, so the tool decides cadence
and the AI decides intent.

## Kernel fixes (found while proving this)
- Queued/runtime events now dispatch to handlers with a capability
  `GameplayContextV1` instead of a null host (`LiveBehavior::dispatchEvent` /
  `dispatchPayload`), so a hot system can emit a generic action and have it
  executed with capabilities.
- Schema registration for generic health/actor-state/weapon-state no longer
  short-circuits via a `done` flag; it always (re)registers, so a
  `DynamicComponentStore::clear()` (world reset/tests) cannot leave a schema
  missing.

## 10. Monster proof
`--gameplay-boundary-selftest` creates runtime monster-like entities (no
`MonsterType`): one equips a generic tool entity with tool key 5; `gameplay.60`
chooses the other as target, emits the generic action, and the hot tool spawns
the canonical projectile.

## 11. Replication
Tool ownership uses `contains-item`/`equips-item` + `ToolRefState` (networkPolicy
ALL); it replicates through the generic relationship/component path. Client
visual consumption of NPC tool state is not done (presentation owned elsewhere).

## 12. Typed weapon state classification
- `Npc.hotToolOwned`: transient compatibility flag (B) driving the cold bypass.
- `Npc.body.equippedWeaponId`, `ServerNpc.equippedSlot`, `Npc.attackCooldown`,
  `Npc.body.weaponRuntimes`: compatibility projections/fallback (B) for non-
  migrated (hitscan/melee) NPCs. Not dead; not deleted.
No dual authority: for migrated projectile NPCs the generic tool entity +
hot action are the owner; the typed fields are read-only projections.

## 13. Evidence
`--gameplay-boundary-selftest` PASS 11/11, including: generic NPC actor equips a
tool entity with its runtime key; the generic NPC tool action spawned the
canonical hot projectile; the tool cooldown ensures one attack per decision
window; plus the prior boundary checks. Full suite PASS 17/17.

## Status labels
- SELFTEST PROVEN: generic tool ownership, action routing to the hot tool
  behavior, canonical projectile spawn, cooldown, one-attack-per-window,
  destroyed-entity safety, event capability context.
- COMPILED INTEGRATION: NPC projectile weapons equip a tool entity at spawn;
  `NpcCombat::tryFire` bypassed for `hotToolOwned`; `npc.combat-ai` emits the
  generic action on the server tick.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game NPC rocket/grenade attack parity (no double
  fire) and client visual of NPC shots.

## Honest limits
- Only **projectile** NPC weapons are migrated (rocket/grenade via the canonical
  hot projectile path). Hitscan/melee NPCs still use `NpcCombat::tryFire`
  (documented next slice: add hot hitscan/melee tool behaviors + a way for the
  kernel to know a hot behavior exists for a key so the cold path can be bypassed
  safely).
- `actorStateRoleIdForHash` mapping is process-local; fine for the server.

## Files changed
`src/network/actor-state.{h,cpp}`, `src/npc/npc.h`, `src/npc/npc.cpp`,
`src/network/server-npcs.cpp`, `src/network/actor-health.cpp`,
`src/network/server-weapon-state.cpp`,
`src/hot-reload/modules/npc-combat-ai.cpp`,
`src/live-code/live-behavior.cpp`, `src/network/gameplay-boundary-selftest.cpp`;
docs + this changelog.

## Next (auto-selected)
Hot hitscan/melee tool behavior + kernel "does a hot behavior exist for this
tool key" query so the cold NPC attack owner can be bypassed for all weapons;
then match/team/objective/respawn genericization.
