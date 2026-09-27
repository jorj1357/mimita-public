# Generic objective entities + objective events (cold bomb branch bypassable)

- EST timestamp: 2026-09-14 22:26:39 EDT (UTC 2026-09-15T02:26:39Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--objective-generic-selftest` 17/17 +
  full suite 21/21)

## 1. Current objective ownership audit (server-side)
| Stage | Owner before | Classification |
|---|---|---|
| objective creation / round reset | `beginObjectiveRound` + `Bomb` fields in `ServerGamemodeState` | objective policy (cold) |
| carrier / pickup / drop | `updateObjectiveBomb` (`BOMB_OBJ_CARRIED`/`DROPPED`) + typed ids | objective policy (cold) |
| plant/activate | `updateObjectiveBomb` + `bombSiteContains` (MapConfig) | objective policy (cold) |
| timer/progress | typed `objectiveBombTimer`/`PlantProgress`/`DefuseProgress` | objective policy (cold) |
| completion/failure | `checkObjectiveRoundEnd` -> roundWins/phase/RESULTS | objective policy (cold) |
| kill interaction | shared kill path; objective reads `matchTeams` | generic mechanism |
| round result / next round | `checkObjectiveRoundEnd` + shared phase machine | objective policy (cold) + mechanism |
| bomb-tag NPC chase flags | `updateBombTagNpcFlags` on typed `Npc` | compatibility (separate bomb-tag mode) |
| UI/network bridge | `broadcastBombTagState`/`BombTagStatePacket` | transport bridge (cold) |

Nothing was proven dead in this pass, so nothing was deleted.

## 2. Objective = entity + components + relationships
- Objective entity: `ObjectiveState { kind, state, ownerTeam, flags }` and
  `ObjectiveProgress { progress, goal, ratePerTick }` (package-private dynamic
  components). World-visible through a `TransformComponent`.
- Site entity: separate entity linked by `objective.at-site`.
- Carrier: `objective.carried-by` relationship (objective -> actor). Same
  objective `EntityId` survives pickup/drop.
- No `BombManager`, `CounterStrikeObjectiveManager`, or `ObjectiveType` enum.

## 3. Generic objective interaction fact
- `objective.interact` (`GameObjectiveInteractV1` in
  `src/network/objective-events.h`): actor entity, objective entity, site
  entity, action hash, tick, input flags. Dispatched by runtime event hash; no
  `plantBomb`/`defuseBomb`/`pickupBomb` event or ABI field.

## 4. Generic objective state-changed fact
- `objective.state-changed` (`GameObjectiveStateV1`) is emitted by hot objective
  code through the existing `emitEvent` capability and observed through generic
  event dispatch (the self-test records it on the objective entity).

## 5. First migration: generic carry-to-site behavior
- Hot runtime mode `objective.carry` owns: lazy objective + site creation,
  pickup/drop via `objective.carried-by`, progress while the carrier is within
  reach of the site, completion, and `match.finish`. Interactions enter through
  `objective.interact`; no kernel bomb branch runs while owned.

## 6. Carrier / drop / pickup
- Pickup adds the relationship; drop removes it. A destroyed/stale carrier edge
  is reconciled generically (no typed carried-object structure, no copy).

## 7. Site / target model
- The site is an entity with a Transform and an `objective.at-site` edge. No
  hardcoded site slots in the kernel (hot code creates/links it).

## 8. Timer / progress as state
- Progress/goal/rate live in `ObjectiveProgress`; the hot system decides when
  progress starts, interruption, threshold, and completion. Nothing is buried in
  `server-gamemode` switches.

## 9. Match integration
- `objective.state-changed` + component state are read by the objective system,
  which calls the generic `match.finish` (and can use `match.setPhase`). No
  TDM/CS branch in the objective code.

## 10. Replication
- Objective entities/components use generic entity lifecycle + dynamic
  component replication; `objective.carried-by`/`objective.at-site` use generic
  relationship replication. Schemas are package-registered. No objective packet;
  HUD is out of scope for this agent.

## 11. Runtime new-objective proof
- `--objective-generic-selftest` creates objective entities after startup,
  drives `objective.interact`, checks the relationship/state/progress changes,
  completion -> `match.finish` (RESULTS), the emitted state-change observation,
  stale-carrier cleanup, and that the schemas are registered for generic
  replication. No `ObjectiveType`, no new `game-api.h` field.

## 12. Multiple objective types
- A second independent behavior, `objective.hold` (hold-area/control point),
  is built from the SAME entity/component/relationship/event primitives with no
  extra kernel branch. Both are runtime modes registered by the package.

## 13. Round reset / cleanup
- The objective entity is reused and reset through generic component/
  relationship writes (the self-test drops/cleans the carrier edge); a fresh
  objective is created lazily per kind. No stale relationship survives (generic
  `eraseEntity` also drops edges when an actor/objective is destroyed).

## 14. Determinism
- Objective systems iterate entities in sorted `EntityId` order (hold-area) and
  use fixed per-tick progress increments; no unordered-container dependence.

## 15. Cold owner removal
- Not removed, only gated: a generic `ObjectiveOwnership` component on the match
  entity makes `serverGamemodeTick` skip `updateObjectiveBomb` +
  `checkObjectiveRoundEnd` and the bomb-tag broadcast. The cold bomb code is now
  compatibility fallback for modes that do not opt in.

## 16. Tests
`--objective-generic-selftest` PASS 17/17 (creation, progress, ownership,
site relationship, pickup/drop, state, completion, state-changed observation,
stale carrier, second behavior, replication registration).

## Status labels
- SELFTEST PROVEN: objective entity/component/relationship composition; generic
  interaction + state-change events; carrier pickup/drop; timer/progress;
  completion -> `match.finish`; second behavior on the same primitives; runtime
  objective modes; no objective enum/ABI.
- COMPILED INTEGRATION: `ObjectiveOwnership` gate in `serverGamemodeTick`;
  package schemas/relationships registered for generic replication.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game objective behavior/HUD and any actual
  objective mode selection.

## Honest limits
- The shipping CS `objective_rounds` bomb path is **not yet replaced**: no hot
  objective mode is wired to it, and a full migration needs map bomb sites as
  entities plus generic actor-position/timer inputs. This pass delivers the
  generic mechanism, the ownership gate, and two proven behaviors.
- Objective events are observation-only for now; the objective components remain
  the source of truth for mode logic.
- Bomb-tag NPC chase flags (`Npc.bombTag*`) remain cold and untouched.

## Files changed
`src/hot-reload/modules/gamemodes/objective.cpp` (new),
`src/network/objective-events.h` (new),
`src/network/objective-generic-selftest.{h,cpp}` (new),
`src/network/server-gamemode.cpp` (ObjectiveOwnership gate),
`src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`; docs + this changelog.

## Next (auto-selected)
Finish the objective/round genericization for the CS-like mode (sites as
entities, generic actor position/timer inputs, round reset hot-owned); then
transform/velocity generic state; then snapshot/relevance; prediction later.
