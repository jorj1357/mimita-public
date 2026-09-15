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

## Round 23 (2026-09-15, hot pose generation via skeleton.apply) — source implemented, cold build pending

- New generic `PoseState` (`hot-pose.h`) + hot `hot.pose-generation`
  (`render.frame`, priority 2): generates idle/move/dead local bone poses from
  `AnimationState` and publishes them through `skeleton.apply`. The kernel stores
  the POD pose on the entity (`capSkeletonApply`); no DLL pointers retained.
- Evidence so far: live-build generation 18; `-fsyntax-only` clean for
  `live-behavior.cpp`, `hot-combat-selftest.cpp`. Cold link + `--hot-combat-selftest`
  PENDING (running `mimita.exe`; not killed). Run in the next no-process window.
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
