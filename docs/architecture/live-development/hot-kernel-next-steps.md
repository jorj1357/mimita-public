// 09 12 2026
/* purpose
* Track the remaining work for the Version 3 kernel/hot-gameplay migration.
* Record what is implemented and exactly what is next, in order.
* this file DOES NOT define gameplay formulas
* this file DOES NOT replace hot-kernel.md or the feature specification
*/

# Hot kernel migration: next steps

Status as of 2026-09-14 (generic dynamic lifecycle pass).
See `docs/architecture/live-development/hot-kernel.md` for the architecture.
See `docs/architecture/live-development/hot-cold-audit.md` for the current map.

## Round 66 (2026-09-15, real ECS Tool Entity resource continuity) — implemented

- `--tool-entity-continuity-selftest` PASS: creates a REAL actor `P` and tool `E`
  through `ctx->entityCreate`, establishes the REAL production edges
  (`relationship.owns-tool` + `actorStateEquipTool` → `ToolRefState` +
  `contains-item` + `equips-item`), and attaches the real `PresentationState`
  (`meshResourceId = HOT_MESH_ROCKET`) plus a gameplay component.
- GLB A→B is published ONLY through the canonical content path
  (`ContentArtifact`/`ArtifactCache` → kind validation → `publishContentArtifact`
  → `PresentationResourceProvider::apply`). A and B are distinct valid GLBs.
- The REAL hot render path runs each version (`hot.presentation-mesh` →
  `render.mesh` → `submitMesh`); a new cold hook
  `PresentationRender::entityMeshResourceId(E)` reports the logical id the
  production path resolved for that exact EntityId. It stays `HOT_MESH_ROCKET`
  while `handleOf` is A, then B.
- Continuity is asserted across A→B (and F→G): E/P EntityIds, owns/contains/
  equips edges, `ToolRefState` key, `meshResourceId`, and the gameplay value all
  unchanged — no recreation, respawn, re-equip, or inventory rebuild. Malformed C
  and a failing loader keep last-good B with the entity graph intact. Handle A is
  observed retired exactly at the A→B swap; the render path keeps only a logical
  id, so no raw handle outlives the swap (no explicit GPU fence for a hypothetical
  multi-threaded renderer — documented gap).
- Real F→G switch transaction with B active (F=1 → G=2) keeps E/P/edges/state and
  B, and the render path still resolves B for E; a valid D published after G
  replaces B on the same logical id and entity. Unknown `mesh.user.test-object`
  resolves with no cold enum/case; `mesh.does.not.exist` is a safe skipped draw.
- New cold test hooks only: `PresentationRender::debugCreateMesh`,
  `debugRetireMesh`, `entityMeshResourceId`. No new registry, packet, enum,
  rocket-specific reload path, or GC system.
- Honest boundary: HEADLESS/SELFTEST EVIDENCE, not a rendered frame.
- NEXT: real PNG consumer (`ui.menu.logo`), real WAV consumer
  (`audio.weapon.rocket.fire`), resource late-join current-state sync,
  cross-kind unresolved fallback; then the rocket multi-axis + runtime-unknown
  tool falsifications.

## Round 7 (2026-09-14, generic dynamic entity/component lifecycle) — implemented

- `GameplayContextV1` ABI v6 adds operation-generic capabilities:
  `entity.create`, `entity.destroy`, dynamic `component.remove`,
  `component.enumerate`, `component.typesOnEntity`, `component.schema`, and
  `relationship.add/remove/query`; all registered as kernel capability ids.
- `DynamicComponentStore` now owns schema versions, 64-bit-keyed migrations, an
  atomic `applySchemaUpdate` at activation, deterministic enumeration, and
  deterministic `serializeType`/`worldHash`.
- `EntityRegistry::createGeneric` allocates package entity ids in
  `EntityDomain::None`; `destroy` purges dynamic components and relationships.
- `--dynamic-lifecycle-selftest` covers the full lifecycle, migration, safe
  rejection, determinism, and destroy cleanup. New hot probe
  `src/hot-reload/modules/banana-component.cpp` exercises it live.
- Not done: dynamic-component replication, editor edit/copy-paste UI, and
  removal of the typed legacy structs.

## Round 8 (2026-09-14, gamemodes as runtime packages) — implemented

- `GamePackageDescriptorV1` gained a metadata-only `modes[]` array
  (`GameModeDescriptorV1`: id, display name, domain, match schema). `GenericRuntime`
  stores modes, exposes `hasMode`/`modeDomain`, and routes an *active mode domain*
  so a mode's systems and domain-scoped event handlers run only while active.
- `GameplayContextV1` ABI v7 adds generic authoritative match capabilities:
  `match.current`, `match.actorTeamRead`, `match.finish`, `match.setPhase`,
  `match.respawn`, `match.setTeam`. General match facts `actor.killed` and
  `match.evaluate` are emitted by the kernel via runtime event ids (no
  `GameEventType` growth).
- Real FFA scoring and the score-limit win live in
  `src/hot-reload/modules/gamemodes/ffa.cpp`; the kernel skips its cold FFA
  score/win branches when the handler sets `handled`. The legacy `DuelStatePacket`
  is fed from package dynamic state through the temporary `match.score.snapshot`
  bridge capability.
- `src/hot-reload/modules/gamemodes/hot-test.cpp` is a mode identity unknown at
  startup (`HotTestMatchState`), proving live mode creation.
- `--gamemode-hot-selftest` covers mode discovery, domain routing, domain-scoped
  events, hot scoring, the score bridge, `match.evaluate`, mode schema migration,
  and last-good preservation.
- Not done: timers/phase/respawn policy, TDM/duel/objective/wave migration,
  team/role relationships, mode-defined UI, and generic replication of package
  match state.

## Round 9 (2026-09-14, hot combat policy) — implemented

- Generic combat facts: `tool.primary-use`/`tool.alt-use` (one held use) and
  `projectile.impact` (world/actor/lifetime hit) with generic payloads in
  `game-api.h`; no `GameEventType` growth.
- `live-behavior` dispatches them to the generic runtime; the DLL-side
  `HotPackageBuilder` gained tool/projectile behavior tables and a router
  (`modules/tools/combat-policy.cpp`) that dispatches by runtime key and leaves
  unknown keys to the cold path.
- Real rocket impact policy is hot (`modules/tools/rocket-policy.cpp`), bypassing
  the cold `explodeOn*` flags when handled. A brand-new tool + projectile
  (`modules/tools/banana-launcher.cpp`) is created after startup and composes
  dynamic tool state, an owned tool entity, a relationship, and a fired fact.
- `--hot-combat-selftest` covers tool-use routing, projectile-impact routing,
  the new tool/projectile, package state persistence, and cold fallback.
- Not done: authoritative item spawn from a hot behavior (`projectile.spawn`),
  ammo/reload as generic state, a server-context damage capability, hitscan
  ownership, and melee contact detection.

## Round 10 (2026-09-14, generic entity + state replication) — implemented

- Generic replication envelope (`PACKET_DYNAMIC_COMPONENT`) carries schema
  descriptors, component upserts/removals, relationship edges, and
  **entity CREATE/DESTROY** records for any runtime type, selected by
  `networkPolicy` (`ALL`/`OWNER`/`NONE`/`SERVER_ONLY`), over the reliable event
  channel. No per-component/entity packet, struct, encoder, or decoder.
- `DynamicComponentStore`/`RelationshipStore` track generic change versions;
  the server sends only changes and diffs per client for removals.
- Client apply registers unknown schemas, migrates on version change, applies
  components/relationships, and processes lifecycle first: CREATE adopts the
  exact `EntityId` (`EntityRegistry::adopt`), DESTROY clears state and retires
  the id so stale records cannot resurrect it.
- `--dynamic-replication-selftest` PASS 23/23 (schema/component/relationship
  replication, migration, lifecycle create/destroy, duplicate/stale/id-reuse
  falsifications). The real `HotProjectileState` entity uses the generic
  lifecycle + component path.
- Not done: generic entity-lifecycle replication for players/NPCs/legacy
  projectiles (typed bridges remain), automatic marking of every created generic
  entity, presentation, and the live two-client run.

## Round 11 (2026-09-14, NPC generic health + lifecycle) — implemented

- `ActorHealthState` (dynamic component, `networkPolicy ALL`) is the
  authoritative health for NPC/monster-like entities; `ServerNpc.health` and the
  typed `HealthComponent` are projections. `finalizeServerNpcSpawn` initialises
  it and marks the NPC entity for generic lifecycle replication;
  `rebuildServerNpcMap` projects from it.
- `damage.apply` mutates the component generically and emits a generic
  `actor.killed` on the alive->dead transition; no NPC-specific callback.
- `EntityRegistry::destroy` feeds a generic destroyed-id log that emits DESTROY.
- `--npc-entity-selftest` PASS 11/11 (authority, damage, replication, lethal,
  destroy, stale rejection, runtime monster-like entity). Full suite 15/15.
- Not done: transform/AI/weapon slices, removing the cold `ServerNpc.health`
  write in the projectile explosion path, and the live two-client run.

## Round 12 (2026-09-14, NPC generic actor state) — implemented

- Generic authoritative `ActorTeamState`/`ActorRoleState`/`ActorProfileState`
  dynamic components + `relationship.targets`, written for every participant
  entity (player/NPC) at assignment and for NPC targeting; typed match/Npc
  fields are projections.
- Hot module `npc-ai-state.cpp` (`gameplay.60`) consumes the generic role/team/
  profile and derives `NpcAiSelection` — a real hot consumer of generic state.
- `--npc-actor-state-selftest` PASS 11/11 (authority, replication, hot
  consumption, stale rejection, monster-like composition). Full suite 16/16.
- Not done: `serverResolveActorSpawnProfile` still reads the typed role
  projection; NPC tool/equipment state remains typed; transform/velocity out of
  scope; no live two-client run.

## Round 13 (2026-09-14, authoritative gameplay.60 + hot NPC combat) — implemented

- `serverResolveActorSpawnProfile` resolves the role from the entity's
  `ActorRoleState` (typed descriptor role is a fallback projection).
- One `gameplay.60` domain execution per authoritative server fixed tick (already
  wired; documented order: context -> domain -> low-level sim -> replication).
- New hot `npc.combat-ai` (`gameplay.60`, priority 650): reads generic
  health/team/Transform, chooses a target, writes `relationship.targets`, and
  performs authoritative damage via `damage.apply` on a generic cooldown.
- Cold NPC target selection now follows `relationship.targets` for generic
  actors (orchestrator bypassed; nearest-enemy search is fallback).
- `--gameplay-boundary-selftest` PASS 8/8; full suite 17/17.
- Not done: the cold NPC weapon/attack owner is not yet bypassed; transform
  snapshot migration; live two-client run.

## Round 14 (2026-09-14, cold NPC weapon owner removed) — implemented

- Generic NPC tool ownership: NPC projectile weapons get a tool entity
  (`contains-item`/`equips-item` + `ToolRefState`); `Npc.hotToolOwned` gates
  `NpcCombat::tryFire` so there is exactly one attack owner.
- Hot `npc.combat-ai` emits the generic `tool.primary-use` action; the DLL
  router dispatches to the hot tool behavior, which spawns the canonical
  composition projectile. Tool-owned cooldown decides cadence.
- Kernel fixes: queued events dispatch with a capability context; generic schema
  registration survives a store clear.
- `--gameplay-boundary-selftest` PASS 11/11; full suite 17/17.
- Not done: hitscan/melee NPC attacks still cold (need hot behaviors + a kernel
  query for whether a hot behavior exists for a tool key); transform migration;
  live two-client run.

## Round 15 (2026-09-14, NPC hitscan/melee generic + handled gate) — implemented

- Hot `hitscan-tool`/`melee-tool` behaviors; all armed NPCs equip a tool entity;
  attacks flow through the one generic tool/action path with tool-owned cooldown.
- Generic action-handled gate: the hot action router records `ActorActionState`;
  `npc.cpp` bypasses cold `NpcCombat::tryFire` from that generic record.
  `Npc.hotToolOwned` deleted; no category query added.
- `--gameplay-boundary-selftest` PASS 15/15; full suite 17/17.
- Not done: match/team/objective/respawn genericization (next pass); transform
  migration; live two-client run.

## Round 16 (2026-09-14, hot match phase ownership + TDM) — implemented

- Generic phase-ownership gate: `MatchPhaseOwnership` on the match entity makes
  `serverGamemodeTick` skip its cold phase transitions.
- Hot `gamemode.tdm`: owns countdown->active, team scoring (`actor.killed`),
  score-limit finish (`match.finish`), win (`match.evaluate`), and respawn policy
  (`match.lifecycle`). No mode enum.
- `--match-policy-selftest` PASS 8/8; full suite 18/18.

## Round 17 (2026-09-14, hot participant assignment + full TDM lifecycle) — implemented

- Hot `gamemode.tdm` now owns participant team/role assignment (deterministic
  over sorted EntityIds; writes generic `ActorTeamState`/`ActorRoleState`) and
  the complete lifecycle: countdown -> active -> results -> intermission -> next
  round, with score limit, time limit, tie handling, win/end, and per-round
  score reset. It calls `match.setPhase`/`match.finish`; the cold phase machine
  stays bypassed via `MatchPhaseOwnership`.
- Generic team authority: `serverMatchActorTeam` reads the generic
  `ActorTeamState` component (source of truth) and `projectGenericActorTeams`
  projects it onto the typed `matchTeams`/participant roster for the
  scoreboard/broadcast. `serverMatchSetTeam` mirrors to the generic component.
  `serverMatchSetPhase` clears the match-over lock when a fresh round starts.
- No `match.assign-participants` slot was added: generic entity discovery
  (`ActorHealthState`) + dynamic components + `match.setTeam`/`match.setPhase`
  already express assignment, per the live-runtime generic-bootstrap rule.
- `--match-policy-selftest` PASS 14/14; full suite 20/20.

## Round 18 (2026-09-14, generic objective entities + events) — implemented

- An objective is an entity + package-private dynamic components
  (`ObjectiveState`, `ObjectiveProgress`) + relationships
  (`objective.carried-by`, `objective.at-site`). The kernel never knows "bomb"
  and no `ObjectiveType`/`BombManager`/ABI field was added.
- Generic facts: `objective.interact` (actor/objective/site/action/tick) and
  `objective.state-changed` (emitted by hot code via `emitEvent`). Two
  independent hot runtime modes (`objective.carry`, `objective.hold`) use the
  same entity/component/relationship/event primitives; completion calls
  `match.finish`. Generic replication carries the objective components,
  relationships, and entity lifecycle; no objective packet.
- Generic `ObjectiveOwnership` component on the match entity bypasses the cold
  `updateObjectiveBomb`/`checkObjectiveRoundEnd` policy for hot-owned objectives.
- `--objective-generic-selftest` PASS 17/17; full suite 21/21.

## Round 19 (2026-09-14, shipping CS-like objective round migrated hot) — implemented

- Generic kernel mechanisms added (no bomb ABI): `GAME_CAP_MATCH_ROUND_RESULT`
  (hot mode records a round winner; kernel tallies/transitions), `GAME_CAP_MAP_ANCHORS`
  (map metadata -> generic site anchors), `GAME_EVENT_MATCH_ROUND_START`
  (round lifecycle fact), and a generic `ObjectiveOwnership` bypass of the cold
  objective policy.
- Hot `gamemode.counterstrike.cpp` (runtime mode id `counterstrike`) owns real
  carrier/plant/defuse/bomb-timer/explosion, elimination/timeout/objective wins,
  no-mid-round-respawn dead markers, and round reset. Sites are entities created
  from generic anchors; positions come from the generic Transform component;
  `objective.interact` is the interaction fact.
- Shipping `objective_rounds` (`counterstrike.json`) now routes to the hot mode:
  `applyActiveHotMode(gameHash("counterstrike"))` activates its domain and it
  claims `ObjectiveOwnership`, so `updateObjectiveBomb`/`checkObjectiveRoundEnd`
  are compatibility fallback only.
- `--counterstrike-selftest` PASS 17/17; full suite 22/22 (one unrelated
  presentation-agent check fails transiently, see changelog).
- Not done: live hot-edit/reload during a CS round; live two-client run;
  transform/velocity generic state.

## Round 20 (2026-09-15, generic actor spawn/reset + full CS phase ownership) — implemented

- Generic `GAME_CAP_ACTOR_SPAWN` (`actor.spawn`) mutates the generic
  Transform/Velocity/health/dead state and keeps typed projections in sync; the
  mode decides where/when. `GAME_CAP_MAP_ANCHORS` also emits `spawn.team`
  anchors (team tag + yaw) from map metadata.
- Hot `gamemode.counterstrike.cpp` owns countdown timing, participant team
  assignment, round-start spawn/reset, and the full phase cycle
  (countdown/active/results/intermission/next round) via `match.setPhase`,
  claiming `MatchPhaseOwnership` + `ObjectiveOwnership`. The cold phase machine +
  `beginMatchCountdown`/`resetGamemodeActorsAtMapSpawn` are bypassed for CS
  (compatibility fallback for unmigrated modes).
- Transform/Velocity are now written as generic authoritative components on the
  migrated actor path; typed `ServerPlayer.pos/vel`/`ServerNpc.pos/vel` are
  projections (updated by `serverSpawnOrResetActor`).
- `--counterstrike-selftest` PASS 22/22; full suite 22/22.
- Not done: live hot-edit/reload during a CS round; live two-client run; the
  simulation still writes typed movement then projects to Transform (full
  generic-authority simulation is the transform snapshot pass).

## Round 21 (2026-09-15, hot snapshot/relevance policy) — implemented

- Generic `net.relevance` query (`network/relevance.h`): viewer position +
  candidate entities (generic Transform + `ReplicationPolicy` metadata). Hot
  `net-relevance.cpp` answers: always-relevant -> high tier; near (<= 30)
  -> every tick; far -> low tier at 1/6 cadence.
- `buildAndSendSnapshot` selects relevant entities per viewer via the hot policy
  (deterministic order: tier then entity id) and falls back to the previous
  broadcast when no policy handles. Transport/framing/socket unchanged.
- `--relevance-policy-selftest` PASS 9/9; full suite 23/23.
- Not done: generic Transform/Velocity authority for the movement integrator
  (player/NPC movement still writes typed first and projects to generic);
  per-viewer relevance needs live multiplayer verification.

## Round 22 (2026-09-15, server generic spatial authority bridge) — partial

