# Generic actor spawn/reset + full Counter-Strike phase ownership

- EST timestamp: 2026-09-15 09:45:48 EDT (UTC 2026-09-15T13:45:48Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--counterstrike-selftest` 22/22 +
  full suite 22/22)

## 1. Spawn/respawn ownership audit
| Stage | Owner before | Classification | Now |
|---|---|---|---|
| match phase change / timing | `serverGamemodeTick` + `beginMatchCountdown` | generic phase mechanism | **HOT** for CS (MatchPhaseOwnership) |
| actor revive / round-start reset | `resetGamemodeActorsAtMapSpawn` | mode policy (cold) | **HOT** (`actor.spawn`) |
| spawn point selection | `gamemodeSpawnPoint(d, team)` + `MapConfig.teamSpawns` | mode policy + map data | kernel `map.anchors` (`spawn.team`) -> **HOT** choice |
| position/orientation | `beginAuthoritativeTransform` + typed `p.pos/yaw` | typed compatibility | **generic Transform authoritative**; typed is projection |
| velocity reset | typed `p.vel`/`npc.body.vel` | typed compatibility | **generic Velocity authoritative**; typed is projection |
| health reset | `resetPlayerForSpawn`/`npc.body.currentHp` | mode policy | **HOT** via `actor.spawn` health flag; generic Health |
| dead/spectator | typed `p.dead`/`CsDead` marker | mode policy | **HOT** (`CsDead` cleared at round start) |
| respawn timer | typed `respawnSeconds` | generic lifecycle | kernel mechanism (`match.lifecycle`) |
| team assignment | `assignMatchParticipants` | mode policy | **HOT** deterministic assignment |
| snapshot/network propagation | typed snapshot fields | typed compatibility | **projections** (unchanged this pass) |

Nothing proven dead was deleted; the cold spawn/phase path is now compatibility
fallback for modes that do not register a hot lifecycle mode.

## 2-3. Generic authoritative spatial state (server authority only)
- The generic `TransformComponent`/`VelocityComponent`/`HealthComponent` are
  written by the new `actor.spawn` mechanism as the authoritative mutation.
- Typed `ServerPlayer.pos/vel/health` and `ServerNpc.pos/vel/health` are kept
  in sync as projections for the existing client snapshot; no interpolation,
  reconciliation, prediction, or packet cadence was changed.

## 4-5. Generic actor spawn/reset + anchors
- New `GAME_CAP_ACTOR_SPAWN` + `GameActorSpawnV1` (actor entity, position,
  velocity, yaw, health, flags) and kernel `serverSpawnOrResetActor`.
- `GameMapAnchorV1` gained a generic `tag` + `yaw`; `serverMapAnchors` now emits
  `spawn.team` anchors (tag = team index) alongside `objective.site` anchors. No
  `ctSpawn[]`/`tSpawn[]` kernel arrays.

## 6-9. Hot CS countdown, round-start reset, and full phase ownership
- Hot `gamemode.counterstrike.cpp` owns: countdown timer, ACTIVE, RESULTS,
  INTERMISSION, next round; participant team assignment (deterministic over
  sorted participant entity ids); round-start spawn/reset through `actor.spawn`
  (deterministic anchor choice per team); objectives (carrier/plant/defuse/
  bomb/explosion/elimination/timeout); `match.round-result`.
- It claims `MatchPhaseOwnership` + `ObjectiveOwnership`, so the cold phase
  machine and cold bomb policy are bypassed for CS.

## 10. beginMatchCountdown removed as CS owner
- For the shipping `counterstrike` mode, `beginMatchCountdown`/
  `resetGamemodeActorsAtMapSpawn` no longer run (the phase-ownership gate returns
  before the cold switch). They remain compatibility fallback for unmigrated
  modes.

## 11-12. Transform/Velocity authority + determinism
- Verified generic Transform/Velocity/health are written on spawn for a
  player-like actor and an NPC-like actor. Spawn/team assignment is deterministic
  (sorted entity ids; explicit per-team anchor index).

## 13-15. Tests
`--counterstrike-selftest` PASS 22/22: hot countdown -> active, kernel phase set,
ownership claimed, deterministic teams, generic Transform/Velocity/health on
spawn, plant -> defuse (defenders), RESULTS, results/intermission -> next round,
respawn on next round, plant -> explosion (attackers), no stale objective edges,
killed actor revived next round, duplicate interaction safe, participant
destruction safe, and the generic `actor.spawn` mechanism working for a non-CS
runtime actor entity.

## 16. Network projection
Generic authoritative Transform/Velocity -> existing snapshot representation is
unchanged. Typed snapshot fields now documented as projections:
`ServerPlayer.pos/vel/yaw/health/dead`, `ServerNpc.pos/vel/health`. No new
position packet.

## 17. Do not touch
Renderer/HUD, GLB/resources, client projectile identity, interpolation/
prediction math, transport, package distribution: untouched.

## Status labels
- SELFTEST PROVEN: generic `actor.spawn` mutation (player-like, NPC-like, and a
  generic runtime actor), generic Transform/Velocity/health authority on the
  migrated path, hot CS countdown/spawn/full phase cycle, deterministic teams and
  spawns, no spawn/bomb ABI.
- COMPILED INTEGRATION: `actor.spawn` + `map.anchors` kernel capabilities;
  `MatchPhaseOwnership` bypass of cold `beginMatchCountdown` for CS; typed
  projections updated by the kernel.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game CS countdown/spawn/round rhythm, HUD, and
  the typed projection matching on real clients.

## Honest limits
- The server *simulation* still writes typed movement (`p.pos`) each tick and
  then projects to the generic Transform; generic Transform is authoritative for
  the spawn/reset path but the movement integrator is not yet generic. Full
  generic-authority simulation belongs to the transform/snapshot pass.
- The client still reads the typed snapshot fields (unchanged this pass).
- Live hot-edit/reload during a running CS round was not performed.

## Files changed
`src/hot-reload/game-api.h` (actor.spawn + anchors),
`src/live-code/live-behavior.cpp` (capability providers),
`src/network/server-context.h`, `src/network/server-gamemode.cpp`
(`serverSpawnOrResetActor`, spawn anchors),
`src/hot-reload/modules/gamemodes/counterstrike.cpp` (full phase/spawn ownership),
`src/network/counterstrike-selftest.cpp`; docs + this changelog.

## Next (auto-selected)
Hot snapshot/relevance policy (starting from the generic Transform/Velocity
authority), then interpolation policy, prediction/reconciliation, lag
compensation/rewind, multiplayer package READY/switch protocol.
