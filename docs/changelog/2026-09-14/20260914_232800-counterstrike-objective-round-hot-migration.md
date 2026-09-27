# Migrate the shipping CS-like objective/round path onto the generic objective architecture

- EST timestamp: 2026-09-14 23:28:00 EDT (UTC 2026-09-15T03:28:00Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--counterstrike-selftest` 17/17 +
  `--objective-generic-selftest` 17/17 + full suite 22/22, one unrelated
  presentation-agent check failing transiently — see below)

## 1. Shipping objective_rounds ownership audit
| Stage | Owner before | Classification | Now |
|---|---|---|---|
| round start trigger/phase timing | `serverGamemodeTick` + `beginObjectiveRound` | generic phase mechanism | kernel mechanism (still drives) |
| participant assignment | `assignMatchParticipants` | generic mechanism | kernel mechanism |
| objective creation | `beginObjectiveRound` typed `objectiveBomb*` fields | mode policy | **HOT** (`CsObjectiveState` entity) |
| carrier assignment | `objectivePickCarrier` + `d.objectiveBombCarrierId` | mode policy | **HOT** (sorted team-0 actor + `objective.carried-by`) |
| map/site lookup | `bombSiteContains` + `MapConfig.bombSites` | mode policy + map data | kernel `map.anchors` -> **HOT site entities** |
| pickup / drop | `updateObjectiveBomb` (`BOMB_OBJ_CARRIED/DROPPED`) | mode policy | **HOT** (`objective.interact`) |
| plant / plant timer | `updateObjectiveBomb` + `objectiveBombPlantProgress` | mode policy | **HOT** (`CsProgress.plant`) |
| planted state / timer | `BOMB_OBJ_PLANTED` + `objectiveBombTimer` | mode policy | **HOT** (`CsObjectiveState`/`CsProgress`) |
| defuse / defuse timer | `updateObjectiveBomb` + `objectiveBombDefuseProgress` | mode policy | **HOT** (`CsProgress.defuse`) |
| explosion | `updateObjectiveBomb` typed player kill loop | mode policy | **HOT** (generic `damage.apply`) |
| win conditions (objective/elim/timeout) | `checkObjectiveRoundEnd` | mode policy | **HOT** (`match.round-result`) |
| round result tally/phase/RESULTS | `checkObjectiveRoundEnd` + `d.roundWins` | generic round mechanism | kernel `match.round-result` |
| round reset | `beginObjectiveRound` typed reset | mode policy | **HOT** (`match.round-start` -> reset) |
| bomb-tag packet bridge | `broadcastBombTagState` | compatibility projection | compatibility fallback (skipped when owned) |
| bomb-tag NPC chase flags | `updateBombTagNpcFlags`/`Npc.bombTag*` | dead-for-CS compatibility | untouched |

Nothing proven dead was deleted; the cold objective code is now compatibility
fallback for modes that do not register a hot objective mode.

## 2. Real map sites as entities
- New generic `GAME_CAP_MAP_ANCHORS` + `GameMapAnchorV1`: the kernel projects map
  metadata (`MapConfig.bombSites`) as points + radius + kind
  (`gameHash("objective.site")`). No `siteA`/`siteB` slots.
- Hot mode creates one `CsSiteState` site entity per anchor with a Transform.
  Verified against `config/maps/dust2cyberiav3.json`.

## 3. Generic position query
- The hot mode reads the existing generic authoritative `TransformComponent`
  through `readComponent(GAME_COMPONENT_TRANSFORM)` for actors/sites; no
  `isPlayerAtBombSite()` / `getBombCarrierPosition()` was added. No transform
  networking change.

## 4. Objective entity
- One reused objective entity per match: `CsObjectiveState` + `CsProgress` +
  Transform. The same `EntityId` survives spawn/pickup/carry/drop/plant; reset
  reuses it (drops `objective.carried-by`/`objective.at-site` edges).

## 5. Generic interaction flow
- Pickup/drop arrive as `objective.interact` (`GameObjectiveInteractV1`). The
  cold server does not decide plant/defuse/pickup/drop; the hot mode does.
  Plant/defuse are automatic while the correct actor is alive and in range.

## 6-9. Plant / defuse / timer / win conditions
- Hot plant (3s, interrupted on leaving site), defuse (5s, interrupted on
  leaving range, defenders only), bomb timer (40s) -> explosion with generic
  `damage.apply`; objective/explosion defused -> defenders, exploded ->
  attackers, elimination (generic `actor.killed` dead markers), and no-plant
  timeout -> defenders. All call `match.round-result`.

## 10. Round reset
- `match.round-start` (new generic event, published by `beginObjectiveRound`)
  makes the hot mode reset objective state, clear carriage/site edges, reset
  timers, and clear dead markers for the next round. Verified no stale edges.

## 11-12. Team/role + respawn
- Teams are read from the generic `ActorTeamState` component (not typed
  ownership). Dead markers (`CsDead`) are package-private and cleared at round
  start; no mid-round respawn. The lifecycle policy still keeps
  `respawn_seconds = 0` for the mode.

## 13-15. Cold owner removal + no bomb ABI + replication
- For the shipping `counterstrike` mode, `applyActiveHotMode` activates the hot
  domain and the mode claims `ObjectiveOwnership`, so `updateObjectiveBomb` and
  `checkObjectiveRoundEnd` no longer run (compatibility fallback only).
- Added capabilities are generic (`match.round-result`, `map.anchors`); no
  `BombState`/`plantBomb`/`defuseBomb`/`BombSiteType`/`CounterStrikeManager`.
- Objective components/relationships replicate through the existing generic
  dynamic-component/relationship/entity-lifecycle paths; no bomb packet for the
  migrated path.

## 16-17. Tests
`--counterstrike-selftest` PASS 17/17: runtime mode registered, real map sites
project to anchors, objective/site entities, ownership claim, plant completes and
moves to at-site, defuse ends the round for defenders, clean round reset,
explosion -> attackers, timeout -> defenders, elimination -> defenders, wrong
team cannot plant, duplicate interaction safe, deterministic carrier assignment,
carrier death drops the objective, and old timers/edges cannot affect the next
round.

## 18. Live-runtime target
Not run: no live edit/reload during an actual CS round was performed. Not
claimed.

## Status labels
- SELFTEST PROVEN: hot CS round policy (carrier/plant/defuse/timer/explosion/
  elimination/timeout), generic map anchors -> site entities, generic
  `match.round-result`/`match.round-start`, `ObjectiveOwnership` bypass, round
  reset, falsification cases, no bomb ABI.
- COMPILED INTEGRATION: `applyActiveHotMode` routes `counterstrike` to the hot
  mode; `serverMapAnchors`/`serverMatchRecordRoundResult` wired as kernel
  capabilities; `beginObjectiveRound` publishes `match.round-start`.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game CS round feel, HUD/bomb rendering, real map
  geometry sites, and live hot-edit during a round.

## Honest limits
- The cold phase machine and `beginMatchCountdown` still drive round phase timing
  and player revive/respawn for CS; only the *objective/round policy* moved hot.
  Full lifecycle ownership needs a generic spawn/revive mechanism (transform/
  spawn slice).
- The typed `objectiveBomb*` fields and `broadcastBombTagState` packet remain as
  compatibility projections (the client may still read them); the presentation
  agent can migrate to the generic objective state later.
- `--hot-combat-selftest` has one failing assertion unrelated to this change
  ("untouched actor presentation retires with the entity", presentation-agent
  area); it is not caused by this migration and was left untouched.

## Files changed
`src/hot-reload/modules/gamemodes/counterstrike.cpp` (new),
`src/network/counterstrike-selftest.{h,cpp}` (new),
`src/hot-reload/game-api.h` (generic capabilities + payloads),
`src/live-code/live-behavior.cpp` (capability providers),
`src/network/server-context.h`, `src/network/server-gamemode.{h,cpp}`
(round-result, map anchors, round-start),
`src/network/match-lifecycle.h` (round-start payload),
`src/hot-reload/modules/gamemodes/objective.cpp` (interact handler domain-scoped),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Generic transform/velocity authoritative state (so hot modes can own spawn/revive
and drift/interp), then hot snapshot/relevance policy, then prediction/
reconciliation, then the multiplayer package READY/switch protocol.