- Generic `serverProjectActorSpatialFromGeneric`/`ToGeneric` added; Transform/
  Velocity are the persistent spatial store for the migrated actor path.
  `simulatePlayer` refreshes typed from generic at the top (honoring
  `actor.spawn`/teleport) and projects typed back on every return;
  `applyLiveActorBehavior` read-bridges NPCs; `beginAuthoritativeTransform`
  (join/respawn/teleport) writes generic immediately.
- Typed `ServerPlayer.pos/vel`, `ServerNpc.pos/vel`, `Npc.body.*` are
  projections of generic state.
- `--server-spatial-authority-selftest` PASS 9/9; full suite 24/24.
- Not done (success-bar gaps): the per-tick movement *integration* still runs on
  the typed working copy; rewind/history still samples typed broadcast positions;
  the server movement *algorithm* is not yet hot-owned (no algorithm hot-edit
  proof). Concurrent `MovementRuntimeStateComponent` phase 1 (local player) left
  untouched. These are the next movement steps.

## Round 23 (2026-09-15, hot air-acceleration algorithm + rewind-from-generic) — partial

- Real algorithm migrated hot: `applySourceAir` dispatches
  `movement.air-accelerate` (`hot-movement-policy.h`); hot `movement-air.cpp`
  owns the actual air-acceleration math (projected-speed diminishing gains,
  velocity modification), not constants. Generic numeric inputs -> reusable for
  server/prediction/any actor.
- `pushPositionHistory` (rewind/history) now samples the generic authoritative
  Transform/Velocity.
- `--movement-algorithm-selftest` PASS 6/6; full suite 25/25.
- Not done (success-bar gaps): the movement *integrator* still runs on the typed
  working copy (read-early/project-late); collision still consumes typed state;
  no live hot-edit proof; wire snapshot payload still typed (derives from
  generic). Behavior change from the hot air algorithm needs human verification.

## Round 24 (2026-09-15, generic collision boundary + shared-policy parity) — partial

- Collision mechanism extracted to `resolveCapsuleCollisionAgainstWorld(world,
  pos, vel, radius, height, onGround)` — no `ServerPlayer`; typed wrapper calls
  it. Cold physics still owns sweep/slide/penetration.
- The shared hot `movement.air-accelerate` policy is proven context-free
  (identical multi-tick results in two independent contexts) — the structural
  precondition for one implementation driving server + prediction.
