# Authoritative gameplay.60 boundary + hot NPC combat

- EST timestamp: 2026-09-14 20:31:32 EDT (UTC 2026-09-15T00:31:32Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--gameplay-boundary-selftest` 8/8 + full
  suite 17/17)

## 1. Role/profile consumer shift
`serverResolveActorSpawnProfile` now resolves the actor's role from the entity's
authoritative `ActorRoleState` component (via `actorStateRoleHash` ->
`actorStateRoleIdForHash`), falling back to the typed
`ActorMatchDescriptor.roleId` projection only when no generic role exists. No
`NpcType`/`MonsterType` branch; existing players/NPCs keep behavior (fallback).

## 2–3. Authoritative server tick / gameplay.60 boundary
The dedicated server already runs one generic domain execution per fixed step in
`runServer`:
`establish ServerContext` -> ingest packets -> `runDomain(GAME_DOMAIN_GAMEPLAY)`
+ `runRegisteredDomains` + `drainEvents` -> player/projectile/NPC low-level sim
-> snapshot/replication. The cold loop knows "run gameplay.60", not
"run weapons/monsters/mode". Documented in `hot-kernel-next-steps.md`.

## Audit: server tick stages
| Stage | Class |
|---|---|
| input validation / normalize | D (low-level) with B policy seams |
| gamemode lifecycle (cold) | C (orchestration) — target of a later pass |
| NPC AI target selection | **migrated to A** for generic actors this pass |
| movement | A (`movement.main`) / D collision |
| attacks (player) | B policy + D mechanism |
| projectiles | A (hot projectile system) + D collision/replication |
| damage | A policy + D application |
| respawn | C (cold) |
| replication | D mechanism + A component policy |

## 4. Capabilities
Hot gameplay uses the existing generic capabilities (entity/component/
relationship, `damage.apply`, read typed Transform, `resolveCapability`,
`emitEvent`). No raw containers exposed; no `NpcAttackFn`/`MonsterMoveFn`/
`WeaponTickFn` added; no new `game-api.h` field.

## 5–6. Real NPC behavior end-to-end (hot)
New hot module `modules/npc-combat-ai.cpp` (`npc.combat-ai`, `gameplay.60`,
priority 650):
- enumerates entities with `ActorHealthState`, reads `ActorTeamState` and typed
  `Transform`;
- chooses the nearest hostile (different team) target, writes
  `relationship.targets`;
- when in range, performs a real authoritative attack through the generic
  `damage.apply` capability, gated by a generic `NpcAttackCooldown` component.

## 7. Cold orchestrator removed/bypassed
`simulateSharedNpcs` no longer owns target selection for generic actors: it now
reads `relationship.targets` (authoritative) and follows the hot-chosen target;
the cold nearest-enemy search remains only as a fallback for actors without
generic state. The typed `Npc.serverTargetId` is a projection.

## 8. Generic monster proof
A runtime entity with `ActorHealthState` + `ActorTeamState` + `ActorRoleState` +
`Transform` participates in the hot combat path and is damaged authoritatively —
no `MonsterType`, no NPC registration.

## 9. Gamemode interaction
The boundary reads generic actor state; a future hot mode system can assign
team/role (`actorState*`), react to kills (generic `actor.killed`), spawn actors
(generic lifecycle), finish matches (`match.finish`), and set respawn policy via
`match.setPhase` — no new cold orchestration required for those seams. Not
migrated this pass.

## 10. Tick order / determinism
`GenericRuntime` sorts systems by `(domainId, priority, id)` at activation, so
ordering is stable and independent of registration accident; `npc.combat-ai`
(650) runs after `npc.ai-state` (700 is higher, so after) — ordering is by
priority ascending. The self-test proves identical runs produce identical
results.

## 11. Generation safety
Hot systems activate only at the top-of-tick safe boundary via the loader
(`pollAndAdvance`); state migration runs before new systems consume state; a
failed candidate keeps last-good. Proven by `--live-code-selftest`; not re-run
headlessly here.

## 12. Evidence
`--gameplay-boundary-selftest` PASS 8/8: hot package active; hot AI chose a
target and wrote `relationship.targets`; authoritative mutation via
`damage.apply`; same-tick double-attack prevented by the generic cooldown;
cooldown elapse re-attacks; AI selection derived from generic role/team state;
destroyed entity ignored; deterministic across identical runs. Full suite
17/17.

## Status labels
- SELFTEST PROVEN: boundary invocation, hot decision + authoritative mutation,
  cooldown, determinism, destroyed-entity safety, generic-role-driven selection.
- COMPILED INTEGRATION: `serverResolveActorSpawnProfile` reads generic role; NPC
  targeting reads `relationship.targets`; `npc.combat-ai` runs on the server tick.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game NPC target/attack behavior parity.

## Honest limits
- The cold NPC *attack* (weapon/fire) path is not yet bypassed; the hot system
  applies generic melee damage, so both may act on a target. Removing the cold
  attack owner is the next step.
- `serverResolveActorSpawnProfile` still uses the role definition's movement/
  behavior presets (role->preset mapping); only the role identity read is generic.
- Transform remains snapshot-based; only a read projection onto the entity was
  added.
- No live two-client run.

## Files changed
`src/hot-reload/modules/npc-combat-ai.cpp` (new),
`src/network/gameplay-boundary-selftest.{h,cpp}` (new),
`src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`,
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Bypass the cold NPC attack owner for generic actors (route NPC weapon/attack
through the hot path), then NPC tool/equipment entity ownership, then
match/team/objective/respawn genericization.
