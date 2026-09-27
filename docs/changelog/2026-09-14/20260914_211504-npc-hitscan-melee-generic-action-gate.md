# NPC hitscan/melee generic tools + generic action-handled gate

- EST timestamp: 2026-09-14 21:15:04 EDT (UTC 2026-09-15T01:15:04Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--gameplay-boundary-selftest` 15/15 +
  full suite 17/17)

## Combat ownership now generic
- Hot `hitscan-tool.cpp` (key 1 revolver): reads the actor's generic target
  relationship, raycasts the world through the kernel query primitive (owns
  range/occlusion policy), applies `damage.apply`, spawns an effect. No
  NPC-specific hitscan function, no weapon switch.
- Hot `melee-tool.cpp` (key 4 swordsword): contact-range check + `damage.apply` +
  effect. No `NpcMeleeAttack`/`MeleeManager`.
- Every armed NPC now gets a generic equipped tool entity at spawn (any weapon,
  runtime key), not just projectile weapons.

## Generic action-handled gate (replaces `Npc.hotToolOwned`)
- New generic mechanism: when the hot action router (`combat-policy.cpp`) handles
  a `tool.primary-use`, it records `ActorActionState { lastHandledTick, handled }`
  on the actor via capabilities.
- `actorStateActionHandled(entity, tick)` lets the cold legacy fallback consult
  that record. `npc.cpp` bypasses `NpcCombat::tryFire` when the actor's action was
  handled at the current simulation tick.
- `Npc.hotToolOwned` is **deleted**. The kernel learns no weapon category and no
  `hasHotWeaponBehavior(toolKey)` API was added; the decision comes from generic
  dispatch/handling state. Unknown tool keys are unhandled → compatibility
  fallback may run (fails safe).
- Cooldown remains tool-owned (`NpcAttackCooldown` on the tool entity); the AI
  decides intent, the tool decides cadence.

## Tests
`--gameplay-boundary-selftest` PASS 15/15 adds: hot hitscan NPC tool dealt
damage; the generic handled gate is set for a handled hitscan action; hot melee
NPC tool dealt damage; an unknown tool key is unhandled (safe cold fallback).
Prior checks (generic tool ownership, canonical projectile spawn, tool cooldown
one-attack-per-window, target selection, authoritative mutation, destroyed-entity
safety, determinism) still pass. Full suite PASS 17/17.

## Audit: cold NPC combat
- `NpcCombat::tryFire` remains only as **compatibility fallback** (class C) for
  tool keys with no hot behavior; it is bypassed whenever a hot action handled
  the tick. Not deleted (unknown/legacy weapons still rely on it).
- `Npc.attackCooldown` / `body.weaponRuntimes`: projections/fallback (B).
- `Npc.body.equippedWeaponId`, `ServerNpc.equippedSlot`: projections (B).
- Low-level ray/sweep/contact queries stay kernel primitives.

## Status labels
- SELFTEST PROVEN: projectile + hitscan + melee NPC attacks through the one
  generic tool/action path; generic handled gate; tool-owned cooldown; unknown
  key safe; destroyed-entity safety.
- COMPILED INTEGRATION: all-weapon tool ownership at NPC spawn; `npc.cpp`
  consults the generic handled gate and bypasses `tryFire`; hot hitscan/melee
  behaviors registered.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game NPC hitscan/melee/rocket parity (no double
  fire) and client visuals.

## Not done this pass (explicitly deferred)
Match/team/objective/respawn genericization (mission items 11–18) was **not**
started. NPC combat ownership is now generic enough to leave; the next pass
should begin generic match state + phase/respawn/objective ownership.

## Files changed
`src/hot-reload/modules/tools/hitscan-tool.cpp` (new),
`src/hot-reload/modules/tools/melee-tool.cpp` (new),
`src/hot-reload/modules/tools/combat-policy.cpp` (handling record + schema),
`src/network/actor-state.{h,cpp}` (`actorStateActionHandled`),
`src/npc/npc.h` (removed `hotToolOwned`), `src/npc/npc.cpp` (generic gate),
`src/network/server-npcs.cpp` (all-weapon tool ownership),
`src/network/gameplay-boundary-selftest.cpp`; docs + this changelog.

## Next (auto-selected)
Generic match state + phase/team/objective/respawn ownership (one real cold
match owner moved hot), then transform/velocity generic state.