- `--movement-algorithm-selftest` PASS 8/8; full suite 25/25.
- Not done / blocked: the local hot `movement.main` (`movement-system.cpp`, owned
  by the movement-state agent) still implements different movement math, so
  server/client parity (#6/#7) and the one-edit-changes-both proof (#8) are not
  established. The integrator still uses the typed working copy (#1/#2). Per the
  concurrency rule this boundary is documented rather than independently
  rewritten.

## Round 25 (2026-09-15, one shared air-acceleration implementation) — partial

- The air algorithm is now defined ONCE as `MimitaHotMovement::airAccelerate`
  (`movement-air.cpp`, declared in `hot-movement-policy.h`).
- `movement.main` (`movement-system.cpp`) now routes its airborne acceleration
  through that shared function (its separate inline blend formula for the air
  case is removed; ground/friction unchanged). The server hook handler calls the
  same function.
- `--movement-algorithm-selftest` PASS 9/9 including "event path == shared
  function"; full suite 25/25.
- Not done: a real server/client PATH parity selftest (driving `movement.main`
  through collision) and the live one-edit-changes-both observation. The
  single shared definition is structural; both call sites reference it.

## Round 26 (2026-09-15, real-path air-movement parity harness) — partial

- New `--air-movement-parity-selftest` drives BOTH real paths with identical
  initial state/input/dt and aligned air tuning over 120 airborne ticks:
  (A) the server sequence `applyPreCollisionBasicMovement` ->
  `applySpecialMovementPreCollision` -> `applyPostCollisionMovementWithSpecials`
  (the walk/air step calls the hot hook), and (B) the local prediction system
  `movement.main` via `GAME_DOMAIN_GAMEPLAY`.
- Result: both paths use the shared air function identically over the first 30
  ticks (maxDev 1.9e-6). **Full-sequence air parity is NOT achieved**: divergence
  begins at tick 84 and grows to maxDev 1.61 by tick 120 as the projected speed
  approaches the wish-speed cap — reported as a `[warn]`, not hidden.
- Full suite 26/26.
- Not done: full air parity (root-cause the late divergence), the live
  one-edit-changes-both observation, and live server/client proof.

## Round 27 (2026-09-15, hot ground-move ownership) — implemented

- One hot ground-move implementation `MimitaHotMovement::groundMove`
  (`movement-ground.cpp`) owns friction + acceleration along wishdir.
- Server `applySourceGround` dispatches `movement.ground-move`; local prediction
  `movement.main` ground branch calls the same function. Cold Source math remains
  as fallback when no hot handler is active.
- `--movement-algorithm-selftest` PASS 11/11 (adds ground friction+accel and
  friction-only checks); full suite 26/26.
- Deferred (recorded, not chased): the tick-84 late air divergence
  (maxDev 1.61); likely wish-speed derivation / speed-cap policy — next movement
  owner.

## Round 28 (2026-09-15, hot speed-policy derivation) — implemented

- One hot speed/wish-speed derivation `MimitaHotMovement::speedPolicy`
  (`movement-speed-policy.cpp`): size-scale factor, effective max speed (with
  fixed speed limit), and the air wish-speed projection cap.
- Server `sourceMaxSpeedValue` and the air `wishspd` derivation dispatch
  `movement.speed-policy`; local prediction `movement.main` derives its max speed
  through the same function. Cold math remains as fallback.
- `--movement-algorithm-selftest` PASS 15/15; full suite 26/26.
- Air-parity `[warn]` unchanged after the speed-policy move (maxDev 2.2699, first
  divergent tick 68) → the divergence is NOT the max-speed/wish-speed derivation;
  it is elsewhere (post-acceleration clamp or the client collision pipeline).
  Recorded, not chased.
- Next: jump, dash/down-dash, freeze, post-acceleration clamp, then generic
  integrator authority.

## Round 29 (2026-09-15, hot jump policy) — implemented

- One hot jump policy `MimitaHotMovement::jumpPolicy` (`movement-jump.cpp`):
  buffer, coyote, grounded/air jump eligibility, air-jump count/arm/lock, scaled
  impulse, and the runtime jump-state transitions.
- Server `applyBasicJump` dispatches `movement.jump` and applies the returned
  state; local prediction `movement.main` calls the same function. Cold logic
  remains as fallback. Persistent state stays in the existing generic
  `MovementRuntimeStateComponent` (client) / `MovementJumpState` (server working
  copy) — no new jump state store.
- `--movement-algorithm-selftest` PASS 17/17 (adds grounded jump, air jump +
  decrement, second-air-jump denial, determinism); full suite 26/26.
- Air-parity `[warn]` unchanged (maxDev 2.2699, tick 68) — jump did not affect the
  airborne divergence.
- Next: dash/down-dash, freeze, post-acceleration clamp, then generic integrator.

## Round 30 (2026-09-15, hot dash / down-dash policy) — implemented

- One hot dash policy `MimitaHotMovement::dashPolicy` (`movement-dash.cpp`):
  direction choice (move input + camera fallback), ground/air impulse
  composition, availability consumption, and the down-dash vertical response.
- Server `tryActivateDash`/`tryActivateDownDash` dispatch `movement.dash` and
  apply the result (server wrapper keeps momentum-protection/grace bookkeeping);
  local prediction `movement.main` calls the same function. Cold logic remains as
  fallback.
- `--movement-algorithm-selftest` PASS 22/22 (adds grounded dash, airborne dash,
  unavailable denial, camera-fallback direction, down-dash); full suite 26/26.
- Air-parity `[warn]` unchanged (maxDev 2.2699, tick 68) — dash did not affect the
  airborne divergence.
- Next: freeze, post-acceleration clamp, then generic integrator.
- Ops note: concurrent EXE/DLL relinks occasionally cause spurious
  selftest/parity failures mid-run (e.g. a tick-0 air divergence); re-running
  after the rebuild gives the stable result.

## Round 31 (2026-09-15, hot freeze + post-step speed clamp) — implemented

- `MimitaHotMovement::freezePolicy` (`movement-freeze.cpp`): activation
  eligibility, duration, velocity suppression while frozen, held/released
  transitions, exit. `updateFreeze` and `movement.main` both call it.
- `MimitaHotMovement::speedClamp` (`movement-speed-clamp.cpp`): the horizontal
  post-acceleration clamp / preservation rule. `applySpeedLimitClamp` calls it;
  the client path has no equivalent clamp (documented).
- Air-parity `[warn]` unchanged after both (maxDev 2.2699, tick 68) — the clamp
  did not remove the divergence (the air test runs with the clamp disabled), so
  the remaining difference is elsewhere (client collision pipeline / preservation
  in the shared air step). Recorded, not chased.
- `--movement-algorithm-selftest` PASS 28/28; full suite 26/26.
- Movement policy is now fully hot: air, ground, gravity, speed, jump, dash/
  down-dash, freeze, clamp. Remaining movement item: the typed working-copy
  integrator (direct generic Transform/Velocity authority), then networking/
  simulation policy (reconciliation/interpolation/rewind/delivery).

## Round 32 (2026-09-15, player collision on generic working state) — partial

- The server player Phase-2 collision no longer calls the typed
  `resolveWorldCollision(ServerPlayer&)`; it calls
  `resolveCapsuleCollisionAgainstWorld` on the GENERIC working movement state
  (`state.position`/`state.baseVelocity`), reports contact, and projects typed
  fields from it. The typed `ServerPlayer` is no longer the collision input.
- Still pending: the working `MovementState` is still populated from typed
  (`movementStateFromServerPlayer`) even though typed is refreshed from generic at
  the top of the tick; the full "read generic directly" working copy, the NPC/generic
  actor path, and the runtime-generic-actor proof remain.
- Air-parity `[warn]` unchanged (maxDev 2.2699, tick 68) — the air test does not
  call `simulatePlayer`, so the collision flip is not exercised there.
- Full suite 26/26.
- Next: finish the generic working-copy read (no typed population), NPC + runtime
  generic actor on the same substrate, then networking/simulation policy.

## Round 33 (2026-09-15, generic integrator authority: player direct + typeless proof) — partial

- Player movement now reads its authoritative position/velocity/yaw DIRECTLY from
  the entity's generic Transform/Velocity (typed `ServerPlayer` no longer the
  authoritative input); collision runs on the generic working state; the result
  is written DIRECTLY to generic Transform/Velocity (typed is a projection). The
  scope guard remains a compatibility failsafe.
- New `--generic-integrator-selftest`: a TYPELESS runtime entity (domain None,
  Entity + Transform + Velocity) runs the SAME movement pipeline and hot policies
  and integrates/deterministically repeats — proving the substrate is
  class-agnostic.
- Still typed: grounded/contact (`ServerPlayer.onGround`) and the jump/dash/freeze
  runtime working state; the NPC path is not yet on the same substrate.
- Air-parity `[warn]` unchanged (maxDev 2.2699, tick 68); the harness bypasses
  `simulatePlayer`.
- Full suite 27/27.
- Next: migrate grounded + runtime ability state generic, then one real NPC path;
  then reconciliation/interpolation/rewind/delivery.

## Round 34 (2026-09-15, generic grounded/contact authority) — partial

- The server player's grounded/contact state is now read from the generic
  `MovementRuntimeStateComponent` (ensured/seeded once per actor) instead of typed
  `ServerPlayer.onGround`; collision writes the generic component and typed
  `onGround` is a projection. Mutating typed `onGround` no longer authorizes
  movement.
- Reuses the kernel `MovementRuntimeStateComponent` (no new state store).
- Still typed: the jump/dash/freeze runtime working state (`player.movement`) is
  not yet read/written through the component; the real NPC path is not migrated.
- Air-parity `[warn]` unchanged; full suite 27/27.
- Ops note: a concurrent `live-behavior.cpp` edit broke the kernel build
  (`pushSurfaceDecal`/`spawnGenericSurfaceDecal` private) for a while; the suite
  showed 27 "NO-RESULT" until the build succeeded. Spurious `--gamemode-hot` /
  `--hot-combat` FAILs also occurred during a concurrent relink; re-running gave
  27/27.
- Next: jump/dash/freeze runtime state on the component, then one real NPC path;
  then reconciliation.

## Round 35 (2026-09-15, generic jump/dash runtime-state authority) — partial

- Server player jump/dash runtime state (grounded, airJumpsLeft, jump intent
  timer, jumpHeldPreviously, airJumpArmed, dashAvailable, downDashAvailable,
  dashGraceSeconds) is now read from and written to the generic
  `MovementRuntimeStateComponent`; typed `player.movement` is a mirror and can no
  longer re-authorize the next tick.
- Uses existing component fields (no ABI change, no second state store).
- Still typed/ephemeral: `coyoteTimerSeconds`, `airJumpLocked`, dash cooldown
  (server has none; client-only) and freeze active/available/timer (no component
  fields yet). The NPC path is not migrated.
- Full suite 27/27; air-parity `[warn]` unchanged.
- Discipline: I scoped this pass to the existing component fields (jump/dash) and
  did not extend the shared component or rewrite the NPC physics in the same
  patch; freeze fields + NPC remain the next slice.

## Round 36 (2026-09-15, movement completion gate + reconciliation audit) — blocked/documentation

- Re-audited remaining movement state. The gate is 13/15-ish: freeze state,
  coyote timer, and air-jump lock still lack generic fields; the real NPC path is
  not on the substrate.
- **Stopped at the concurrency boundary:** completing 6/7 requires extending
  `MovementRuntimeStateComponent` (ecs/components.h) + `GameMovementRuntimeStateComponentV1`
  (game-api.h) + the read/write mapping in `live-behavior.cpp`; completing 11
  requires the NPC physics path. A concurrent agent is actively editing exactly
  those files. No duplicate/parallel change was created.
- Added the movement completion-gate table and the reconciliation audit table to
  `hot-cold-audit.md` (the audit is analysis-only, no source edits).
- Next: when the concurrent agent's files settle, (a) extend the component +
  mapping with freeze/coyote/airJumpLocked (schema-versioned defaults), (b)
  migrate one real NPC path, (c) declare movement complete, (d) start real
  reconciliation hot-policy migration.

## Round 37 (2026-09-15, hot reconciliation policy) — implemented (new lane)

- New lane per concurrency rule: did NOT touch the movement agent's
  `game-api.h` movement component, `live-behavior.cpp` mapping, or
  `movement-system.cpp`.
- New `hot-reconciliation.h` (`GameReconcileV1` + `net.reconcile`) and
  `modules/reconcile-policy.cpp`: hot error metric + thresholds (ignore/smooth/
  medium/snap) and a conservative **generation-mismatch -> hard reset** rule.
- Real shipping path wired: `mpReconcileLocalPlayer` dispatches `net.reconcile`
  and maps the hot `correctionMode` back to `MovementCorrectionClass` (cold
  `classifyMovementCorrection` remains the fallback). Cold still applies the
  correction.
- `--reconciliation-policy-selftest` PASS 7/7; suite 28 tests, 27 PASS.
- Unrelated concurrent failure recorded (do not fix): `--hot-combat-selftest`
  fails on "real explosion shake reaches the hot camera policy" / "hot
  camera-effect command works" — the effects/camera agent's active files.
- Next: interpolation policy (hot delay/sample/lerp/stale), then rewind policy,
  then distributed generation delivery.

## Round 38 (2026-09-15, hot interpolation policy) — implemented

- New `hot-interpolation.h` (`GameInterpolateV1` + `net.interpolate`) and
  `modules/interpolate-policy.cpp`: owns interpolation alpha (clamp/override) and
  a generation-boundary hard-snap rule.
- Real shipping path wired: `buildReceiveTimeRender`
  (multiplayer-interpolation.cpp) dispatches `net.interpolate` after computing
  alpha; on generation mismatch it hard-snaps to the newer sample. Cold still
  stores samples and performs the numeric mix.
- `--interpolation-policy-selftest` PASS 6/6; full suite 29/29 (the concurrent
  camera/effects failure is resolved).
- Next: extend interpolation policy (delay re-targeting, extrapolation decision,
  stale handling), then rewind/lag-comp policy, then distributed generation.
- Note: generation ids are structurally carried but currently 0; extend policy
  once generations are populated.

## Round 39 (2026-09-15, complete interpolation-policy ownership) — implemented

- Extended `net.interpolate` to own: generation/lifecycle discontinuity snap,
  packet-gap snap (>= 30 ticks), buffer-dry hold-vs-extrapolate, the
  extrapolation cap, delay override (`outDelaySeconds`), and alpha. One dispatch
  per entity/frame at the top of `buildReceiveTimeRender`; the cold path consults
  the hot mode and keeps sample storage + numeric mix/extrapolation.
- Removed the previous second (normal-branch) dispatch; exactly one policy owner.
- `--interpolation-policy-selftest` PASS 10/10; full suite 29/29.
- Interpolation policy is now hot enough except: the delay policy is a
  pass-through placeholder (adaptive-delay measurement remains cold), and
  generation ids are still 0.
- Next: rewind / lag-comp policy, then distributed generation delivery + real
  generation ids.

## Round 40 (2026-09-15, adaptive interpolation-delay ownership) — implemented

- `outDelaySeconds` is now a real hot policy result: `net.interpolate` owns the
  adaptive delay (min/max bounds, jitter response, loss/starvation response,
  increase/decrease convergence rate). Cold remains the measurement source
  (`estimatedArrivalJitterMs`, `recentLossFraction`, effective base delay) and
  stores the returned delay; when hot handles, cold convergence does not run.
- Extension: `net.interpolate` handles a `delayQuery` mode (adaptive delay)
  alongside the existing interpolation mode. Fallback unchanged.
- `--interpolation-policy-selftest` PASS 15/15 (adds healthy/jitter/loss/
  recovery/min-bound adaptive-delay cases); full suite 29/29.
- INTERPOLATION CATEGORY COMPLETE (policy): alpha, generation/lifecycle snap,
  packet-gap, buffer-dry, extrapolation allow/deny+cap, stale/hold/snap, adaptive
  delay. Cold retains storage/clock/decode/measurement/numeric application.
  Generation ids remain 0 (distributed-generation gap, not interpolation).
- Rewind audit started: `estimateServerRewindTick` + `getPlayerPoseAtTick`/
  `getNpcPoseAtTick` (server-attack.cpp) are the next cold policy owner.
- Next: rewind/lag-comp policy, then distributed generation delivery.

## Round 41 (2026-09-15, hot rewind / lag-comp policy) — implemented

- New `hot-rewind.h` (`GameRewindPolicyV1` + `net.rewind`) and
  `modules/rewind-policy.cpp`: owns the rewind target tick (latency +
  interpolation-delay compensation), the max-rewind clamp, and a conservative
  generation-mismatch reject. Cold executes the historical lookup.
- Real shipping path wired: `estimateServerRewindTick` (server-players.cpp)
  dispatches `net.rewind`; it feeds both hitscan (server-attack.cpp) and the
  projectile fire-view tick (server-projectiles.cpp). Cold math is fallback.
- `--rewind-policy-selftest` PASS 6/6; full suite 30/30.
- Still cold/absent: the generic `historicalState(EntityId, T)` (player/NPC
  `getPlayerPoseAtTick`/`getNpcPoseAtTick` remain parallel), and explosion
  victim-pose rewind (known gap; substrate is prepared but not integrated).
- Next: distributed generation delivery (logical generation id/hash + platform
  artifact hash + READY/SWITCH), then optional generic historical-state +
  explosion rewind.

## Round 42 (2026-09-15, distributed generation bookkeeping) — partial

- Audited the EXISTING pipeline: `HotReloadSystem` (source watch → local build →
  candidate → safe-tick activation/rollback) and `CodeGenerationPacket`
  (generation, direction, phase status/READY/SWITCH, switchTick, codeHash,
  logicalCodeHash, platformPackageHash). The logical-vs-platform split already
  exists in the packet.
- Added the missing host bookkeeping: `GenerationDistribution`
  (`generation-distribution.{h,cpp}`): per-peer state machine
  (Unknown/Announced/Acquiring/Validating/Ready/Active/Failed), quorum over a
  required-peer set for a logical generation, Active-counts-as-ready, multiple-
  candidate supersede, peer removal, and switch scheduling/cancel. Mechanism
  only (no build/load/transport).
- `GenerationIdentityV1` keeps `logicalGenerationId`/`logicalBehaviorHash`
  separate from `platformArtifactHash` + ABI.
- Wired real generation ids into reconciliation:
  `predictedGeneration = local activeGeneration`,
  `authoritativeGeneration = ctx.serverCodeGeneration`. Interpolation/rewind
  still pass 0 (they need per-sample generation storage — next).
- `--generation-distribution-selftest` PASS 10/10; full suite 31/31.
- NOT done: artifact acquisition over the network (content-addressed cache +
  transfer), the ACQUIRE/VERIFY peer flow, per-sample generation tagging in
  interpolation/rewind, and the real two-process server/client proof.
  MILESTONE NOT COMPLETE.
- Next: artifact acquisition + READY wire-up on the real client path, then the
  server+client proof; optional generic `historicalState(EntityId,T)`.

## Round 43 (2026-09-15, content-addressed artifact acquisition) — partial

- New `artifact-cache.{h,cpp}`: content-addressed **immutable** cache
  (`<root>/<hash>.bin`, dedupe, never overwrite) + `hashArtifactBytes` +
  `ArtifactAcquirer` receive/verify state machine (Idle/CheckingCache/Requesting/
  Receiving/Verifying/Complete/Failed). ACQUIRE is separate from ACTIVATE:
  committing an artifact never activates a generation.
- `--artifact-cache-selftest` PASS 13/13 (verify, immutable dedupe, no-overwrite,
  read-back, wrong-hash reject, acquirer transitions, mismatch failure,
  cache-hit short-circuit); full suite 32/32.
- Still missing (documented): network transfer/chunking over the wire, wiring
  VERIFY->READY into the real client path, authoritative tick-domain mapping
  proof, per-sample generation tags in interpolation/rewind, late join, and the
  live two-process proof.
- Audit (live Creation): hot glob re-resolution already supports add/delete/
  rename of hot .cpp under the globbed dirs; `CommandRegistrar` is hot; content is
  C++-first (JSON optional). Asset hot-upload, shared/multi-user source editing,
  and ChangeSets are missing.
- Next: wire ACQUIRE/VERIFY/READY into the real client path + tick mapping, then
  the two-process proof; then asset/resource hot-upload + ChangeSet model.

## Round 44 (2026-09-15, chunked artifact wire transfer) — partial

- New packet types/structs: `PACKET_ARTIFACT_REQUEST`/`BEGIN`/`CHUNK`
  (`ArtifactRequestPacket`, `ArtifactBeginPacket`, `ArtifactChunkPacket`,
  bounded 1000-byte chunks).
- New `artifact-transfer.{h,cpp}`: `ArtifactStreamer` (server chunks a verified
  artifact) and `ArtifactReceiver` (client indexed, duplicate-safe, order-
  tolerant reassembly; commits only when all chunks present and hashes; feeds
  `ArtifactAcquirer`/`ArtifactCache`). Acquire stays separate from activate.
- `--artifact-transfer-selftest` PASS 13/13 (in-order, duplicate, out-of-order,
  missing, wrong size/hash, disconnect, invalid begin, cache commit); full suite
  33/33.
- NOT wired into the live client/server packet handlers yet (no
  REQUEST→BEGIN→CHUNK send/receive in `multiplayer-tick.cpp`/
  `server-packet-handlers.cpp`), and no tick-domain mapping or two-process proof.
- Next: wire the live handler path + READY, audit/prove the tick domain, run the
  two-process proof.

## Round 45 (2026-09-15, live artifact wire path) — compiled integration

- Server now serves `PACKET_ARTIFACT_REQUEST` by streaming the ready candidate
  artifact (`HotReloadSystem::readCandidateArtifact` → `ArtifactStreamer`) as
  BEGIN + CHUNKs; only the matching content hash is served (no client-supplied
  path).
- Server announce advertises the REAL platform artifact hash
  (`readCandidateArtifact` FNV hash) and registers each peer in
  `GenerationDistribution`.
- Client: on announce, requests the artifact by hash if not cached; reassembles
  BEGIN/CHUNK via `ArtifactReceiver`; on complete commits (verify+immutable store)
  and sends `READY(G)` (phase=1 with the exact logical generation).
- Server: on phase=1 READY, records per-peer readiness for the exact announced
  generation (`GenerationDistribution::setPhase` only when the candidate id
  matches).
- Full suite 33/33; cold build SUCCESS.
- TICK DOMAIN AUDIT (unresolved): the server schedules `switchTick = serverTick+30`
  in SERVER tick space; the client applies it against its local
  `clientSimulationTick` (`pollAndAdvance`) and has NOT been proven to equal the
  authoritative tick. This must be mapped explicitly before a live switch.
- Not proven live: no two-process run. Interpolation/rewind generation ids still 0;
  late join and superseded-transfer cancellation not wired.
- Next: explicit tick-domain mapping + two-process proof.

## Round 46 (2026-09-15, tick-domain mapping + quorum-gated switch) — compiled integration

- Explicit tick-domain mapping: `generation-switch-mapping.h`
  `mapServerSwitchTickToClientLocal(latestServerTick, localSimTickAtReceipt,
  serverSwitchTick)` — DELTA-based (not equality). The client maps the
  authoritative `SWITCH(G,T)` into its own local simulation boundary using the
  announce's server tick and its local tick at receipt. Wired into
  `multiplayer-tick.cpp` (replaces the raw `switchTick` comparison). It also
  refuses to schedule a SWITCH for a generation it has not validated.
- Quorum-gated switch (`server.cpp`): the server now (1) announces the candidate
  (phase 0) with the real artifact hash + registers per-peer identity, holding
  its OWN activation with a far-future switch tick; (2) only after
  `GenerationDistribution::quorumReady(required, G)` schedules the shared
  `SWITCH(G, tick+30)`. No SWITCH before quorum.
- `--generation-switch-mapping-selftest` PASS 6/6; full suite 34/34.
- TICK DOMAIN AUDIT: server `tick` (authoritative fixed step) vs client
  `clientSimulationTick` (local prediction counter) are NOT proven identical; no
  explicit offset existed. The delta mapping now defines the relationship.
- Still pending: socket-level integration test, live two-process proof,
  interpolation/rewind provenance (ids still 0), peer ABI/capability/schema/
  dependency verify gate, late join, superseded-transaction rule, ACTIVE ack.
- Next: socket integration test + provenance, then two-process proof.

## Round 47 (2026-09-15, generation provenance scaffolding + committed-switch rule) — partial

- Canonical provenance: added `logicalGenerationId` to `SnapshotTransform`
  (interpolation samples) and fed `net.interpolate` `aGeneration`/`bGeneration`/
  `currentGeneration` from the samples (replacing hardcoded 0). Cross-boundary
  pairs now trigger the hot snap policy **when samples are stamped**.
- Committed-switch supersede rule enforced in `server.cpp`: pre-commit a newer
  candidate may supersede the announced one; once a SWITCH is committed
  (`switchCommitted`), H does not cancel G — G activates, then H is the next
  candidate. Flags reset on activation.
- Full suite 34/34; cold build SUCCESS.
- STILL NOT POPULATED: samples are not yet stamped with the server's **active**
  generation per snapshot (needs a per-snapshot wire `generation` field so
  receive-time guessing is avoided). Rewind history and predicted-state
  provenance are not tagged. So provenance is structurally wired but not yet real
  end to end.
- Next: add the per-snapshot generation field + stamp on receipt; tag rewind
  history + predicted state; then the socket integration test and two-process
  proof.

## Round 48 (2026-09-15, authoritative snapshot generation provenance) — implemented

- The authoritative snapshot wire now carries the canonical logical generation:
  `SnapshotChunkPacket.logicalGenerationId` (wire size 1124→1128), set in
  `buildSnapshotChunks` from `HotReloadSystem::status().activeGeneration` (the
  generation that actually executed the tick — activation happens at the safe
  boundary before simulation, so T<switch → F, T>=switch → G).
- `reassembleSnapshotChunks` verifies all chunks agree (`mixed-generations`
  rejected) and returns the generation; the client threads it through
  `processSnapshotEntities` → `pushInterpolationTarget` and stamps
  `SnapshotTransform.logicalGenerationId`.
- `buildReceiveTimeRender` already feeds `net.interpolate` a/b/current generation
  from the samples, so a cross-boundary pair now snaps instead of lerping — real
  provenance, no receive-time inference.
- Full suite 34/34 minus one unrelated concurrent UI failure (see below).
- NOT done: rewind/history sample tagging, predicted-state per-sample tagging,
  socket integration test, verify gate, late join, live proof.
- Next: tag rewind history + predicted state; then the socket integration test.
- Concurrent: `--hot-combat-selftest` fails "hot ui.frame fails safe without match
  HUD state" (UI/HUD agent's active work) — recorded, not touched.

## Round 49 (2026-09-15, prediction + rewind generation provenance) — implemented

- Rewind history provenance: `PositionHistoryEntry` gained
  `logicalGenerationId`, stamped at insertion in `pushPositionHistory` from
  `HotReloadSystem::status().activeGeneration` (the generation that produced that
  authoritative tick). `getPositionAtTick`/`getPlayerPoseAtTick` now CLAMP to the
  newer side when the two bracketing samples straddle an F/G boundary instead of
  interpolating across generations. `net.rewind` receives real
  attacker/target/current generation ids (previously 0).
- Prediction provenance: the client does not store per-tick predicted history (it
  reconciles by snapping to the authoritative snapshot), so there is no predicted
  state to tag retroactively; reconciliation already compares the authoritative
  snapshot generation (stamped) against the local active generation. Documented,
  not invented.
- NOT done: NPC rewind-history provenance (`getNpcPoseAtTick`), socket
  integration test, verify gate, late join, live proof.
- Suite 34/34 minus the unrelated concurrent `--hot-combat-selftest` UI failure.
- Next: NPC rewind provenance, then the socket integration test.

## Round 50 (2026-09-15, NPC rewind provenance + provenance complete-enough) — implemented

- NPC rewind history provenance: `ServerNpcPositionSample.logicalGenerationId`,
  stamped at insertion in `pushNpcPositionHistory` from the active generation;
  both NPC pose lookups clamp to the newer sample across an F/G boundary (same
  semantics as player rewind).
- Generation provenance is now complete-enough: authoritative snapshots,
  interpolation samples, player rewind history, NPC rewind history, and the
  rewind policy inputs all carry the canonical logical generation id; cross-
  generation interpolation/rewind clamp/snap instead of mixing. Prediction has no
  per-tick history by design (reconcile snaps to the stamped authoritative
  snapshot) — documented, not invented.
- Full suite 34/34.
- Next (primary): the actual transport-level end-to-end generation test
  (encode/send/receive/decode for ANNOUNCE/REQUEST/BEGIN/CHUNK/READY/SWITCH +
  snapshot generation), then the verify gate, late join, and two-process proof.
- LIVE-PROOF DEBT: unchanged; no live socket transaction observed yet.

## Round 51 (2026-09-15, transport-level generation transaction test) — implemented

- New `--transport-generation-selftest`: serializes the REAL packet structs and
  sends them over an actual OS loopback UDP socket (sendto/recvfrom), decodes the
  received bytes back into packets, and drives the real `ArtifactStreamer`/
  `ArtifactReceiver`/`ArtifactCache`:
  - ANNOUNCE, ARTIFACT_REQUEST, ARTIFACT_BEGIN, ARTIFACT_CHUNKs, READY, SWITCH
    all cross the real transport;
  - 3000 artifact bytes reassemble and hash-verify and commit to the immutable
    cache; cache-hit verifies with zero chunk bytes;
  - SWITCH tick mapping is exercised in the transport path (server tick 1000,
    client local 1004, T=1030 -> mapped 1034, not 1030);
  - snapshot generation crosses the transport (`parseSnapshotChunk` returns the
    stamped generation).
  - Reported byte accounting: artifact=3000, chunks=3, transportBytes=3200.
- `--transport-generation-selftest` PASS 12/12; full suite 35/35.
- Honest scope: this is a packet/transport integration over a loopback socket
  using the real structs/streamer/receiver; it does NOT boot the full game
  server/client loop. Verify gate, late join, and the two-process proof remain.
- Next: verify gate (ABI/capability/schema/dependency), late join, then a
  full-loop/two-process proof; then distributed assets.
- LIVE-PROOF DEBT: full-loop/two-process not observed.

## Round 52 (2026-09-15, generation verify gate + ABI wire) — implemented

- New `generation-verify.h`: `GenerationManifestV1` +
  `GenerationLocalFactsV1` + `verifyGeneration()` returning an explicit
  `VerifyFailure` reason (HashMismatch / LogicalGenerationMismatch / AbiMismatch /
  MissingCapability / SchemaMismatch / DependencyMissing / LoadFailed /
  MigrationFailed). Checks artifact hash + size, logical generation association,
  hot ABI, required capabilities, required schemas, and required dependencies via
  generic probes (no feature-specific tables).
- ABI crossed the wire: `CodeGenerationPacket.hotAbiVersion` set by the server
  announce; the client stores `ctx.serverHotAbiVersion` and now sends READY
  (phase=1) **only** when the local build is loaded AND the server's hot ABI is
  compatible with `MIMITA_GAME_API_VERSION`. Incompatible ABI → never READY.
- `--generation-verify-selftest` PASS 8/8 (valid + each failure reason);
  full suite 36/36.
- NOT done: schema/dependency requirement population on the real wire (the gate
  is ready but the announce carries only ABI + hashes today), migration
  preparation, socket-level quorum with 2 clients, late join, full-loop/two-
  process proof.
- NEXT CATEGORY (planned): distributed resource/asset generations reusing the
  content-addressed cache + chunk transfer + verify + last-good.
- NEXT LARGEST COLD OWNER: populate schema/dependency requirements on the wire and
  wire `verifyGeneration` into the client READY path; then late join; then the
  full-loop proof; then assets.

## Round 53 (2026-09-15, real manifest population) — implemented

- The verify manifest is now populated from REAL package registration, not only
  synthetic test data: `GenericRuntime::capabilityRequirementAt`/`schemaAt`
  expose per-entry metadata, and the verify selftest builds a
  `GenerationManifestV1` from the live package's capability requirements and
  registered schemas, then verifies it against the live registry
  (`GenericRuntime::hasCapability` + `DynamicComponentStore::schema`). This proves
  the manifest describes what the package actually requires.
- `--generation-verify-selftest` PASS 12/12; full suite 36/36.
- Still missing: carrying the populated requirement arrays over the wire, wiring
  `verifyGeneration` into the client READY path, migration preparation,
  multi-peer quorum over real transport, late join, and the full-loop/two-process
  proof.
- NEXT CATEGORY (queued): distributed resource/asset generations reusing the
  content-addressed cache + chunk transfer + verify + last-good, with a
  `ContentArtifactV1 { logicalResourceId, resourceKind, contentHash, byteSize }`.

## Round 54 (2026-09-15, real manifest over the wire + verify-gated READY) — implemented

- The server now transmits the REAL manifest: `HotReloadSystem::buildCandidateManifest`
  builds a `GenerationManifestV1` (identity + ABI + the package's declared
  capability requirements + registered schemas), and `server.cpp` sends it as a
  bounded `GenerationManifestPacket` (type 81) alongside the announce.
- The client stores it keyed by exact logical generation (`pendingManifest`,
  `pendingManifestGeneration`) and runs `verifyGeneration` on the received
  manifest with REAL local facts (kernel ABI, `GenericRuntime::hasCapability`,
  `DynamicComponentStore::schema`, dependency probe). READY is sent only when the
  hash verifies AND `verifyGeneration == None`. Hash validity alone no longer
  produces READY.
- Bounded + versioned: explicit `GENERATION_MANIFEST_VERSION`, capped arrays
  (`GENERATION_MANIFEST_MAX_REQUIREMENTS = 8`), counts rejected on decode.
- `--transport-generation-selftest` now crosses the socket with the manifest
  arrays and proves: verify -> READY, and (artifact valid but) MissingCapability /
  AbiMismatch / SchemaMismatch / HashMismatch -> no READY. PASS (19 checks).
- Concurrent, unrelated: a UI agent's in-progress "hot navigation state" change
  makes `--hot-combat-selftest` fail its nav-migration check (not caused by this
  work; that selftest file is modified in the working tree by that agent).
- Still missing: migration preparation, multi-peer quorum over real transport,
  late join, full-loop transaction, two-process/raw-cpp proof.

## Round 55 (2026-09-15, migration preparation + multi-peer quorum transport) — implemented

- **Migration preparation is real and gates READY**: `hot-reload/migration-prep.h`
  (`prepareMigration(F,G,facts)` → `NoMigrationRequired | Prepared(plan) | Failed`).
  It reuses the EXISTING `DynamicComponentStore` schema/version + migration
  substrate: it compares the candidate's declared schema versions against LIVE
  stored versions (`maxStoredVersion`) and resolves whether a registered path
  exists (`hasMigration`). Plans store only LOGICAL identities (schema id +
  from/to version), never function pointers, so they cannot dangle across an
  unload; commit resolves against the still-registered migrations atomically via
  the existing `applySchemaUpdate`.
- Manifest now carries `requiredSchemaVersions[]` (from `GameComponentSchemaDescriptorV1.version`
  via `GenericRuntime::schemaAt`), so the peer prepares against the candidate's
  declared target versions.
- Client READY path: hash → manifest verify → `verifyGeneration` → `prepareMigration`
  against the ACTIVE generation F → READY only if verify passes AND prepare is not
  `Failed`. Server likewise gates its own switch on a local prepare.
- REAL multi-peer quorum over sockets (server + A + B) in
  `--transport-generation-selftest`: A READY + B not → no quorum, no SWITCH; B
  READY → quorum true → SWITCH scheduled; disconnect after commit does not cancel
  G; disconnect before commit recomputes quorum over survivors.
- `--migration-prep-selftest` PASS 14/14; transport PASS 21; full suite 37 tests,
  only the concurrent UI-agent `--hot-combat-selftest` (hot navigation) fails.
- Still missing: late join, full game-loop transaction, live/two-process proof.

## Round 56 (2026-09-15, plan explicitly in the switch transaction) — implemented

- `hot-reload/switch-transaction.h`: `validateSwitchTransaction` — a missing,
  stale, or invalid plan is an explicit rejection (`PlanMissing` / `PlanStale` /
  `PlanInvalid` / `NoCandidate`). A no-op plan is still validated for its exact
  F->G identity; only an initial load (no active source) needs no plan.
- `HotReloadSystem::tryActivateCandidate` now runs the switch transaction before
  any registration/state commit: it registers the candidate's dynamic migrations
  (additive), builds the authoritative local F->G plan from the loaded candidate's
  declared schemas vs LIVE stored versions, cross-checks any peer-supplied plan
  (`setCandidateMigrationPlan`), and rejects the switch on mismatch. Only then does
  `GenericRuntime::activate` apply the atomic schema update and the generation get
  published.
- Atomicity: `applySchemaUpdate` stages migrated blobs and returns false WITHOUT
  mutation on failure, and it runs before any registration commit — so G can never
  execute against F state and F survives a failed commit.
- `--switch-transaction-selftest` PASS 11/11 (validation matrix, T-1/T/T+1 atomic
  migration+publication, stale/missing rejection with no mutation, no-op switch).
  Full suite 38/38.
- Still missing: late join, full game-loop transaction, live/two-process proof.

## Round 57 (2026-09-15, late-join generation bootstrap) — implemented

- `hot-reload/generation-bootstrap.h`: `GenerationBootstrapV1` state machine
  (Idle/AwaitingMetadata/Acquiring/Verifying/Ready/Failed). Bootstrap means "become
  locally ACTIVE on the generation the server is ALREADY running" — NOT the READY
  path (no fake SWITCH for a late joiner). Stale packets/generations cannot advance
  or complete a bootstrap; `onServerActiveChanged` re-targets and a late completion
  for the old target cannot activate.
- Server: on join-accept it advertises the ACTIVE generation
  (`CODE_GENERATION_PHASE_ACTIVE_BOOTSTRAP = 3`) + the ACTIVE manifest
  (`HotReloadSystem::buildActiveManifest`). Client enters bootstrap, acquires the
  artifact by hash, verifies the manifest against real local facts, and only then
  can participate.
- Client gating: `processSnapshotEntities` ignores world snapshots and the input
  sender blocks while bootstrap is active/failed; participation requires
  `state == Ready && localActive == serverActive`. A fresh joiner runs no F->G
  migration (load/activate G then sync G world).
- `--generation-bootstrap-selftest` PASS 16/16; real transport late join (cache
  miss transfers bytes; cache hit transfers zero chunks and still verifies) PASS.
  Full suite 39/39.
- Honest gap: the client does not yet install the downloaded artifact into
  `HotReloadSystem` (so `codeLoaded` is only true once that exists) — the bootstrap
  gates and server advertisement are real; the artifact-install step and the full
  production-loop transaction remain.
- Next: artifact install into the loader, then the full production-loop F->G
  transaction; then distributed resources.

## Round 58 (2026-09-15, remote artifact install into the real loader) — implemented

- `HotReloadSystem::installCandidateArtifact(bytes, logicalGeneration, hash, err)`:
  hash-gates the bytes, stages an immutable generation-specific DLL copy, and
  loads it through the SAME `loadCandidateFromFile` path as a local build
  (API/ABI + self-test) as a real INACTIVE candidate. No remote-only loader; the
  candidate representation is identical (module, api, package descriptor) so the
  switch transaction, migration plan, and activation all apply unchanged.
- `pollAndAdvance` coordinated-switch branch now activates either a locally built
  `candidateReady_` candidate or a `remoteCandidateInstalled_` downloaded one at
  the agreed tick. The immediate path is unchanged, so a remote candidate is never
  activated before its switch boundary.
- Client wiring: after artifact commit, the verified bytes are installed into the
  loader; late-join bootstrap `codeLoaded` now comes from the REAL install
  (`hasInstalledCandidate()`), and a coordinated READY additionally requires the
  install to have succeeded.
- `--artifact-install-selftest` PASS: the actual 16.8 MB `build/mimita-game.dll`
  installs as a real inactive candidate via the loader; a tampered artifact is
  rejected before load. Full suite 40/40.
- Still missing: full production-loop F->G with same-session/entity assertions;
  raw-cpp/two-process proof. Then distributed content (`ContentArtifactV1`).

## Round 59 (2026-09-15, production-loop F->G + primitive cold-boundary audit)

- `--production-loop-selftest` PASS: real `HotReloadSystem` startup (F active),
  real artifact bytes (build/mimita-game.dll), real manifest + `verifyGeneration`,
  real `prepareMigration` against live state, real `installCandidateArtifact`,
  real coordinated switch + `tryActivateCandidate`, F=1 -> G=2, and the persistent
  dynamic component on its EntityId SURVIVED the generation change. This is the
  in-process production-loop proof (no handler shortcuts).
- Still no two-process interactive raw-cpp run (live-proof debt).
- Primitive audit (first pass): capabilities are 20 and mechanism-shaped
  (physics.move, effect.spawn, projectile.spawn, damage.apply, render.mesh/ui,
  audio.play, socket.query, setting.get/set, world.project, resource.register,
  match.round-result, map.anchors, actor.spawn, ...). No `rocket.fire` /
  `bomb.defuse` / `zombie.spawn` capability.
- Cold closed-world candidates found: `combat/weapon-types.h`
  (`WeaponBehaviorType`/`WeaponExecutionType`/`WeaponFireMode`) still used by cold
  `weapon-system.cpp`, `server-attack.cpp`, `npc-combat.cpp`, `weapon-data.cpp`;
  `npc/npc-goal.h` (`NpcGoalKind`) used by cold `npc-navigator.cpp`/`npc.cpp`.
  Whether these are still authoritative (vs legacy projection) is the next audit
  question and the largest likely cold owner.
- ArtifactCache/transport: opaque content-hash bytes; no code-specific transport
  assumptions observed (DLL vs PNG/GLB/WAV agnostic).
- Next: `ContentArtifactV1` + PNG live publication + malformed last-good; then GLB,
  WAV; then trace the weapon/NPC enums to decide whether they are real cold owners.

## Round 60 (2026-09-15, generic content artifact primitive) — implemented

- `hot-reload/content-artifact.h`: `ContentArtifactV1 { logicalResourceId,
  resourceKind, contentHash, byteSize }`, `ResourceVersionState { activeHash,
  candidateHash, lastGoodHash }`, `ResourceRegistry` (`announceCandidate`,
  `publishCandidate` / `publishCandidateFromCache`, `resolve`), and low-level
  structural validators for Code (MZ), PNG (signature+IHDR), GLB (glTF v2), WAV
  (RIFF/WAVE). `resourceKind` selects only the validator; no gameplay semantics.
- Reuses the EXISTING `ArtifactCache` for immutable bytes: no new transfer, no
  Texture/Mesh/Audio transfer stacks. The existing `ArtifactRequestPacket` /
  `ArtifactBegin` / `ArtifactChunk` already carry an opaque content hash, so the
  descriptor rides the same path (logical id in `logicalGenerationId`).
- Publication is atomic from `resolve()`'s perspective; a superseded or malformed
  candidate cannot overwrite the active mapping; last-good is preserved.
- `--content-resource-selftest` PASS 17/17: PNG/GLB/WAV live publish A->B
  (last-good A), cache-hit publish with zero chunks, malformed candidate keeps
  last-good, superseded candidate rejected, stable logical identity, generic kind
  validator selection.
- Next: carry the descriptor over the real socket and publish a real in-world GLB
  on the existing Tool Entity; then the rocket multi-axis + runtime-unknown-tool
  falsifications; then trace the cold weapon/NPC enums.

## Round 65 (2026-09-15, tool-entity continuity trace) — audit

- Traced the real tool-entity production path: hot tools create their tool entity
  via `ctx->entityCreate(host, realm, &outEntity)` and own it via
  `ctx->relationshipAdd(host, gameHash("relationship.owns-tool"), userEntity,
  toolEntity, toolKind)` (`modules/tools/banana-launcher.cpp:55-61`); the rocket
  sets `present.meshResourceId = HOT_MESH_ROCKET` (`rocket-tool.cpp:90`).
- The real-ECS Tool Entity continuity proof (same EntityId/owner/equip across
  A->B, production `handleOf` for that exact entity, malformed C, F->G with the
  entity attached) was NOT completed this pass.
- NEXT: a headless selftest that creates a real Tool Entity + `owns-tool`
  relationship through the generic capability context and runs the already-proven
  canonical content path against it.

## Round 64 (2026-09-15, real GLB consumer path) — implemented

- `--glb-consumer-selftest` PASS: the REAL logical mesh id
  `HOT_MESH_ROCKET = gameHash("mesh.rocket")` publishes A then B through the
  canonical content path (`publishContentArtifact`), and the SAME production
  consumer call the renderer uses
  (`PresentationResourceProvider::handleOf(HOT_MESH_ROCKET)`) resolves B with a NEW
  handle while the logical id is unchanged. Malformed bytes keep last-good B in the
  consumer path; a failing loader (runtime preparation failure) also keeps B; a
  post-code-swap publish D works on the same logical id.
- Honest gaps: a real ECS Tool Entity (with `HOT_PRESENTATION_COMPONENT`
  `meshResourceId`) owner/equip continuity is NOT yet asserted (no live object
  graph this pass); retirement/fence audit not done; F->G with the resource active
  not run (provider is a cold singleton with no DLL pointer, so the risk is low but
  unproven); unknown `mesh.user.test-object` and unresolved fallback not run.
- NEXT: assert an actual Tool Entity's continuity (same EntityId/owner/equip
  across A->B), retirement audit, then PNG/WAV consumers through the same bridge,
  then resource late join.

