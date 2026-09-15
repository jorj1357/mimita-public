# NPC generic actor state: team / role / behavior profile / target

- EST timestamp: 2026-09-14 20:19:40 EDT (UTC 2026-09-15T00:19:40Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--npc-actor-state-selftest` 11/11 + full
  suite 16/16)

## Ownership audit (before)

| Field | Current authority | Mirrors | Hot/cold consumers | Network path | Generic this pass |
|---|---|---|---|---|---|
| team | `ServerGamemodeState.matchTeams` / `ActorMatchDescriptor.teamId` | `Npc`/`ServerNpc.matchTeam`, snapshot participantTeams | cold gamemode/AI | snapshot participant fields | Yes -> `ActorTeamState` |
| role | `ActorMatchDescriptor.roleId` (string) | `Npc.movementProfileId`/`behaviorProfileId` | cold `serverResolveActorSpawnProfile` | snapshot participantRoles | Yes -> `ActorRoleState` |
| behavior profile | `Npc.behaviorProfileId` (string) resolved at spawn | `ServerNpc` | cold NPC AI | none direct | Yes -> `ActorProfileState` |
| movement preset | `Npc.movementProfileId` (string) | `ServerNpc` | cold movement | none direct | Yes -> `ActorProfileState` |
| current target | `Npc.serverTargetId` (uint32) | `ServerNpc` | cold NPC AI targeting | none direct | Yes -> `relationship.targets` |
| equipped tool/weapon | `Npc.body.equippedWeaponId` (string) + role loadout | `ServerNpc.equippedSlot` | cold combat/AI | snapshot wep state | No (documented blocker) |

## What moved (generic)
- New `network/actor-state.{h,cpp}`: authoritative dynamic components
  `ActorTeamState` (`team`), `ActorRoleState` (`roleHash`), `ActorProfileState`
  (`movementPresetHash`, `behaviorProfileHash`), all `networkPolicy ALL`, plus a
  `relationship.targets` edge. Works for players, NPCs, and future
  monsters/bots/minions; no NPC-specific versions.
- `assignMatchParticipants` now writes the generic team/role/profile components
  for every participant entity (player and NPC); the typed match/snapshot fields
  remain projections (bridges).
- NPC targeting now also writes `relationship.targets` alongside the typed
  `serverTargetId` projection.
- New hot module `modules/npc-ai-state.cpp` (`npc.ai-state` system on
  `gameplay.60`, priority 700): reads `ActorRoleState`/`ActorTeamState`/
  `ActorProfileState` for every entity and derives a `NpcAiSelection` component,
  so a real hot behavior consumes the generic state instead of typed Npc fields.

## Replication
All generic actor state (team/role/profile + target relationship) replicates
through the existing generic dynamic-component/relationship replication. No NPC
packet field and no NPC-specific codec; no new `game-api.h` field, NPC enum, or
`MonsterType`.

## Evidence
`--npc-actor-state-selftest` PASS 11/11:
- generic team/role/profile written + read back authoritatively (role id
  recovered from the component hash);
- target relationship resolves to a generic `EntityId`;
- client receives team/role/profile + target through generic replication;
- hot `npc.ai-state` system derived `NpcAiSelection` from the generic
  components;
- stale actor state cannot resurrect a destroyed entity;
- a runtime monster-like entity composes the same generic state.
Full suite PASS 16/16.

## Status labels
- SELFTEST PROVEN: generic authority for team/role/profile/target, replication,
  hot consumption, stale rejection, runtime monster-like composition.
- COMPILED INTEGRATION: participant assignment writes the components; NPC
  targeting writes the target relationship; the hot system runs on `gameplay.60`.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game NPC team/role/target parity.

## Authority
- Source of truth now: `ActorTeamState`/`ActorRoleState`/`ActorProfileState`
  components and `relationship.targets`.
- Projections (bridges): `ActorMatchDescriptor.teamId/roleId`,
  `Npc/ServerNpc.matchTeam`, `Npc.movementProfileId/behaviorProfileId`,
  `Npc.serverTargetId`, snapshot participant fields. No dual authority was added:
  the generic components are written as the canonical record; the typed fields
  are read-only projections for cold consumers not yet migrated.

## Honest limits
- `serverResolveActorSpawnProfile` still reads the typed `matchActors.roleId`
  (projection) rather than the generic role component; switching it is the next
  small consumer step.
- NPC equipped tool/weapon remains typed (blocker: full tool-entity inventory
  migration for NPCs; not done to avoid overlapping inventory work).
- Transform/velocity explicitly out of scope.
- The live two-client run was not performed.

## Files changed
`src/network/actor-state.{h,cpp}` (new), `src/network/npc-actor-state-selftest.{h,cpp}`
(new), `src/hot-reload/modules/npc-ai-state.cpp` (new),
`src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`,
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Make a cold consumer read the generic role (`serverResolveActorSpawnProfile`),
then the generic authoritative `gameplay.60` boundary; then NPC tool/equipment
state once inventory unification is ready.