## Round 63 (2026-09-15, ONE resource authority) — implemented

- Unified the two overlapping resource-version systems onto a single authority:
  `MimitaRuntime::PresentationResourceProvider` (`project/presentation-resource.h`)
  is now the ONLY owner of the active logical mapping + prepared handle +
  last-good + retirement (it was already the real render path's resolver).
- `hot-reload/content-artifact.h` `ResourceRegistry` SHRANK to transport/acquisition
  metadata only (announceCandidate, pendingLogicalIdForHash, candidateHashOf,
  descriptorOf, acknowledgePublished); its authoritative `activeHash`/`lastGoodHash`/
  `versions_`/`publishCandidate` state was removed. `ResourceRegistry::resolve` is
  now a thin read of the provider (one truth).
- New canonical bridge `publishContentArtifact(...)` / `publishContentArtifactFromCache(...)`:
  supersede check (advertised candidate) -> hash verify -> kind validator ->
  `PresentationResourceProvider::apply(logicalId, contentHash)`. The provider's
  `apply` is a no-op for the same hash and keeps the previous generation on loader
  failure (last-good), and retires the old handle on success.
- `--content-resource-selftest` PASS 20/20: PNG/GLB/WAV publish A->B land in the
  provider; `resolve` reads the provider; cache hit; malformed keeps provider
  last-good; superseded candidate rejected; "one canonical authority" assertion.
- Transport content section updated to the bridge + provider; PASS.
- Not yet proven: live in-world Tool Entity GLB swap with same EntityId/equip
  (production path already resolves through `handleOf`, so the seam now exists),
  retirement safety audit, F->G-with-resource-active, unknown logical resource,
  unresolved fallback.
- NEXT: prove the live GLB swap on an equipped Tool Entity + entity/equip
  continuity + malformed last-good in the real render path; then PNG/WAV consumers
  through the SAME bridge; then resource late join.

## Round 62 (2026-09-15, tool render-path trace + registry collision) — audit

- Traced the real tool render path: `PresentationState.meshResourceId` is the
  authoritative LOGICAL identity; the draw path resolves it at use via
  `PresentationResourceProvider::instance().handleOf(meshResourceId)`
  (`presentation-render.cpp:631,660,690`; `presentation-entities.cpp:318`). The
  required invariant (logical id kept, handle resolved at use) largely already
  holds.
- **COLLISION FOUND:** `MimitaRuntime::PresentationResourceProvider`
  (`project/presentation-resource.h`) is ALREADY the generic logical-resource
  resolver (logicalId -> contentHash -> opaque handle + loader/retire + apply).
  My Round 60/61 `MimitaRuntime::ResourceRegistry` (`hot-reload/content-artifact.h`)
  DUPLICATES it. Must UNIFY: route `ContentArtifactV1` publication through
  `PresentationResourceProvider::apply(logicalId, contentHash)`; keep one owner.
- GLB in-world consumer NOT proven this pass; no runtime change.
- NEXT (immediate): unify the resource owner, then prove the live GLB swap on an
  equipped Tool Entity (same EntityId/equip), malformed last-good in the real
  render path, F->G with the resource active, and unknown `mesh.user.test-object`.

## Round 61 (2026-09-15, content descriptor on the real wire) — implemented

- `ContentArtifactPacket` (PACKET_CONTENT_ARTIFACT = 82), bounded: logical
  resource id + kind + byteSize + contentHash + monotonic token. Distinct from
  `CodeGenerationPacket` (no ABI/capabilities/schemas/coordinated switch).
- Client routing: on descriptor receipt, `ResourceRegistry::announceCandidate`;
  cache hit -> validate/publish immediately; cache miss -> request via the
  EXISTING `ArtifactRequest`/`Begin`/`Chunk` path (logical id in
  `logicalGenerationId`). Completed bytes for a pending logical resource route to
  `publishCandidateFromCache` instead of the code loader
  (`pendingLogicalIdForHash`).
- Transport selftest: descriptor crosses a real OS loopback socket; content cache
  miss transfers bytes -> validate -> publish; cache hit publishes with zero
  chunks; a superseded descriptor cannot overwrite the newer content version.
  (All 5 content-wire checks PASS; `--transport-generation-selftest` PASS.)
- Still missing (next): real in-world consumers (Tool Entity GLB swap with same
  EntityId/equip, live UI PNG, live WAV), renderer per-frame logical re-resolution
  audit, resource late-join current-mapping manifest, resource fallback when
  unresolved, rocket multi-axis + unknown-tool falsifications, weapon/NPC enum
  classification.

## Round 10 (2026-09-14, generic runtime state replication) — implemented

- One opaque envelope (`PACKET_DYNAMIC_COMPONENT`, `dynamic-replication.*`) carries
  schema descriptors, component upserts/removes, and relationship add/remove
  records for ANY dynamic component or relationship type. No per-component
  packet, struct, encoder, or decoder, and no new `GameplayContextV1` field.
- `DynamicComponentStore` already owned schema versions, migrations, and generic
  change markers. `RelationshipStore` now owns per-edge `changeVersion` and a
  per-type network policy: a relationship type unknown at startup becomes
  replicable the first time an edge is added, so no cold registration is needed.
- `serverReplicateDynamicComponents` sends per-client diffs of components AND
  relationships, detects removals by diffing its sent set, honors `GAME_NET_OWNER`
  by source entity, and batches across packets so truncation never drops a change.
  Wired in both the dedicated and listen/host server ticks.
- `--dynamic-replication-selftest` covers schema distribution, component and
  relationship add/update/remove, payload-size rejection, and v1->v2 migration
  with last-good preservation. `HotProjectileStateV1` is `GAME_NET_ALL` and is
  proven to flow through the generic path.
- Not done: generic `ENTITY_CREATE`/`ENTITY_DESTROY` replication (only component
  and relationship records replicate today, applied to the client store); stale
  component-record rejection on the client; two-client live network proof;
  client rendering of replicated projectile state; and migrating the remaining
  typed player/NPC/projectile snapshot structs onto the generic substrate.

## Architecture-first migration rule (current phase)

During the current migration phase, prioritize ownership transfer over behavior
polish. A subsystem counts as **migrated** when:

1. its ordinary behavior owner is hot;
2. state is generic/persistent;
3. cold code provides only low-level mechanism;
4. a running generation can replace its algorithm;
5. failure keeps last-good behavior;
6. no concept-specific kernel ABI was added.

Visual/feel parity can be improved afterward. Do not polish animation/blending
before ownership has moved.

## Round 64 (2026-09-15, NPC spawn audio hot + audio-leak classification) — source implemented

- NPC spawn audio: `npc-spawn.cpp` now emits the generic `effect.actor.sound` fact
  (key "actor.spawn") for both spawn paths; hot maps it to `audio.play`
  (`npc_spawn`), cold `AudioManager::play` is fallback. (NPC weapon fire was
  already hot; NPC dash hot in Round 62.) All NPC one-shot selection is now hot.
- Owner position-follow: audited; no real shipping persistent spatial NPC loop
  was found (spawn is one-shot; music is streamed separately). Per the mission,
  NOT implemented; recorded as a future generic extension.
- Music: `MusicManager` streams tracks (menu random / ingame playlist) via
  miniaudio — a real policy leak but tied to a streaming mechanism the current
  `audio.play` (cached one-shot) does not express. Recorded; not migrated (would
  need streaming in the generic slot path).
- Ambient/interaction: audited; no separate cold ambient loop owner or
  interaction-sound manager found beyond the existing one-shot paths. Recorded.
- Weapon fire sound: confirmed hot (no cold per-weapon sound branch).
- Proof: `--hot-combat-selftest` PASS incl. "NPC spawn audio policy is hot
  (actor.spawn -> audio.play)" + the Round 63 slot tests; full suite PASS.

## Round 63 (2026-09-15, generic audio slot lifecycle mechanism) — source implemented

- ABI: `GameAudioCommandV1` gained `ownerEntity`, `slotId`, `op`
  (`GameAudioOp`: PLAY_ONESHOT/SET_SLOT/STOP_SLOT) and `loop`. Slots are opaque
  logical ids (hash); no enum.
- Cold mechanism (`capAudioPlay`): a voice registry keyed by (ownerEntity,
  slotId); SET_SLOT is idempotent (same desired sound = no restart), a changed
  sound replaces the voice, STOP_SLOT is a safe no-op when absent, and slots
  whose owner entity dies are terminated. Physical voices use `AudioManager`
  owner ids; no handle crosses the hot boundary. `AudioEvent.loop` added
  (`ma_sound_set_looping`).
- Proof (`--hot-combat-selftest` PASS): SET_SLOT starts a persistent voice;
  identical SET is idempotent; A->B replaces; runtime-unknown slot + sound
  accepted; STOP of a nonexistent slot is a safe no-op. Full suite PASS.
- NOT YET MIGRATED (mechanism exists): hot NPC-owner loops, music/ambient policy
  (global owner 0 slots). Position-follow for owner loops is a recorded
  limitation (position fixed at SET).

## Round 62 (2026-09-15, Help classification + NPC action audio) — source implemented

- Help: audited; `help-menu.cpp` is a 49-line generic JSON-layout renderer
  (`config/gui/help-menu.json`) already hot-reloadable through `GuiLayoutManager`
  and containing no feature policy. Classified LOW-VALUE DEBT (D): migrating it
  would duplicate a generic renderer, so it was NOT migrated. Pause/global Help
  share the same renderer.
- NPC action audio: `npc.cpp` dash sound now emits a generic
  `effect.actor.sound` fact (logical key "actor.dash" + position); hot
  `hot.effect-composition` maps the key to `audio.play` (cold `playWorldSound` is
  fallback). NPC weapon-fire audio already routed through the hot
  `effect.weapon.fire.sound` seam.
- MECHANISM GAP (recorded): NPC/`AudioManager` sounds with owner/loop semantics
  (e.g. `npc_spawn`, music) cannot be expressed by `audio.play` (fire-and-forget);
  they need owner/loop fields or reuse of the AudioManager owner path. AudioManager
  is used by 1 proxy audio.cpp + npc.cpp/npc-spawn.cpp.
- Proof: `--hot-combat-selftest` PASS incl. "NPC action audio policy is hot
  (actor.dash -> audio.play)"; full suite PASS.
- UI ARCHITECTURE: declared COMPLETE ENOUGH (see audit). Remaining cold UI is
  secure/resource/narrow debt.

## Round 61 (2026-09-15, pause Settings/ConfirmLeave + first UI-audio batch) — source implemented

- Pause Settings: `pause.settings` now routes to the existing hot settings screen
  with a generic return target (`HotUiNavigationStateV1.previousScreenId =
  screen.pause`); `menu.back` returns to pause when the previous screen is pause.
  No duplicate pause-settings implementation.
- ConfirmLeave: hot pause composes the confirm modal (`pause.confirm-leave` view:
  "Leave this game?" + CONFIRM/CANCEL) and claims the screen; actions
  `pause.leave.confirm`/`pause.leave.cancel` route to the cold mechanism
  (leaveRoom / view Main). Cold Help view stays cold.
- First UI-audio batch (hot policy): `ui.action` CLICK maps element ids to
  logical sound ids (`audio.ui.back`, `audio.ui.confirm`, `audio.ui.click`) played
  through `audio.play`. Cold keeps decode/mix/device. Falsification: the mapping
  is hot C++ (change the id without an EXE rebuild).
- Proof: `--hot-combat-selftest` PASS incl. "hot UI-sound policy plays through
  audio.play" and "pause Settings routes to hot settings with return"; full suite
  PASS.
- Recorded: global Help, replay browser, avatar creator, login composition,
  notification/consent/music overlays classified but not migrated.

## Round 60 (2026-09-15, real input wiring + browser ownership flip) — source implemented

- Real keyboard wiring: the cold GLFW char/key callbacks (`main-init.cpp`) route
  to `LiveUi::handleTextChar/handleTextBackspace/handleTextSubmit` while a hot
  text field is focused, before any cold text widget; cold keeps OS/unicode/
  clipboard/repeat. No server-browser keyboard polling.
- Connect glue: `consumeHotUiPendingAction` maps `serverbrowser.connect` /
  `serverbrowser.join-code` (bounded value = room code) to the existing
  `gPendingConnect` room-code connection path (no second join implementation).
  Stale/invalid codes fail in that path (hot state advisory).
- Ownership FLIP: `menu.play` now routes to `screen.server-browser` (hot owns the
  browser by default); cold `drawOnlineMenu` yields via `hotOwnsScreen` and is the
  fallback. Host is not part of the shipping online menu (only a host-name column),
  so no host form was required/invented.
- Navigation: BACK -> main menu; REFRESH -> cold refresh.
- Proof: `--hot-combat-selftest` PASS incl. PLAY routes to the hot browser and
  text input; full suite PASS.
- Remaining: pause Settings/Help/ConfirmLeave; help; then audio policy.

## Round 59 (2026-09-15, generic text input + join-by-code) — source implemented

- New generic widget `GAME_UI_TEXT_INPUT` + action types TEXT_INPUT/TEXT_SUBMIT.
  The backend tracks only the focused element id and reports keystrokes as
  generic `ui.action` events (TEXT_INPUT with codepoint, 0 = backspace;
  TEXT_SUBMIT on enter). It never owns the text value.
- Hot-owned migratable text state: `HOT_UI_TEXT_COMPONENT` / `HotUiTextStateV1`
  (elementId + bounded text[32]) on the local actor entity. Hot updates it from
  the events and re-emits the widget; bounded by HOT_UI_TEXT_MAX (no overflow).
- Hot server browser now emits a join-by-code field + JOIN CODE button; submit
  routes `serverbrowser.join-code` (bounded value) through the generic pending
  action. Masked flag supported; secrets stay cold (this pass is non-secret).
- Proof: `--hot-combat-selftest` PASS incl. "generic text input updates
  hot-owned bounded text state" (focus -> type AB -> state == "AB"); full suite
  PASS.
- STILL PENDING (recorded): wiring the real cold char/key callback to
  `LiveUi::handleTextChar/Backspace/Submit`; host flow; the `serverbrowser.connect`/
  `join-code` cold connect glue; flipping the browser claim to default. Cold
  online menu remains the shipping owner.

## Round 58 (2026-09-15, generic server listings + hot server browser) — source implemented

- Generic listing representation (entity/component projection, per the mission's
  preferred option): cold `PresentationEntities::projectServerListings()` projects
  each discovered `ServerBrowserEntry` as a presentation entity carrying
  `HOT_SERVER_LISTING_COMPONENT` / `HotServerListingV1` (opaque listingId =
  gameHash(code), players/max/ping/flags + name/map/mode/code). Removed listings
  have the component cleared (no stale rows/pointers).
- Hot `hot.server-browser` composes rows + JOIN per listing + REFRESH + BACK,
  sorts by hot policy (ping asc, unreachable last), and claims
  `screen.server-browser`. Cold `drawOnlineMenu` yields when hot owns; discovery
  still ticks cold so listings keep flowing.
- Connect bridge: a listingId ui.action resolves back to the cold room code and
  is routed through the generic pending action (`HotUiPendingActionV1` gained a
  bounded `value[32]`); REFRESH -> cold `serverBrowserRequestRefresh()`.
- Runtime-unknown listing proof: a listing entity with only generic facts appears
  in the hot browser and the claim is owned (selftest PASS).
- OWNERSHIP FLIP PENDING (recorded): the cold online menu also ships host/hosting
  and join-by-code (needs a generic text-input primitive) so hot does not yet
  default-claim the screen; cold remains the shipping owner until those are
  covered. Not invented/removed.
- Proof: full suite PASS.

## Round 57 (2026-09-15, hot pause Main view) — source implemented

- Audit: `gui/menus/pause-menu.cpp` (namespace `PauseMenu`) owns the Esc modal
  with views Main/ConfirmLeave/Settings/Help. Main has resume/settings/help/
  discord/invite/leave; Settings reuses the cold `drawSettingsMenu`, Help reuses
  `drawHelpMenu`, Leave uses a cold confirm modal.
- Generic bridge: `HOT_PAUSE_STATE_COMPONENT` / `HotPauseStateV1`
  (viewHash + visible), written from the cold modal in `engine-tick-ui.cpp`.
- Hot `hot.pause-menu` composes the Main view (RESUME/SETTINGS/HELP/DISCORD/
  INVITE/LEAVE) and claims `screen.pause`; cold `PauseMenu::render` yields via
  `hotOwnsScreen("screen.pause")`. Per-view ownership: hot claims only when
  viewHash == pause.main, so Settings/Help/ConfirmLeave stay cold (no feature
  loss).
- Actions: pause.* ui.action ids -> `HotUiPendingAction`; the in-game UI tick
  consumes them and calls `PauseMenu::requestAction` (cold modal mechanism:
  close/set view/leave/discord/invite). No callback pointers.
- Generation safety: pause visibility lives in a dynamic component; a generation
  that stops claiming falls back to the cold view (no trapped state).
- Proof: `--hot-combat-selftest` PASS (pause bridge + Main widgets + claim);
  full suite PASS.
- Recorded next: server browser; then remaining UI re-audit; then audio policy.

## Round 56 (2026-09-15, discrete settings SELECT + resolution/preset) — source implemented

- New generic widget `GAME_UI_SELECT`: displays the current option label; click
  emits `VALUE_CHANGED` with the next option index. Hot owns option ids/labels;
  the backend only reports an index (generation-safe; no cached pointers).
- `GameSettingType::GAME_SETTING_OPTION` + `GameSettingV1.optionCount/optionLabel`:
  the kernel provides the valid option list; hot sets by index; the kernel
  validates/clamps and applies.
- `setting.get/set` now covers `video.resolution` and `video.graphicsPreset`
  (kernel lists: resolutions 1280x960/1600x900/1920x1080, presets Low/Medium/High).
- Hot `hot.settings-screen` emits SELECT rows for both; `ui.action VALUE_CHANGED`
  routes the index to `setting.set`.
- Settings now hot for all user-facing values: fov, master/music/sfx volume,
  sensitivity, mute, resolution, graphicsPreset.
- Proof: `--hot-combat-selftest` PASS incl. "discrete option setting get/set
  works (kernel list)"; full suite PASS.
- Recorded next: pause menu; server browser; then audio policy.

## Round 55 (2026-09-15, scoreboard show/hide bridge) — source implemented

- Real Tab scoreboard found: `mpContext.showPlayerList` (`engine-tick-net.cpp:821`,
  Tab hold) rendered by the cold tab player list (`engine-tick-ui-overlays`).
- Generic visibility state: `HOT_SCOREBOARD_VISIBLE_COMPONENT` /
  `HotScoreboardVisibleV1`; cold `PresentationEntities::projectScoreboardVisible()`
  bridges the physical Tab hold into it.
- Hot `hot.scoreboard` now composes only while visible AND real generic rows
  exist, claiming `screen.scoreboard`; the cold tab player list yields via
  `hotOwnsScreen("screen.scoreboard")`. Hold semantics preserved (no toggle).
- With Round 54's real per-actor stats, the hot scoreboard is now the shipping
  owner while Tab is held; cold remains the fallback when stats are absent.
- Proof: `--hot-combat-selftest` PASS (visibility-gated scoreboard + claim); full
  suite PASS.
- Recorded next: discrete settings SELECT (resolution/preset); pause menu; server
  browser; then audio.

## Round 54 (2026-09-15, real per-actor stats wire bridge) — source implemented

- Authoritative source: server `ServerGamemodeState::ffaKills/ffaDeaths` +
  `matchTeams` (per actor). The client previously received only FFA top-3 and TDM
  team kills.
- Wire: the existing match-state packet already carried `participantIds/Teams/
  Roles/States`; added `participantKills/Deaths/Scores/Names[32]` (minimal
  extension of the participant array). Server fills them from the authoritative
  mode counters + `ServerPlayer.name`.
- Client: `ReplicatedActorIdentity` now stores kills/deaths/score/name.
- Projection: `PresentationEntities::projectMatchStats()` writes generic
  `ActorIdentityState` + `ActorTeamState` + `ActorMatchStatsState` onto the SAME
  actor EntityId used by overlays/presentation (no shadow id), called from the
  match HUD tick.
- Result: the hot `hot.scoreboard` now receives real shipping per-actor stats, so
  its claim (already written whenever it composes rows) becomes the default
  owner; cold `MatchLeaderboard` yields via `hotOwnsScreen("screen.scoreboard")`.
- Proof: full suite PASS; the runtime-unknown actor hot-scoreboard test still
  passes. Live network appearance is debt.
- Deferred: discrete settings SELECT (resolution/preset); scoreboard show/hide
  generic action (cold Tab input not yet surfaced).

## Round 53 (2026-09-15, generic actor match-stats + hot scoreboard) — source implemented

- Generic `HOT_ACTOR_STATS_COMPONENT` / `HotActorMatchStatsV1` (score/rank/flags)
  — actor match FACTS, joined by EntityId with `ActorIdentityState` +
  `ActorTeamState`. No scoreboard-only row type.
- Hot `hot.scoreboard` (ui.frame, order 14): enumerates actors with generic
  stats, sorts by score desc, groups/colours by team, highlights the local actor
  via flags, emits rows with repeated `render.ui` (no table ABI), and claims
  `screen.scoreboard`.
- Cold `MatchLeaderboard::render()` yields when `hotOwnsScreen(screen.scoreboard)`.
- Claim is written only when the hot scoreboard actually composed rows, so the
  cold leaderboard remains the live owner until generic stats are projected (no
  regression). Runtime-unknown actor proof: an entity with only Identity/Team/
  Stats appears in the hot scoreboard (selftest PASS).
- DATA GAP (recorded): the client only receives top-3 FFA rows + TDM team kills
  (`CommunityMatchClient`), so a full per-actor scoreboard needs a generic
  stats projection/replication (networking follow-up). Recorded, not blocking.
- Deferred: discrete settings SELECT (resolution/preset).

## Round 52 (2026-09-15, generic objective state + CS HUD hot-owned) — source implemented

- AUDIT FINDING: there is **no shipping loadout or spectate menu** in the repo
  (mirrors the agent instruction to audit and not invent settings/features).
  Weapon selection is in-game via the existing generic equip substrate; spectate is
  camera mechanism only. Building those screens would be a new feature, not an
  ownership migration, so they were not created.
- Generic objective primitive: `HOT_OBJECTIVE_COMPONENT` /
  `HotObjectiveStateV1` (objectiveId/stateHash/ownerTeam/progress/timer/flags).
  Reusable for bomb, capture point, payload, flag, control zone. No CS primitive.
- Transitional cold projection: `ModeHud::projectFromClient` now also writes the
  objective from `CommunityMatchClient` bomb state, and marks `counterstrike` as
  hot-covered.
- `hot.match-hud` renders the objective line + progress bar by interpreting the
  generic `stateHash` (bomb.carried/planted/defusing).
- Cold CS HUD (`gGamemodeManager.renderHud`) now yields when `ModeHud::hotOwned()`.
- Proof: full suite PASS (incl. counterstrike selftest). Live appearance is debt.

## Round 51 (2026-09-15, settings hot-owned via generic setting seam) — source implemented

- Generic setting seam: `GAME_CAP_SETTING_GET`/`SET` + `GameSettingV1`
  (`setting.get`/`setting.set`). Logical ids map to real `PlayerSettings` fields;
  the kernel clamps/validates (fov 60..140, volumes 0..1, sensitivity 0.01..1).
  Hot code never sees a settings object; values stay engine-authoritative.
- Generic widgets: `GAME_UI_SLIDER` (min/max/step) + `GAME_UI_TOGGLE`; the backend
  hit-tests and emits `VALUE_CHANGED` with the new value. No setting-specific ABI.
- Hot `hot.settings-screen`: composes sliders/toggles for fov/master/music/sfx/
  sensitivity/mute from `setting.get`, handles `VALUE_CHANGED` -> `setting.set`
  (kernel validates), BACK -> nav main, and writes the `screen.settings` claim.
- Navigation: `menu.settings` now routes hot (`screen.settings`); PLAY/QUIT leave
  hot screens so the hot shell never overlaps cold screens. Cold `drawMainMenu`
  yields whenever hot owns any screen.
- Proof: `--hot-combat-selftest` PASS incl. get/set round-trip, clamp validation,
  settings composition + claim, and a `VALUE_CHANGED` action modifying the real
  setting. Full suite PASS.
- Recorded next: a generic SELECT for discrete settings (resolution/window mode/
  preset); then loadout, spectate, scoreboard, CS objective state; then audio.

## Round 50 (2026-09-15, hot main menu is the default shipping owner) — MILESTONE

- Hot main menu claim is ON by default (`g_menuEnabled = true`); cold
  `drawMainMenu` yields as the fallback. Shell coverage: background + logo via
  logical texture resources (`resource.register`), title, account name/stats
  (MMR/W/L/K/D), VIP tier colour, primary nav, account/auth entry buttons, and an
  approximate 3D avatar preview via the generic `uiClip` + view-space
  `render.mesh`.
- Generic pending-action bridge: hot UI routes logical account/screen actions
  (`menu.play`, `menu.settings`, `menu.quit`, `account.signin/signup/switch/logout`)
  through `HotUiPendingActionV1`; the cold menu layer consumes it and performs the
  secure/screen transition (tokens/passwords/screen enum stay cold). Id-based,
  generation-safe.
- `HotMenuShellStateV1` extended with profile stats + tier; cold projection
  populates it.
- Hot UI runs in `GAME_MENU`; nav state is migratable; claim recomputed per frame.
- Proof: `--hot-combat-selftest` PASS; full suite PASS. Live appearance not
  screen-verified (debt).
- STILL COLD (recorded): settings/loadout/spectate/scoreboard/CS objective+HUD;
  audio policy; resource generations. Avatar preview framing is approximate.

## Round 49 (2026-09-15, migratable hot UI nav state + generation-safe claim) — source implemented

- Hot UI navigation state moved out of the module static into a migratable
  dynamic component `HOT_UI_NAV_COMPONENT` / `HotUiNavigationStateV1`
  (screenId/previousScreenId/modalId/focusId). A hot generation swap no longer
  resets the current screen to main.
- Generation-safe claim: `LiveUi::beginFrame()` clears `HotUiClaim` each frame, so
  the active generation must re-assert ownership; a new generation that removes/
  renames a screen leaves no claim and the cold owner recovers (no blank UI, no
  permanent cold-yield).
- UI action routing reads/writes the migratable nav component (no static).
- Proof: `--hot-combat-selftest` PASS incl. "hot navigation state is migratable
  component state" and "hot screen claim is recomputed per frame
  (generation-safe)". Full suite PASS.
- STILL OPEN for the main-menu flip (recorded): shell coverage (avatar preview via
  uiClip, logo/background images, account stats, VIP style, auth entry points,
  modals). Claim remains OFF by default until coverage is sufficient.

## Round 48 (2026-09-15, hot UI in the menu + shell data + 3D-in-UI primitive) — source implemented

- Fixed `GAME_UI_BUTTON`: it was hit-tested but never drawn (invisible buttons).
  `LiveUi::endFrameAndDraw` now draws the button rect + border + centered label.
- Hot UI now runs while `GAME_MENU` is active (`gui-main.cpp` runs the UI domain +
  `LiveUi::beginFrame/endFrameAndDraw` around the menu switch). Before this the
  hot menu could never render live (engineTickUI is game-only), so the claim was
  moot.
- Generic menu-shell data: `HOT_MENU_SHELL_COMPONENT` / `HotMenuShellStateV1`
  (username/version/avatar/flags/connection) projected once by cold
  `MenuShell::project()` from typed auth/avatar/version. Hot menu policy composes
  usernames/status without reading cold GUI/account objects.
- Hot `hot.main-menu` now composes shell chrome (background panel, title,
  username, version) + PLAY/SETTINGS/QUIT.
- New generic 3D-in-UI primitive: `GameRenderMeshCommandV1.uiClip` binds a mesh
  draw to a UI rect (viewport + scissor) in `submitMesh`; reusable for avatar/
  inventory previews, editor viewports, spectator thumbnails.
- Claim still OFF by default: the shell still lacks avatar/logo/account-stats/auth
  entry points, so flipping now would drop required UI. Remaining to flip: hot
  avatar preview (primitive now exists), logo/background images, account stats,
  and the sign-in/sign-up/switch/logout entry points.
- Proof: full suite PASS incl. hot main menu emits widgets, generic ui.action,
  claim, navigation. `build_agent.py` SUCCESS.

## Round 47 (2026-09-15, generic UI action event + hot menu composition) — source implemented

- Generic UI interaction ABI (no per-widget callbacks): `GAME_UI_BUTTON` widget
  kind + `elementId` on `GameUiCommandV1`; `GAME_EVENT_UI_ACTION` + `GameUiActionV1`
  {elementId, actionType, value, pointerX/Y, handled}; `GameUiActionType`.
- Backend: `LiveUi` stores only logical element ids + rects (generation-safe, no
  hot function pointers), hit-tests on click, and dispatches the generic
  `ui.action` event via `LiveBehavior::dispatchGameplayEvent64`. Wired into
  `uiBeginFrame` on the mouse click edge.
- Hot policy: `hot.ui-actions` event handler maps element ids (`menu.play`,
  `menu.settings`, `menu.quit`) to navigation; `hot.main-menu` composes panel +
  interactive buttons via render.ui and writes `HotUiClaim` (screenId). The
  `uiscreen` command sets the screen (dev/selftest).
- Cold yield: `LiveUi::hotOwnsScreen(screenId)` gates the cold `drawMainMenu`.
- Safety: the hot main menu is OFF by default (`uiscreen` enables it) because it
  currently covers only the buttons; claiming the live screen would drop the cold
  background/account/avatar panels. Cold remains the single live owner until
  coverage is complete.
- Proof: `--hot-combat-selftest` PASS incl. hot main menu emits widgets, backend
  reports the click as a generic ui.action, hot UI claims the screen, and hot
  navigation moved off the main menu after the action. Full suite PASS.
- Recorded next: CS HUD objective facts, scoreboard, remaining menus/settings/
  loadout/spectate, audio policy, resource generations.

## Round 46 (2026-09-15, TDM/FFA mode HUD hot-owned via generic state) — source implemented

- Generic mode-HUD claim: `HOT_MODE_HUD_CLAIM_COMPONENT` / `HotModeHudClaimV1`
  (registered by the hot package). `hot.match-hud` now composes only when the
  claim says hot owns the mode's HUD (otherwise the cold client HUD owns).
- Transitional cold projection: `gui/hud/mode-hud-bridge.{h,cpp}`
  `ModeHud::projectFromClient()` projects the typed cold `CommunityMatchClient`
  match state ONCE into generic `MatchHudState` + `ModeHudClaim` on the generic
  match entity. Hot code never reads `CommunityMatchClient`; the projection is a
  compatibility bridge, not the long-term authority (forward path: authoritative
  hot mode writes/replicates generic state).
- Cold yield: `ModeHud::hotOwned()` gates the cold FFA/TDM HUD block
  (`engine-tick-ui-overlays.cpp`) and the cold `MatchTimer`
  (`engine-tick-ui-hud.cpp`) — the latter also fixes a latent coupling where the
  always-on overlays made `hotOwnsHud()` true and would have starved the timer.
- Coverage this pass: TDM + FFA (timer/scores/phase text/team labels). The claim
  is written `owned=0` for other modes, so CS/duel HUDs stay cold (no duplicate,
  no loss).
- Proof: `--hot-combat-selftest` PASS incl. "cold owns the mode HUD when the
  claim is not owned" and "hot mode HUD composes when the claim is owned"; full
  suite PASS.
- Recorded next: CS HUD needs generic objective facts (bomb/defuse) before it can
  be claimed; scoreboard needs generic row data; both stay cold.

## Round 45 (2026-09-15, actor overlays hot-owned per-actor) — source implemented

- Actor overlay coverage: generic `ActorIdentityState` (+ `GameHealthComponentV1`,
  `ActorTeamState`) is now projected for local, network players and network NPCs
  onto the shared actor EntityId (`PresentationEntities::projectActorOverlayState`
  / `actorEntityFor`; NPC generic actors via `ensureActor`).
- `hot.actor-overlays` now enumerates `ActorIdentityState` (one generic path, no
  per-player/per-NPC loop), projects the head via `world.project`, and emits
  name + team-coloured health bar + HP text via `render.ui`. It is ON by default
  and writes `ActorOverlayClaim` on every actor it handles.
- Per-actor cold yield: `drawPlayerHealthbar` gains `actorEntity` and returns
  early when the actor carries `ActorOverlayClaim` (exactly one owner; partial
  coverage stays safe — uncovered actors keep cold). Local NpcSystem bodies pass
  0 (still cold) until they are on the generic actor path.
- Proof: `--hot-combat-selftest` PASS incl. world.project front/behind, generic
  actor overlay via render.ui, and "hot overlay claims the actor (cold yields
  per-actor)". Full suite PASS.

## Round 45b (2026-09-15, mode HUD) — BLOCKED on generic client match-state exposure

Recorded, not implemented to avoid a duplicate owner: the cold FFA/TDM/CS HUD
(`engine-tick-ui-overlays.cpp:606-683`, `gamemode-manager.cpp`) is driven by the
typed cold `CommunityMatchClient` (redScore/blueScore/phase/phaseTimer/mode) and
does NOT yield on `hotOwnsHud()` (only the cold `MatchTimer` does). Writing a hot
`MatchHudState` from this requires a generic exposure of client match state (or a
replicated `MatchHudState` written by the hot mode), plus a mode-HUD claim so the
cold JSON HUD yields. That is a deliberate primitive/bridge decision; a partial
writer would duplicate or drop the HUD, so it was not wired.

## Round 44 (2026-09-15, weapon batch + equip lifecycle + overlays substrate) — source implemented

- Weapon presentation batch (no new ABI): shotgun / rocket_launcher /
  grenade_launcher added to the hot `g_bindings` (tool key = `gameHash(weaponId)`
  -> logical mesh -> GLB). Every standard weapon now has a generic tool identity
  and hot presentation.
- Equip lifecycle: `PresentationEntities::projectLocalPlayer` reconciles the
  local actor's generic `equips-item` identity from the typed mirror when they
  disagree (spawn/respawn/join/reconnect), so the identity reconstructs without a
  manual re-equip. `WeaponSystem::equip`/`unequip` remain the authoritative
  action; typed fields are mirrors.
- New generic primitive `GAME_CAP_WORLD_PROJECT` (`world.project`): world position
  -> screen x/y + in-front flag + depth, using the live camera and kernel
  viewport. Legitimate cold mechanism; headless-capable (kernel viewport
  fallback). Reusable for nameplates, prompts, objective labels, damage
  indicators, editor gizmos.
- New generic component `ActorIdentityState` (actor-state): display name,
  `GAME_NET_ALL`; one cross-system source for nameplates/chat/killfeed/scoreboard.
  Populated for the local actor from the typed username.
- New hot system `hot.actor-overlays` (`ui.frame`): enumerates generic actors
  (PresentationState + Health + optional Identity), projects the head point, and
  emits a health bar + HP text + name via `render.ui`. No typed player/npc.
- Ownership gating: cold `player-nameplates.cpp` remains the live owner (this
  is off by default, toggled with `hotoverlays 1|0`) because a full flip needs a
  per-actor ownership gate and remote/NPC identity coverage; this avoids a
  duplicate owner. Recorded as the next overlay step.
- Proof: `--hot-combat-selftest` PASS incl. world.project front/behind, generic
  actor overlay via render.ui, equip lifecycle/bridge/switch/unequip, weapon
  batch. Full suite PASS.
- Audits recorded (not implemented): audio policy owners (NPC/UI/ambient/music),
  UI composition owners (CS/TDM/FFA HUD, scoreboard, menus), and resource classes
  (sounds/clips/skeleton/fonts) — see hot-cold-audit.md.

## Round 43 (2026-09-15, standard weapon-slot equip -> generic tool identity) — source implemented

Option A (staged): the real weapon-slot equip now produces the generic tool
identity. Tool key = `gameHash(weaponId)` (no enum, mod-friendly), matching the
key runtime tools already use.
- `actor-state`: `actorStateEquipWeaponKey(actor, toolKey, realm)` reuses a
  persistent generic tool entity per (actor, key), writes `ToolRefState`, and
  equips it (one equipped tool per actor). `actorStateUnequipTool(actor)` removes
  the edge; the tool entity persists.
- `WeaponSystem::equip`/`unequip` (`weapon-system-equip.cpp`) call these. Typed
  `Player.equippedWeaponId`/`equippedSlot` stay as compatibility mirrors
  (authority: generic edge; typed mirrors follow).
- Hot bindings re-keyed to `gameHash("swordsword")` / `gameHash("revolver")`.
- One-frame double owner avoided by running `hot.tool-presentation` in
  `GAME_DOMAIN_POST_MOVEMENT` (before the cold render pass) so the claim is fresh
  when `WeaponViewModel::render` decides to yield the same frame.
- Selftest: standard weapon-slot bridge creates identity + real swordsword
  reaches hot presentation; switch keeps one equipped tool and the old identity
  persists; unequip removes the edge with no stale claim; unmigrated shotgun
  keeps the generic identity but cold fallback (migrated==0); revolver same
  substrate. Full suite PASS.
- Remaining (staged): the bridge is client-local (Local realm). The
  server-authoritative counterpart so REMOTE observers see other players' tools
  is not wired yet, and NPC standard weapons remain typed-only. No new ABI.

## Round 42 (2026-09-15, second weapon no new ABI + live-equip blocker traced) — BLOCKED at gate

- Second weapon (revolver, tool key 1) migrated on the SAME substrate: one hot
  binding entry (`mesh.tool.revolver` -> `assets/objects/weapons/mimita-revolver-v1.glb`).
  No kernel/ABI change, no new context field, no per-weapon registry. Selftest
  proves the real revolver carries its logical mesh + claim via the real generic
  equip API.
- Ordering safety: an `equips-item` edge without `ToolRefState` yields no claim
  and no presentation (no crash, cold stays owner). Missing socket/parent hides.
- Runtime-unknown tool uses the same real path (entity + equips-item +
  ToolRefState; presentation system does the rest).
- **LIVE-EQUIP BLOCKER (exact, with evidence):** the standard weapon-slot equip
  path never creates a generic tool entity or `equips-item` relationship.
  `WeaponSystem::equip`/`unequip` (`src/combat/weapon-system-equip.cpp:84-170`)
  only mutate `Player.equippedWeaponId`/`equippedSlot`; the generic tool entity +
  `equips-item` is created only for items/runtime tools
  (`serverItemEquip`/`serverEquipRuntimeTool`, `src/network/server-attack.cpp:1482,1540`)
  or on a runtime-tool use with `toolId != 0` (`:1285`). So `hot.tool-presentation`
  sees nothing for a normally-equipped swordsword; the cold viewmodel stays the
  owner. Replication itself is generic and fine (`dynamic-replication.cpp`).
- STOPPED at the tool-presentation gate (#19 items 1-3 not met). Fix requires an
  equip-authority decision: (A) route weapon-slot equip through the existing
  generic tool entity + `equips-item` (true convergence, touches equip authority),
  or (B) a client presentation projection for the local equipped weapon
  (analogous to `projectLocalPlayer`). I did not pick unilaterally because (A)
  materially changes gameplay/equip authority and neither is verifiable live here.
- Proof: `--hot-combat-selftest` PASS (revolver same substrate, ordering safe,
  unmigrated shotgun fallback); full suite PASS (artifact-transfer transiently
  broke from concurrent work, then went green).

## Round 41 (2026-09-15, real equip flow on the generic substrate + local body owner) — source implemented

- Local body duplicate owner resolved: `hot.presentation-mesh` skips the local
  possessed actor entity (`GameSharedStateV1.localPlayerEntity`), so the cold
  body mechanism is the one local-body draw owner. Tools/attachments/effects on
  other EntityIds still draw generically. Regression test: submission delta with
  the actor visible == delta with it skipped + 1.
- Real equip bridge uses the EXISTING generic substrate; no new ABI:
  `relationship.equips-item` + `ToolRefState.toolKey` (cold generic equip API
  `actorStateEquipTool`/`actorStateGetEquippedTool`). Hot `hot.tool-presentation`
  (RENDER, order 3) reads the actor's equipped tool EntityId, hot policy maps the
  tool key -> logical mesh (hot C++, not a kernel weapon DB), and writes
  `PresentationState` + `AttachmentState` onto the REAL tool EntityId. Local
  possessed actor -> VIEW (first person); other actors -> WORLD.
- Cold `WeaponViewModel::render` yields when the local actor carries a hot
  `ToolPresentationClaim` matching the equipped tool key (one owner). Unmigrated
  weapons have no claim and keep the cold path.
- Runtime-unknown tool uses the SAME real path: `hottool` only creates the entity
  + `equips-item` + `ToolRefState`; presentation then flows through
  `hot.tool-presentation -> hot.attachment -> render.mesh`. No debug-only draw.
- Proof (`--hot-combat-selftest` PASS): local body submitted once; real equip API
  resolves the tool EntityId; real swordsword tool carries `mesh.tool.swordsword`;
  attachment resolves; tool EntityId preserved across presentation changes;
  unmigrated (revolver) keeps cold fallback; runtime-unknown tool via real
  substrate; view-space context. Full suite PASS.
- HONEST LIMITATION: the "real equip" proof is headless via the real generic
  equip API, not an interactive equip. Whether the live client populates
  `equips-item` for the local actor (so the cold viewmodel actually yields) is not
  yet verified on screen; until then cold is the safe fallback (no regression).

## Round 40 (2026-09-15, generic attachment/socket + logical mesh resources + view space) — source implemented

Approved generic primitives (no weapon/tool ABI):
- `GAME_CAP_SOCKET_QUERY` (`socket.query`): entity + socket/bone hash + caller
  local offset -> world transform. Cold composes the entity's canonical
  transform with its current generic skeleton pose (SkeletonInstances) and the
  drawn mesh's part bind (resolved live by logical id, never a raw handle);
  non-skeletal parents fall back to entity transform + local offset; a missing
  entity fails safe (`valid=0`).
- `HOT_ATTACHMENT_COMPONENT` / `HotAttachmentStateV1`: generic presentation
  attachment (parent entity, socket, local TRS, context). It is a presentation
  override only - the child's authoritative Transform is never overwritten.
  Hot system `hot.attachment` (RENDER, order 4) resolves it each frame; `hot.presentation-mesh`
  (order 5) consumes the resolved transform and hides the entity when unresolved.
- `GAME_CAP_RESOURCE_REGISTER` (`resource.register`): hot code registers an
  arbitrary logical mesh/texture id backed by a path in the existing
  generation-aware provider; `PresentationRender::poll()` re-applies file-backed
  dynamic resources so a generation can swap while entities survive. Malformed
  GLBs preserve last-good (no bad generation).
- `GAME_RENDER_MESH_SPACE_VIEW` (flags bit0): a generic camera-relative (VIEW)
  presentation space; the cold renderer uses an identity view and keeps the
  projection/depth mechanism. No "this is the X viewmodel" branch.

Runtime-unknown proof: `hotmesh <path>`, `hottool [path]` (third-person, world
space, attached to a parent `rightArm` socket), `hottool1p [path]` (first-person,
view space). All use `assets/objects/weapons/mimita-hafs-v1.glb` (the real
swordsword model) but are driven by logical ids, never an enum.

STOPPED before the real swordsword equip migration: it needs a generic tool->mesh
data mapping (resource manifest) and a client possessed-tool bridge. Both are
deliberate data/ownership decisions, not new mechanisms, so the primitives are
complete and the migration is the next step. Cold weapon viewmodel remains the
only owner for real weapons (no duplicate owner introduced).

Proof: `--hot-combat-selftest` PASS incl. socket fallback/fail-safe,
runtime-unknown logical mesh, malformed-mesh last-good, attachment resolve,
first-person view space; full suite PASS.

## Round 39 (2026-09-15, hot footstep/air-jump audio + effect-dispatch bug fix) — source implemented

- Footstep audio: real local trigger `entities/player.cpp:313` now dispatches the
  generic `effect.footstep.sound` fact; hot `hot.effect-composition` owns
  cadence sound choice (honoring an arbitrary logical id in `EffectRequestV1.text`),
  volume, pitch, and falloff and emits `audio.play`. Cold `playWorldSound` is the
  fallback only (yields when handled).
- Air-jump audio (second proof on the SAME substrate): `player.cpp:258` dispatches
  `effect.jump.sound`; hot policy emits `audio.play` (`entity/player/doublejump`);
  cold `playAirJumpSound` yields.
- Landing: audit found **no** shipping cold landing sound (only VFX
  `HitEffects::spawnLandingBurst`), so there is no policy to migrate; recorded.
- Dead cold audio helpers with no callers: `playRandomFootstep` (`audio.cpp:430`),
  `playFreezeBegin/Hold/EndSound` (`:449-462`).
- Bug fixed (FIX NOW): `hot.effect-composition` `onEffectRequest` set
  `handled = 1` for every fact and defaulted unknown facts to an explosion, which
  would silently suppress any unmigrated cold owner. `handled` is now set only by
  real branches; unknown facts stay unhandled so the cold owner runs (exactly one
  owner). Selftest asserts this.
- Proof: `--hot-combat-selftest` PASS incl. "grounded footstep audio policy is
  hot", "arbitrary logical footstep sound id is hot", "air-jump audio uses the
  same movement-fact substrate", "unmigrated landing audio stays cold-owned";
  full suite PASS.
- Weapon-presentation audit done (see `hot-cold-audit.md` item 3). Blocked on two
  deliberate ABI decisions: generic socket/bone world-transform query, and weapon
  GLB as a logical presentation mesh resource. First target chosen: `swordsword`.

## Round 38 (2026-09-15, screen effect via hot UI + hot weapon-fire audio) — source implemented

- Screen effect: no dedicated cold damage-flash owner exists; reused the hot UI
  path. Hot `hot.screen-fx` (`ui.frame`) owns a fading full-screen panel; the
  `hotscreenfx` command triggers a runtime-unknown screen effect. No compositor
  duplication.
- Weapon-fire audio: `WeaponAudio::playShootSound` (the common cold fire-sound
  seam) now emits a generic `effect.weapon.fire.sound` fact; the hot
  `hot.effect-composition` policy owns volume/pitch/falloff and emits
  `audio.play`; the cold playback yields (one owner). `EffectRequestV1` gained a
  generic `char text[64]` logical-name field.
- Proof: `--hot-combat-selftest` PASS ("hot screen-effect command registered
  (reuses hot UI)", "real weapon-fire sound policy is hot (audio.play)"); full
  suite PASS.
- Cold-restart item 2 reduced: explosion + weapon-fire sound policy hot;
  footstep/UI/NPC/music remain.
- Next: footstep/landing audio; then weapon presentation (tool entity +
  PresentationState/AnimationState + attachment + effect/audio).

## Round 37 (2026-09-15, generic camera-effect primitive + hot explosion shake) — source implemented

- New generic `camera.effect` capability + `GameCameraEffectV1` (pitch/yaw
  amplitude, falloffDistance/distance, source, runtimeKey). Kernel
  `capCameraEffect` applies `camera.addPunch` with distance attenuation; it has
  no explosion/damage branch.
- Real explosion camera shake (`weapon-rocket-launcher.cpp`) now emits a generic
  `effect.camera.shake` fact; the hot `hot.effect-composition` policy chooses
  amplitude/falloff and emits `camera.effect`; the cold `camera.addPunch` is the
  compatibility fallback. `hotcamerafx` proves runtime-unknown camera effects.
- Proof: `--hot-combat-selftest` PASS ("real explosion shake reaches the hot
  camera policy", "hot camera-effect command works (no enum)"); full suite PASS.
- Note: a concurrent in-progress `reconcile-policy.cpp`/`hot-reconciliation.h`
  briefly broke the DLL build (their area, since fixed), and a stale
  `live-behavior.o` had to be invalidated.
- Cold-restart item 1 reduced: explosion/hit/blood/surface/muzzle/camera-shake
  hot; remaining screen flash / damage vignette.
- Next: generic screen-effect primitive + one real screen path; then weapon-fire
  audio; footstep audio; weapon presentation.

## Round 36 (2026-09-15, muzzle flash hot) — source implemented

- `EffectPartSystem::spawnMuzzleFlash` (the single cold owner called by all
  weapon-fire paths) now emits a generic `effect.request` (`effect.muzzle`) with
  the tool key as a runtime hash; the hot `hot.effect-composition` policy
  composes a generic flash effect entity and sets `handled = 1`, so the cold
  composition yields (one owner).
- Proof: `--hot-combat-selftest` PASS ("real weapon-fire fact reaches the hot
  muzzle policy (runtime tool key, no enum)"); full suite PASS.
- Cold-restart item 1 reduced: explosion + hit/blood + surface + muzzle hot;
  remaining screen flash and camera shake.
- Next: camera/screen effect policy; weapon-fire audio; footstep audio; weapon
  presentation.

## Round 35 (2026-09-15, generic surface-effect/decal primitive) — source implemented

- New generic `surface.effect` capability + `GameSurfaceEffectV1` (position/
  normal/axis/color/radius/height/lifetime/persistence). Kernel
  `capSurfaceEffect` pushes a generic `SurfaceDecal` (`generic = true`); the
  renderer draws it from color/size only and never interprets a feature kind.
- Hot `hot.effect-composition` emits a surface effect for `effect.hit.blood` /
  `effect.hit.world`; `hotsurfaceeffect` command creates a runtime-unknown mark.
- Proof: `--hot-combat-selftest` PASS ("hot surface-effect policy creates a
  generic decal (no enum)"); full suite PASS.
- Cold-restart item 1 reduced: hit/blood/world composition + surface marks hot;
  remaining muzzle flash, screen flash, camera shake.
- Next: muzzle flash; camera/screen policy; weapon-fire/footstep audio; then
  weapon presentation.

## Round 34 (2026-09-15, movement policy verification — already hot/shared) — verified, no changes

- Verified (read-only) that air/ground/gravity/speed/jump/dash/down-dash/freeze/
  speed-clamp are already shared hot policies (`hot-movement-policy.h`,
  `modules/movement-dash.cpp`, `movement-freeze.cpp`, `movement-speed-clamp.cpp`,
  `movement-system.cpp`), dispatched from the cold movement step and used by local
  prediction identically. `--movement-algorithm-selftest` and
  `--movement-parity-selftest` PASS.
- No edits made: re-implementing would duplicate the concurrent movement agent's
  ownership. Movement/integrator authority remains theirs.
- This agent's next cold owners remain client/presentation: decal primitive,
  muzzle flash, camera/screen policy, remaining audio policy, weapon presentation.

## Round 33 (2026-09-15, real hit/blood composition hot) — source implemented

- `effects/hit-effects.cpp::HitEffects::onHit` (the real client hit/blood/world
  impact composition owner) now emits a generic `effect.request`
  (`effect.hit.blood` / `effect.hit.world`); the hot `hot.effect-composition`
  handler composes generic effect entities and sets `handled = 1`, so the cold
  composition yields. Exactly one owner.
- Proof: `--hot-combat-selftest` PASS ("real hit/blood fact reaches the hot effect
  owner"); full suite PASS.
- Cold-restart item 1 reduced again: explosion + hit/blood composition hot.
  Remaining: muzzle flash, decals (no generic decal primitive yet), screen flash,
  camera shake.
- Next: generic surface-effect/decal primitive; bullet/impact via the same
  substrate; muzzle flash; camera/screen policy; then weapon-fire audio.

## Round 32 (2026-09-15, generic hot audio policy) — source implemented

- New generic `audio.play` capability + `GameAudioCommandV1` (logical sound name,
  position, volume, pitch, maxDistance, spatial). Kernel `capAudioPlay` calls the
  cold `playWorldSound`/`playSoundPitched`; hot policy chooses everything else.
  No per-weapon/feature audio ABI.
- The real rocket/grenade explosion sound is now hot-owned
  (`hot.effect-composition` emits it); the cold sound in `explosion-fx.cpp` moved
  into the compatibility fallback. `hotaudiotest` plays a runtime sound.
- Proof: `--hot-combat-selftest` PASS ("hot audio policy plays sounds via the
  generic command"); full suite PASS.
- Cold-restart item #2 reduced: explosion sound policy hot; weapon/footstep/UI/
  music policy + sound resources still cold.
- Next: blood/decal/bullet/muzzle-flash/camera-shake effects, then remaining audio
  policy, then weapon presentation.

## Round 31 (2026-09-15, real rocket/grenade explosion composition hot) — source implemented

- `explosion-fx.cpp` (real client rocket/grenade explosion) now emits a generic
  `effect.request` fact after the (temporary cold) sound; the hot
  `hot.effect-composition` handler composes generic effect entities (flash,
  smoke, debris) and sets `handled = 1`, so the cold composition yields.
- Generic `EffectRequestV1` payload + `LiveBehavior::dispatchEffectRequest`
  (cold -> hot). No effect enum, no ABI field.
- Proof: `--hot-combat-selftest` PASS ("real explosion fact reaches the hot
  effect owner and composes generic effects"); full suite PASS.
- Cold-restart item #1 reduced again: rocket/grenade explosion composition and
  lifetime are hot; remaining cold effect types: bullet impact, blood, muzzle
  flash, decals, screen flash, camera shake. Sound stays cold temporarily.
- Next: blood impact; generic decal primitive if needed; audio policy.

## Round 30 (2026-09-15, generic hot effect lifecycle) — source implemented

- New generic `EffectLifetime` (`hot-effect.h`, GAME_NET_NONE) + hot
  `hot.effect-lifecycle` (`render.frame`, priority 3): owns age, Velocity
  integration, scale growth, alpha fade, and expiry/destroy for any effect
  entity. Creation uses the existing generic `effect.spawn`/entity primitives.
- Runtime-unknown effect proof: `hoteffect` command creates a brand-new effect
  (cube rises, grows, fades, expires) with no enum/switch/ABI field.
- Proof: `--hot-combat-selftest` PASS ("runtime-unknown effect entity created (no
  enum/switch)", "hot effect ages, integrates, and grows", "hot effect expires
  and is destroyed"); full suite PASS.
- Cold-restart item #1 (effects) partially reduced; remaining: client
  `EffectPartSystem` composition branches (bullet impact, blood, muzzle flash,
  decals, camera shake) still cold.
- Next: wire the client rocket-explosion event to the hot effect entity so the
  cold composition yields; then blood; then audio policy.

## Round 29 (2026-09-15, client/presentation cold-owner audit + command registration) — source implemented

- Added the client/presentation cold-owner audit table and the primary metric
  section "BUGS THAT STILL REQUIRE A COLD EXE REBUILD" to `hot-cold-audit.md`.
  Largest remaining client cold owners: effects, audio, weapon presentation,
  nameplates, menus/UI interaction, clip/skeleton/font/sound resource
  generations.
- Command registration is already generic (`CommandRegistrar` ->
  `GenericRuntime`); proved `hasCommand("posedebug"/"hotactor"/"hotpresent")`,
  so runtime-new commands do not need a cold switch (post-startup dynamic
  add/remove remains a future primitive, priority low).
- Proof: `--hot-combat-selftest` PASS ("hot package registers commands without a
  cold switch"); full suite PASS.
- Next: effects hot ownership (effect behaviors emitting generic
  `effect.spawn`/`render.debug` with logical resources).

## Round 28 (2026-09-15, local player migrated to the generic pose path) — source implemented

- THE_PLAYER now has a canonical generic entity (`Ecs::ensureLocalPlayerEntity`)
  projected each frame with Transform/Velocity/Health + `PresentationState` +
  `AnimationState`. The hot `hot.animation-policy` selects its clip and
  `hot.pose-generation` writes `SkeletonInstances[localEntity]`.
- `PresentationEntities::applyHotPoseToPlayer` maps the hot skeleton pose onto the
  visible body parts; the typed renderer only draws the body (mechanism).
- `animation.update` is no longer called by the local-player path
  (`animation.main` is now a no-op); the capability remains for replay/legacy
  compatibility (A/B).
- Proof: `--hot-combat-selftest` PASS ("local player has a canonical generic
  entity", "local player entity is driven by the hot pose path"); full suite PASS.
- NOT claimed: LIVE HOT-EDIT PROVEN (no screen). Editing
  `pose-generation.cpp` now changes THE_PLAYER at the architecture level.
- Remaining cold Player owners: body meshes/skeleton/skinning/first-person draw
  (mechanism); `updateProceduralAnimation` still exists but is not the local
  animation policy owner.

## Round 27 (2026-09-15, live visual proof tooling) — source implemented

- `posedebug 1|0` forces an unmistakable pose for visual proof; `hotactor`
  spawns a typeless local entity on the full generic chain (`mesh.actor` +
  AnimationState + PresentationState).
- Proof: build SUCCESS; `--hot-combat-selftest` + full suite PASS.
- NOT claimed: `LIVE VISUAL PROVEN`, `LIVE HOT-POSE EDIT PROVEN` (no screen
  access). Human steps in the changelog. Jump/attack/blend gated behind it.

## Round 26 (2026-09-15, part-aware real actor GLB) — source implemented

- The actor GLB is rigid multipart (named nodes with local binds). `loadGlbMesh`
  now parses part-aware and populates `GpuMesh::parts` (bone hash, index range,
  world bind); non-articulated GLBs keep the static parse. `inspectGlbParts`
  exposes the structure headlessly.
- Proof: `--hot-combat-selftest` PASS ("real actor GLB parses into named body
  parts", plus the skinned consumption + fallback checks); full suite PASS.
- Remaining: LIVE visual confirmation of on-screen deformation; attack/jump;
  blend model; generation-aware skeleton/clip resources.

## Round 25 (2026-09-15, render.mesh consumes SkeletonInstances) — source implemented

- `GpuMesh` gained optional bone-tagged `parts`; `submitMesh` looks up
  `SkeletonInstances::get(entityId)` and draws per part with
  `entityModel * boneWorld (* bind)`, else a static fallback. Lookup by EntityId
  only. `skinnedSubmissionCount`/`staticFallbackCount` + `debugInstallPartMesh`
  are the headless hooks.
- Proof: `--hot-combat-selftest` PASS (skinned consumption by EntityId; static
  fallback); full suite PASS.
- Remaining: populate `GpuMesh::parts` from a real (part-aware/skinned) GLB load
  so the real remote NPC visually deforms; then attack/jump + blend; then
  generation-aware skeleton/clip resources. `animation.update` intentionally kept
  (local-player compatibility).

## Round 24 (2026-09-15, generic per-entity skeleton instance) — source implemented

- New cold `render/skeleton-instances.*`: EntityId-keyed skeleton instance driven
  by `skeleton.apply` from hot `PoseState`. `purgeDead()` on entity destroy.
- `animation.update` is NOT dead (local-player `animation.main`); the migrated
  remote NPC never used it. Classified A.
- Proof: `--hot-combat-selftest` PASS (skeleton instance driven; purged on
  destroy); full suite PASS.
- Remaining: consume `SkeletonInstances` in a real GPU skinned draw (skinned mesh
  + shader); attack/jump clips; blend model.

## Round 23 (2026-09-15, hot pose generation via skeleton.apply) — VALIDATED

- New generic `PoseState` (`hot-pose.h`) + hot `hot.pose-generation`
  (`render.frame`, priority 2): generates idle/move/dead local bone poses from
  `AnimationState` and publishes them through `skeleton.apply`. The kernel stores
  the POD pose on the entity (`capSkeletonApply`); no DLL pointers retained.
- VALIDATED (2026-09-15): `build_agent.py` SUCCESS; `--hot-combat-selftest` PASS
  incl. "hot pose generation invokes skeleton.apply" and "hot pose publishes a
  generic PoseState on the entity"; full suite PASS.
- Remaining: attack/jump poses; driving real per-entity skeletons; retiring
  `animation.update` for the typed path.

## Round 22 (2026-09-15, generic hot animation policy) — source implemented

- New generic `AnimationState` (`hot-animation.h`, GAME_NET_ALL) and hot
  `hot.animation-policy` (`render.frame`) selecting idle/move/dead from Velocity
  + Health and advancing playback. Cold skeleton/skinning/draw untouched.
- Proof: `--hot-combat-selftest` PASS (move/idle/death clip selection); full
  suite PASS. Round 21 NPC presentation also validated (cold build SUCCESS).
- Remaining: attack/jump transitions; typed `updateProceduralAnimation` still
  owns pose generation/skinning (compatibility + mechanism); animation clips are
  not yet logical provider resources.

## Round 21 (2026-09-15, real remote-NPC presentation bridge) — VALIDATED

- `build_agent.py` SUCCESS in a no-process window; `--hot-combat-selftest` and
  the full suite PASS, including the NPC projection/persist/retire checks.

- `PresentationEntities::ensureActor` projects a real client NPC replica into a
  generic presentation entity; `engine-tick-render.cpp` yields typed NPC draw to
  the hot path when `mesh.actor` is loaded. Ownership gate prevents double draw
  and prevents invisible NPCs.
- Evidence so far: live-build generation 17; `-fsyntax-only` clean for all
  changed cold TUs. Cold link + `--hot-combat-selftest` PENDING because
  `mimita.exe` was running (`HOT_RELOAD_BOUNDARY_VIOLATION`); it was not killed.
- Next: run the cold build + selftests in a no-process window; then animation
  presentation.

## Round 20 (2026-09-15, UI image resources + actor-like generic presentation) — source implemented

- `GAME_UI_IMAGE` resolves generation-aware logical resource ids through the
  provider; new cold `uiDrawTexture` backend primitive.
- Typeless actor-like entity (Transform + Health + PresentationState team color)
  presents via the hot `hot.presentation-mesh` system and `render.mesh`.
- Proof: `--hot-combat-selftest` PASS ("ui image resolves a generation-aware
  resource handle", "typeless actor-like entity presents via render.mesh (team
  color as data)"). Full suite PASS.
- Not done: mode package populating `MatchHudState`; migrating typed
  player/NPC renderers (`NpcSystem::render` / `render-player.cpp`) to generic
  PresentationState (next slice); animation presentation.

## Round 19 (2026-09-14, generic hot HUD/UI composition) — source implemented

- New generic `render.ui` capability + `GAME_DOMAIN_UI` (`ui.frame`). `LiveUi`
  buffers hot-emitted widgets (text/panel/bar/image) and draws them through the
  cold immediate-mode UI backend. The engine only knows "run ui.frame".
- New hot module `modules/ui/hud.cpp` (`hot.match-hud`) composes timer + score
  panel + phase + round bar from a generic replicated `MatchHudState`. Cold
  `MatchTimer` draw yields via `LiveUi::hotOwnsHud()` (compatibility fallback).
- Proof: `--hot-combat-selftest` PASS (capability buffers; ui.frame fails safe;
  emits HUD commands; deterministic). Full suite PASS.
- Not done: `MatchHudState` population by the mode package; UI image logical
  resource resolution; kernel stack/anchor primitives (currently hot-side
  arithmetic); live visual proof.

## Round 18 (2026-09-14, GLB mesh loading via the resource provider) — source implemented

- `mesh.demo.glb` is a GLB loader in the existing `PresentationResourceProvider`:
  container validate -> reuse `loadGLB` (tinygltf) -> same `GpuMesh` upload.
  `PresentationState` keeps logical ids only; swap needs no entity recreation.
- Proof: `--hot-combat-selftest` PASS ("valid GLB container is accepted",
  "malformed GLB is rejected (last-good preserved)"). Full suite PASS.
- Not done: per-frame GLB hash polling (init-time only), live visual proof, GLB
  material/texture dependency graph, hot HUD widget tree.

## Round 17 (2026-09-14, real prediction key transport) — source implemented

- `ToolUsePolicyV1.predictionKey` (generic, append-only); server-attack populates
  it from the client request id / held intent id. Hot rocket/grenade tools write
  the generic `PredictionLink` on the created entity; a non-projectile test tool
  (`hot.predicted-test`) proves the path is generic. Replication is the existing
  generic dynamic-component envelope.
- Proof: `--hot-combat-selftest` PASS (authoritative projectile carries the
  prediction key; non-projectile predicted entity carries PredictionLink). Full
  suite PASS.
- Classified redundant (not removed): `PresentationEntities::ensure` bridge,
  write-only `predictedProjectileIds`.
- Next: GLB loading through `PresentationResourceProvider`, then hot HUD tree.

## Round 16 (2026-09-14, generic predicted -> authoritative entity association) — source implemented

- New ECS `PredictionRegistry` (type-agnostic): prediction key -> provisional /
  authoritative entity + status, with safe handling of every arrival order and
  automatic provisional retirement. Proven with a non-projectile entity.
- New generic `PredictionLink` hot component; client `ensurePredicted` +
  `associateByLink`. `PresentationEntities` is now a thin projection/prediction
  helper, not an identity owner.
- Proof: `--hot-combat-selftest` PASS (assoc: prediction-first, authority-first,
  destroyed authority, stale prune, client link). Full suite PASS.
- Missing server hook (documented, owned by server-combat agent):
  `ToolUsePolicyV1.predictionKey` populated from the attack request id and the
  hot projectile tools writing `PredictionLink` on the created entity. No context
  field added.
- Remaining: GLB loading, hot HUD tree, single-player/replay presentation.

## Round 15 (2026-09-14, projectile identity unified on the replicated EntityId) — source implemented

- New `PresentationEntities::projectReplicatedProjectiles()` projects replicated
  `HotProjectileState` position/velocity into typed Transform/Velocity on the
  REAL replicated EntityId, so the authoritative entity presents itself through
  the hot `hot.presentation-mesh` system. No parallel bridge identity for remote
  projectiles. Join-in-progress works from replicated state alone.
- `PresentationEntities::ensure()` is now prediction-only; it no longer owns a
  parallel normal-projectile identity. It remains a thin prediction adapter.
- Proof: `--hot-combat-selftest` PASS (replicated projectile projects onto the
  authoritative entity; presents via its own EntityId). Full suite PASS.
- Remaining: generic predicted->authoritative association key (needed to retire
  the provisional predicted entity exactly when the authoritative entity
  appears); single-player and replay compatibility; GLB loading; hot HUD tree.
  No `ProjectileType` switch or new game-api field.

## Round 14 (2026-09-14, client projectile presentation bridge removed) — source implemented

- New `src/render/presentation-entities.*`: materializes/updates/retires a generic
  client entity (Transform + Velocity + PresentationState) per live
  network/predicted projectile, tied to the projectile id.
- `mpRenderNetworkProjectiles` materializes the generic entity for rocket/grenade
  and suppresses the typed `renderProjectile` draw when the bridge owns the
  projectile; the hot `hot.presentation-mesh` system draws it via `render.mesh`.
- Proof: `--hot-combat-selftest` PASS (materialize, generic submit, typed
  suppression, persist, retire). Full suite PASS.
- Remaining typed owners: single-player `weapon-system.cpp`/`npc-combat.cpp`
  (inactive in MP) and the replay rocket path; `projectile-render.cpp` is a
  fallback for those. Not yet deleted.
- Not done / not visually proven: two-client visual handoff, unifying the bridge
  entity with a future replicated projectile entity, GLB loading, hot HUD tree.

## Round 13 (2026-09-14, real projectile presentation generic) — source implemented

- Server-side hot projectile entities (player/NPC rocket and grenade) now carry
  the shared generic `PresentationState` (`hot-reload/hot-presentation.h`) with
  logical resource ids, plus a `Transform`. `hot.presentation-mesh` orients them
  from `Velocity` (generic data, no renderer branch).
- `PresentationRender` registers `mesh.rocket` (cylinder), `mesh.grenade`
  (sphere), `texture.rocket`/`texture.grenade` through the generation-aware
  provider with per-frame polling.
- Proof: `--hot-combat-selftest` PASS (rocket and grenade projectiles carry
  `PresentationState`; render.mesh + provider checks). Full suite PASS.
- Classification: `weapon-system.cpp` (player rocket/grenade), `npc-combat.cpp`
  (NPC rocket), `multiplayer-projectiles.cpp` (network) and the replay rocket
  draw remain the client compatibility renderer for predicted/legacy
  projectiles; `projectile-render.cpp` is compatibility-only for now and has
  DEAD members (`clearProjectileMeshes`, `replay_rocket` branch).
- Not done / not visually proven: client predicted/network projectile entities
  carrying `PresentationState` (needs the other agent's client entity lifecycle +
  id mapping), deleting `projectile-render.cpp`, GLB model loading.

## Round 12 (2026-09-14, generic presentation of runtime entities) — source implemented

- New generic `render.mesh` capability + `GameRenderMeshCommandV1`: hot systems
  submit **logical** mesh/texture resource ids; the kernel resolves the
  generation handle and draws. No `GameplayContextV1` field.
- New `PresentationState` hot schema; `hot.presentation-mesh` presents any entity
  carrying it (Transform + PresentationState). Typeless runtime entities now draw
  through one generic path. `hot.debug-presentation` also composes a generic HUD
  panel (`GAME_RENDER_DEBUG_HUD_TEXT` -> `uiDrawText`) — one hot UI path.
- New cold `src/render/presentation-render.*`: real procedural-mesh and PNG
  texture loaders through the generation-aware provider (hash, swap, last-good,
  retire). `PresentationRender::poll()` runs per frame.
- Proof: `--hot-combat-selftest` PASS (render.mesh resolves; hot system presents
  a meshed generic entity; provider generation/no-op/swap/last-good). Full suite
  PASS.
- Not done / not visually proven: GLB model loader, wiring texture/mesh swap
  into a typed render path, migrating projectile rendering, replicated-entity →
  presentation end-to-end, full hot HUD widget tree, animation/effect/audio
  presentation.

## Round 11 (2026-09-14, generation-aware presentation + resources) — source implemented

- New generic `render.debug` capability (`GAME_CAP_RENDER_DEBUG`,
  `GameRenderDebugCommandV1`): hot render systems submit lines / wire boxes /
  wire spheres / world labels; the kernel owns the draw. Append-only, no
  `GameplayContextV1` field.
- New hot module `src/hot-reload/modules/presentation/debug-presentation.cpp`: a
  `render.frame` system owns debug/wireframe policy and presents any entity
  carrying the package `PresentationDebug` dynamic component (no type switch),
  plus a `hotpresent` command that creates such an entity. Wired via a new
  `modules/presentation/*.cpp` glob.
- New `PresentationResourceProvider` (`src/project/presentation-resource.*`):
  one generic logical-id -> content-hash -> immutable generation -> opaque
  handle mechanism with atomic swap, last-good preservation on failure, and
  retire-at-swap. Type-agnostic loader/retire callbacks.
- `Renderer::pollShaderReload()` routes `shaders/basic.*` through the provider
  (content-hash gated, last-good on failure), called once per frame.
- Proof: `--hot-combat-selftest` PASS (render.debug resolves; command reaches the
  kernel buffer; hot render.frame presents a generic entity; provider load /
  no-op / swap / last-good). Full suite PASS.
- Not done / not proven: live visual edit proof (wireframe and shader), texture
  and model/GLB loaders wired to the provider, generic entity presentation
  driven by replicated entities, HUD hot composition, animation/effect/audio
  presentation. Those are the next presentation-side owners.

## Round 2 (2026-09-12, repeated activation + observability) — implemented

- Per-process build isolation: `build/hotreload/p<pid>/gen<gen>/` with
  per-generation `build-result.json`/`build.log`, so client and server can no
  longer read each other's build outcome.
- Failed-hash retry with bounded backoff (2s -> 10s, indefinite), notification
  on every attempt, and `activeSourceHash_` only updated on success.
- Process identity on every journal event: `process`, `pid`, `session_id`
  (`src/live-code/live-identity.*`, auto-stamped by `live-journal`).
- `hot_damage_policy_result` now records `generation`, `code_hash`, `module`,
  `source_file`, `distance`, `base_damage`, `out_damage`, `result`.
- Notifications identify `side`, `pid`, `session`, `generation`, `hash`,
  candidate/active generation, and cold restart severity.
- Generation agreement seed `PACKET_CODE_GENERATION` (client report + server
  announce) and a client mismatch warning. Each process keeps independent state.
- Dedicated server live-code lifecycle (journal + loader + poll + announce).

## Still required before round 2 is proven

- One cold build to install the EXE-owned mechanism changes (loader, journal,
  notifications, damage-policy call site, server lifecycle, packet).
- Running-server proof: `hot_damage_policy_result process=server
  generation=<current> code_hash=<current> base_damage=1520 out_damage=999999
  result=hot`, matched by a server `code_activation`, with stable PID/session/
  entity ids.

## Round 3 (2026-09-12, project layer + behaviors) — implemented source

Phases 0-3 primitives are implemented in `src/project/` and `src/live-code/`:

- Versioned project tree + content-addressed blob store + ChangeSet/
  ProjectVersion/ProjectHistory (record/undo/redo/checkpoint/restore/diff).
- `.d` dependency graph and state schema registry with package migrations.
- Shared authoring surface: `project` terminal command + loopback control server
  (`project serve [port]`) for agents.
- Glob-based hot sources (`hot-modules.json` `globs`) so live add/delete/rename
  of hot files changes the package source set and triggers a rebuild.
- `BehaviorBindingsComponent` (behavior referenced by id/hash, not duplicated).
- Kernel event queue + `emitEvent` capability: behaviors can emit nested events;
  `LiveBehavior::drainEvents` processes them FIFO after a dispatch.

Run `--project-selftest` for the primitives. Still requires one cold build to
install these EXE-owned mechanisms, then the live proofs.

## Round 4 (2026-09-12, phases 4-6 primitives) — implemented source

- **Phase 4:** per-source hot build with `-MMD` (per-TU `.d` in the generation
  `obj/` dir) then link; tree packages (`package.json`); content-addressed
  resources; subsystem replacement lifecycle
  (`initialize -> mirror -> validate -> switch -> retire`); runtime component
  schemas; recursive `ReadDirectoryChangesW` watcher wired into the loader;
  `ProjectControl` on the server.
- **Phase 5:** `CodeGenerationPacket` extended with `logicalCodeHash`,
  `platformPackageHash`, `moduleSetHash`, and `phase` (status/READY/SWITCH);
  clients report generation/hash + READY; servers announce switch generation and
  tick; per-player reports stored.
- **Phase 6:** capability model (grant/request/denial, not yet enforced at every
  call site); dependency hash verification (`Verified`/`NotLocal`/`HashMismatch`;
  download later); multi-domain scheduler (`gameplay.60`, `solver.600`);
  deterministic world hashing; agent control socket + terminal surface.

Run `--project-selftest` and `--phase456-selftest`. One cold build installs these
EXE-owned mechanisms.

Remaining after that cold build:
1. Behavior-table ABI (per-behavior id/hash) + per-entity binding dispatch.
2. Assets/shaders/configs in the project tree with generation-stamped loaders.
3. Run declared schema migrations at activation.
4. Full READY/switch-tick handshake with delayed activation at tick N.
5. Capability enforcement at call sites; agent auth on the control socket.
6. Dependency download/sandbox/promote pipeline.
7. Inspector for entities/components/bindings/generations/hashes.
8. Schema-driven components/events, hierarchical spaces, GPU/CPU compute
   capabilities, and the "arbitrary real-time domain" generality.

## Implemented (round 1)

- Generic event ABI: `GameEventV1`, `GameplayContextV1`, `DamagePolicyV1`,
  `GAME_EVENT_DAMAGE_POLICY`, `GameDamageSource` (`src/hot-reload/game-api.h`).
- `GameGameplayModuleV1` now exposes `onEvent`; the parameter-only
  `explosionParameters` bridge was removed.
- Kernel resolver `serverResolveDamagePolicy` + explicit unlimited dev safety
  limit (`src/network/server-damage-policy.*`); the silent `clamp(1,500)` was
  removed from `applyPlayerDamageLegacy`.
- Generic damage policy wired for: rockets/grenades and all explosions
  (`server-projectiles.cpp` player and NPC victims), player hitscan
  (`server-attack.cpp`), NPC hitscan (`server-npcs.cpp`), melee
  (`server-melee.cpp`), physical contact (`server-physical-contact.cpp`).
- Hot behavior `onEvent` in `src/hot-reload/modules/rocket-behavior.cpp`
  (identity by default; edit to return 999999).
- `--hot-authoritative-selftest` proves kernel dispatch -> hot behavior ->
  kernel apply, and that large values are not clamped. `--live-code-selftest`
  includes a policy-dispatch check.
- Periodic in-game cold-restart reminder (every ~10s) while a cold source
  change is pending (`cold_restart_pending`).

## Next: P5 actor/intent and P6 scale-out

1. **Behavior references per entity.** Add a `BehaviorBinding` component
   (`OnSpawn/OnTick/OnCollision` -> behavior id). The hot generation ships a
   behavior table by content hash; entities keep ids; activation swaps the
   table. Currently only one implicit damage-policy handler exists.
2. **Event queue + nested emit.** Implement the bounded `GAME_EVENT_*` queue in
   the kernel, FIFO and deterministic, with the `emitEvent` capability so a
   behavior can emit `DamageEvent`/`ImpulseEvent`/`DestroyEntityEvent` instead
   of only writing one payload.
3. **Component capabilities.** Add `readComponent`/`writeComponent`/
   `findEntities`/`spawnEntity`/`destroyEntity` to `GameplayContextV1` so
   behaviors can own full resolution (for example, enumeration and per-victim
   damage) rather than receiving a victim list.
4. **Explosion behavior ownership.** Move the splash formula fully into the hot
   explosion behavior using the capabilities above; the kernel only enumerates
   candidates and applies returned results.
5. **Determinism.** Replace `unordered_map` iteration with stable,
   deterministic ordering (sorted id iteration or insertion-ordered storage) in
   all systems that hash or replicate; define per-entity RNG streams. This is
   required before world hashing/replay.
6. **Converge the two ECS owners.** Make components canonical for player/NPC/
   projectile state and reduce `ServerPlayer`/`ServerNpc`/`ServerProjectile`
   to network/authority bookkeeping. Record adapters in `ecs-migration.md` with
   sync direction and deletion points.
7. **Performance.** Cache component lookups and behavior bindings per tick;
   avoid per-event allocation; keep dispatch branch-light. Budget the generic
   path so it does not regress the fixed 60 Hz loop.
8. **Schema-driven events/components.** Move event payload and component
   schemas to data so new event/component types no longer require a new EXE
   call site; keep versioning/bounds checks so hot code cannot corrupt kernel
   memory.
9. **Roll out further.** Move remaining gameplay policy behind the same
   boundary: grenade/frag specifics, weapon spread/ammo/reload policy, NPC
   weapon preference, gamemode scoring, respawn rules, area effects.
10. **Long-term.** Content-addressed behavior code, canonical component
    serialization, world/state hashing, deterministic replay, causal event
    history, distributed storage, and Blender/MiMITA live world editing. A
    bytecode/IR sandboxed evaluator can replace C++ DLLs behind the same
    capabilities; do not build it until the current slice needs it.

## Production safety

- `serverAuthoritativeDamageLimit()` is `0` (unlimited) on the private
  development branch. A public server must set a finite bound; this is the
  explicit, generic safety rule, separate from gameplay tuning.
- Untrusted client damage claims remain capped in `server-packet-handlers.cpp`.

## Remaining cold items

The generic boundary itself is installed (bootstrap cold build). New kernel
capabilities (event queue, component capabilities, schema registry) are cold
changes and are `HOT_RELOAD_BOUNDARY_VIOLATION` until installed. Gameplay
behavior in `src/hot-reload/modules/` stays hot.

## Human proof still required

1. Launch `mimita.exe`; enter the 1v1 vs one NPC; record PID/session/EntityIds
   and the active generation.
2. Fire a rocket; record authoritative NPC health loss (baseline).
3. Edit `src/hot-reload/modules/rocket-behavior.cpp` so a near/direct hit sets
   `outDamage = 999999`; save.
4. Confirm a new generation compiles and activates; fire again; authoritative
   NPC health loss reflects the hot value; client-visible result matches.
5. Confirm PID/session/EntityIds/world unchanged (no relink, no restart).
6. Revert the hot edit; confirm baseline damage returns after another
   activation.
7. Edit a `cold` file; confirm the periodic cold-restart notification and no
   relink.
