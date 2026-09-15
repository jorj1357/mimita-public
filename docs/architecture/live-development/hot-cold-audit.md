// 09 14 2026
/* purpose
* Maintain the HOT / WARM / COLD map for major source areas and the criteria a
* subsystem must meet to count as HOT. Update this file every migration pass.
* this file DOES NOT define gameplay formulas
* this file DOES NOT replace hot-kernel.md or the live-code feature spec
* this file DOES NOT count a file as hot merely because it compiles into the DLL
*/

# Hot / warm / cold audit

Last updated: 2026-09-14 (authoritative gameplay.60 boundary with a hot
`npc.combat-ai` system owning target selection + authoritative attack via
generic capabilities; NPC team/role/behavior/target generic-authoritative; NPC
health generic + replicated + lifecycle; generic entity create/destroy and
dynamic-component/relationship replication; generic damage; generic item
containment/equip; generic action input routing).

Companion review: `docs/hot-warm-cold-review-09-14-2026.md` (full subsystem table).

## HOT criteria

A subsystem is HOT only when all of these hold:

1. editing its active implementation changes runtime behavior;
2. a new valid generation activates without an EXE relink;
3. persistent state survives activation;
4. a syntax/build failure preserves the last-good generation;
5. add/remove/rename of its source works;
6. no named cold policy owner still decides the actual behavior.

Compiling into `mimita-live-gNNNNNN.dll` is not sufficient.

## HOT (proven live or self-tested)

- Package/system/event/schema/command/capability registration (`GenericRuntime`).
- `movement.main` (default-on; legacy flag is an opt-out, not the owner).
- Editor / `modecreate` package behavior, selection, copy/paste, free-fly.
- `banana.*` falsification probes (new system/event/capability live).
- Rocket damage / explosion policy and actor decisions behind the event seam.
- Presentation module (damage numbers, rocket trail).
- **New 2026-09-14:** generic dynamic entity + component lifecycle:
  `entity.create`/`entity.destroy`, dynamic attach/read/write/remove,
  `component.enumerate`, `component.typesOnEntity`, `component.schema`,
  `relationship.add/remove/query`, activation-time schema migration with safe
  failure, and deterministic dynamic-component serialization/hash.
- **New 2026-09-14 (hot match phase ownership + TDM):** a hot mode that writes
  the generic `MatchPhaseOwnership` component on the match entity owns the phase
  clock; `serverGamemodeTick` skips its cold phase transitions for it. Hot
  `gamemode.tdm.cpp` owns the countdown->active transition (`match.setPhase`),
  team scoring from `actor.killed`, the score-limit win (`match.finish`), and its
  respawn policy (`match.lifecycle`). No mode enum/switch.
- **New 2026-09-14 (hot participant assignment + full TDM lifecycle):** the hot
  `gamemode.tdm.cpp` now owns participant team/role assignment and the complete
  round/match lifecycle: countdown -> active -> results -> intermission -> next
  round, with score limit, time limit, tie handling, win/end, and per-round
  score reset. Assignment writes generic `ActorTeamState`/`ActorRoleState`
  (deterministic over sorted EntityIds); the kernel now treats generic
  `ActorTeamState` as the source of truth in `serverMatchActorTeam` and projects
  it onto the typed `matchTeams`/participant roster (`projectGenericActorTeams`)
  for the scoreboard/broadcast. `serverMatchSetPhase` clears the previous
  match-over lock when a fresh round starts, so a hot mode can run multiple
  rounds through generic phase changes. No `match.assign-participants` slot was
  added: entity discovery + dynamic components + `match.setTeam`/`match.setPhase`
  already express assignment, per the live-runtime generic-bootstrap rule.
- **New 2026-09-14 (generic objective entities + events):** an objective is an
  entity + package-private dynamic components (`ObjectiveState`,
  `ObjectiveProgress`) + relationships (`objective.carried-by`,
  `objective.at-site`); the kernel never knows "bomb". Two hot runtime modes
  (`objective.carry`, `objective.hold`) are built from the SAME primitives:
  generic entity create, dynamic components, relationships, the generic
  `objective.interact` fact, and an emitted `objective.state-changed` fact.
  Completion feeds the generic match mechanism (`match.finish`). A generic
  `ObjectiveOwnership` component on the match entity bypasses the cold
  `updateObjectiveBomb`/`checkObjectiveRoundEnd` policy for hot-owned objectives.
  No `BombManager`, `ObjectiveType`, or ABI field. The cold CS `objective_rounds`
  bomb behavior is not yet replaced (see limits in the changelog).
- **New 2026-09-14 (shipping CS-like objective round migrated hot):** hot
  `gamemode.counterstrike.cpp` (runtime mode id `counterstrike`, matching the
  `counterstrike.json` gamemode) owns the real bomb/defuse round policy with
  generic primitives only: carrier assignment, plant/defuse/build-in timers,
  explosion, elimination/timeout/objective wins, and round reset. Real map bomb
  sites are projected by the kernel as generic `map.anchors`
  (`GAME_CAP_MAP_ANCHORS`) and become site entities; actors use the generic
  Transform component; interactions arrive as `objective.interact`; round
  outcomes feed the generic `match.round-result` capability (`GAME_CAP_MATCH_ROUND_RESULT`)
  and `match.finish`. The mode claims `ObjectiveOwnership`, so the cold
  `updateObjectiveBomb`/`checkObjectiveRoundEnd` are bypassed for it. A generic
  `match.round-start` event lets round modes reset hot; no BombManager,
  ObjectiveType, bomb capability, or ABI field.
- **New 2026-09-15 (generic actor spawn/reset + full CS phase ownership):** a
  generic `GAME_CAP_ACTOR_SPAWN` (`actor.spawn`) mechanism performs
  authoritative actor mutation (generic Transform/Velocity/health/dead plus the
  typed projections the client snapshot still reads); the mode decides where and
  when. `GAME_CAP_MAP_ANCHORS` now also projects map team spawn points as
  `spawn.team` anchors with a generic team tag. Hot `gamemode.counterstrike.cpp`
  now owns the COMPLETE phase cycle (countdown -> active -> results ->
  intermission -> next round), participant team assignment, and round-start
  spawn/reset, claiming `MatchPhaseOwnership` + `ObjectiveOwnership` so the cold
  phase machine and `beginMatchCountdown`/`resetGamemodeActorsAtMapSpawn` are
  bypassed for CS. Transform/Velocity are written as generic authoritative
  components (typed `ServerPlayer.pos/vel` remain projections). No
  `respawnCounterStrikePlayer`, `PlayerTransform`, or mode ABI field.
- **New 2026-09-14 (NPC hitscan/melee generic + action-handled gate):** hot
  `hitscan-tool`/`melee-tool` behaviors execute NPC hitscan/melee through the
  same generic tool/action path (kernel owns the ray/contact query; the behavior
  owns range/damage/occlusion/effect). All armed NPCs get an equipped tool
  entity. The hot action router records a generic `ActorActionState` when it
  handles an action; the cold `NpcCombat::tryFire` is bypassed from that generic
  gate (no weapon category, no `hasHotWeaponBehavior` API, `Npc.hotToolOwned`
  deleted). `tryFire` is now compatibility-fallback only.
- **New 2026-09-14 (cold NPC weapon owner removed):** NPC projectile weapons
  equip a generic tool entity (`contains-item`/`equips-item` + `ToolRefState`);
  hot `npc.combat-ai` emits the generic `tool.primary-use` action and the hot
  tool behavior spawns the canonical projectile. `NpcCombat::tryFire` is
  bypassed for those actors (one attack owner; tool-owned cooldown). Kernel fix:
  queued events dispatch with a capability context; schema registration survives
  store clear.
- **New 2026-09-14 (gameplay.60 boundary + hot NPC combat):** the authoritative
  server runs one `gameplay.60` domain execution per fixed tick; a hot
  `npc.combat-ai` system reads generic actor state, chooses a target
  (`relationship.targets`), and performs real authoritative damage through
  `damage.apply`. `serverResolveActorSpawnProfile` reads the generic
  `ActorRoleState`; the cold NPC target-selection branch now follows the
  generic relationship (bypassed for generic actors). A runtime monster-like
  entity uses the same path; no `MonsterType`.
- **New 2026-09-14 (NPC generic actor state):** NPC/actor team, role, movement
  preset, behavior profile, and current target are authoritative in generic
  dynamic components (`ActorTeamState`/`ActorRoleState`/`ActorProfileState`)
  and a `relationship.targets` edge; typed match/Npc fields are projections. A
  hot `npc.ai-state` system consumes the generic state generically (no
  `MonsterType`). Replicates through generic replication with no NPC packet.
- **New 2026-09-14 (NPC generic health + lifecycle):** NPC health is
  authoritative in the replicated dynamic `ActorHealthState` component on the
  NPC entity; `damage.apply` mutates it generically and emits a generic
  `actor.killed` death fact; `ServerNpc.health` and the typed `HealthComponent`
  are projections. NPC entities are marked for generic lifecycle replication
  (CREATE) at the single spawn boundary and destroyed through the generic
  DESTROY log. A runtime monster-like entity uses the same path, no
  `MonsterType`. Health replicates through generic dynamic-component
  replication (no NPC health packet).
- **New 2026-09-14 (generic entity lifecycle):** CREATE/DESTROY records in the
  same envelope, so a new entity created after startup becomes visible and can
  be destroyed on clients with no per-feature entity packet. `EntityRegistry::adopt`
  materializes the server's exact identity; retired ids reject stale state;
  duplicate CREATE/DESTROY and id reuse (generation) are safe. The real
  `HotProjectileState` entity uses generic lifecycle + component replication.
- **New 2026-09-14 (generic replication):** one opaque envelope (`PACKET_DYNAMIC_COMPONENT`)
  replicates schema descriptors, component upserts/removals, and relationship
  edges for ANY dynamic type invented after startup, selected by schema
  `networkPolicy` (`ALL`/`OWNER`/`NONE`/`SERVER_ONLY`), over the reliable event
  channel. Clients register unknown schemas, apply/remove state generically, and
  migrate on version change via registered migrations; mismatched payloads are
  rejected. `HotProjectileState` (a real gameplay component) uses the same path.
  No per-component packet, struct, encoder, or decoder.
- **New 2026-09-14 (generic damage + item containment):** `damage.apply` works
  on players, NPCs, and non-actor damageable entities through one capability.
  Items are owned by generic `contains-item`/`equips-item` relationships; the
  same item EntityId survives inventory->equip->drop->pickup->re-equip, with
  `on.equip`/`on.unequip`/`on.drop`/`on.pickup` behavior-binding dispatch.
- **New 2026-09-14 (generic action input):** primary client input can carry a
  generic runtime `toolId` (hash) instead of requiring a registered
  `NETWORK_WEAPON_*`/`WeaponRegistry` weapon. The server lazily creates/links an
  equipped tool entity (relationship + dynamic component) and dispatches the
  generic action; per-entity `BehaviorBindingsComponent` or the runtime behavior
  table owns the consequence. A tool created after startup is equipable and
  usable from real input with no weapon registration.
- **New 2026-09-14 (authoritative server context):** a generic
  `ServerContextV1` handle plus `projectile.spawn` and `damage.apply` capability
  primitives let hot behaviors mutate authoritative world state (spawn a
  simulated projectile entity, apply real damage) while the kernel keeps the
  players/projectiles containers and networking. Per-id capability resolver, no
  weapon/projectile callback and no new context fields. The real rocket's
  authoritative spawn is now owned by a hot tool behavior; a brand-new
  composition-driven projectile and a `projectiles.60` hot system run alongside.
- **New 2026-09-14 (combat policy):** generic tool-use
  (`tool.primary-use`/`tool.alt-use`) and projectile-impact
  (`projectile.impact`) runtime facts. A hot DLL router dispatches each fact to
  the runtime-registered behavior for its key; unregistered keys leave
  `handled = 0` so the cold path owns them. Real rocket impact policy and a
  brand-new `banana.launcher` tool/projectile behavior live behind it. Tools and
  projectiles compose from entities/dynamic state/relationships; no weapon enum
  or kernel switch.
- **New 2026-09-14 (gamemodes):** runtime-registered modes
  (`GameModeDescriptorV1`) with data-driven active-domain routing
  (mode id -> descriptor -> domain -> systems); generic authoritative match
  capabilities (`match.current`, `match.actorTeamRead`, `match.finish`,
  `match.setPhase`, `match.respawn`, `match.setTeam`); generic match facts
  (`actor.killed`, `match.evaluate`); real FFA **scoring and score-limit win**
  now owned by the hot `gamemodes/ffa.cpp` package with the legacy score packet
  fed from package dynamic state via the temporary `match.score.snapshot`
  bridge. A brand-new mode (`gamemode.hot-test`) is created after startup.

## WARM

- `src/hot-reload/modules/movement-system.cpp` (hot-owned but not bit-parity with
  the built-in step; dash/freeze/air-strafe simplified).
- `src/ecs/dynamic-components.*` (kernel mechanism; schema/migration now executed
  but not yet replicated or editor-editable).
- `src/project/*` (tree/watcher/dependency graph/state schema: primitives exist,
  package activation integration partial).
- `src/network/server-damage-policy.*` (policy partly hot).
- selected JSON configuration loaders.
- `src/live-code/live-editor.cpp` (inspection is kernel-formatted).

## COLD (policy owners still deciding behavior)

- Gamemodes (mostly migrated): FFA scoring + score-limit win are hot; **match
  lifecycle policy (countdown/go/intermission/results durations + respawn rule)
  is now hot** via the generic `match.lifecycle` event owned by the active mode
  domain; FFA and TDM own their lifecycle. **Still cold**: the phase-transition
  *mechanism* in `serverGamemodeTick`, TDM team scoring, duel/objective/wave
  rules, team/role assignment, mode-specific client UI, and the
  `GamemodeRegistry` JSON rule reload (`server-gamemode.cpp:1989-2042`).
- Note: `enum class ServerMode` is dead (write-only); the `matchMode` string is
  the routing key and is now bridged to runtime mode ids.
- Weapons/inventory/tools: `src/combat/*` (WARM): use/impact **policy** is hot
  for registered keys; the **revolver** ammo/cooldown/reload state is now
  component-authoritative on its tool entity (one migrated weapon; the rest stay
  on the string-keyed scratch map). Weapon-type selection (`projectileConfig`,
  `weaponExecutionTypeForBehavior`), other weapons' ammo/reload state, item spawn/containment,
  hitscan, and melee contact detection remain cold. `server-projectiles.cpp`
  spawn/sim/collision stay kernel mechanisms.
- Projectile spawn/simulation/collision/reconciliation: `server-projectiles.cpp`,
  `multiplayer-*`.
- NPC execution: `src/npc/*`.
- Ragdoll solver/bodies: `src/ragdoll/*`.
- Network batching/prediction/interpolation/lag simulation: `src/network/*`.
- Rendering, shaders, textures, GLB, audio, maps/world.
- Replay, persistence/backend.

## Highest-leverage remaining cold call sites

1. `server-projectiles.cpp` — projectile policy.
2. `src/gamemode/*` + `server-gamemode.*` — mode selection/score/win.
3. `src/combat/*` — weapon/tool composition.
4. `multiplayer-interpolation.cpp` / snapshot policy.
5. `src/ragdoll/*` solver policy.
6. Asset/resource generation-stamped providers.

## Update 2026-09-14 — match lifecycle policy moved hot

- Kernel publishes `match.lifecycle` (payload in `src/network/match-lifecycle.h`,
  runtime event id `gameHash("match.lifecycle")`) each tick for a hot mode.
- Active mode domain decides countdown/go/intermission/results durations and the
  respawn rule; `server-gamemode.cpp` applies handled out-fields. No mode-name
  switch enters the lifecycle path (`d.activeModeDomain != 0`).
- `gamemodes/ffa.cpp` and `gamemodes/tdm.cpp` own FFA/TDM lifecycle policy; TDM
  now registers a runtime mode descriptor (selection is descriptor/domain driven).
- New `LiveBehavior::dispatchGameplayEvent64` provides a valid context to
  domain-scoped handlers for arbitrary 64-bit event ids.
- Proof: `--gamemode-hot-selftest` PASS, including "active FFA owns
  match.lifecycle", "inactive mode does not own match.lifecycle", "TDM owns
  match.lifecycle through the same path".
- Next cold owner unchanged: projectile/weapon/tool policy
  (`server-projectiles.cpp`, `src/combat/*`).

## Update 2026-09-14 — per-entity behavior bindings are live

- `BehaviorBindingsComponent` was previously dead (only read by the inspector).
  It is now dispatched: `LiveBehavior::runBehaviorBindings(entity, eventType,
  payload, size, tick)` emits the bound `behaviorId` runtime event.
- Tool use now prefers the equipped tool entity's `on.primary-use` binding over
  the global tool-hash router (`dispatchToolUse` in `live-behavior.cpp`). A tool
  can own its use through a per-entity binding, not a weapon switch.
- `banana-launcher.cpp` registers `banana.launcher.use` as a binding target.
- Proof: `--hot-combat-selftest` PASS, including "per-entity behavior binding
  owns tool use".
- Still cold: authoritative projectile simulation/damage fully hot, hitscan/melee
  path, ammo/cooldown/equip component state, Equipped/ContainedBy relationships.
  Those are the next slice. The tool-use and projectile-impact routers themselves
  are now hot and entity/behavior driven.

## Update 2026-09-14 — hot projectile lifecycle (banana.launcher)

- New `hot-reload/modules/tools/banana-projectiles.cpp`: a `projectiles.60`
  system enumerates projectile entities by package dynamic component, integrates
  position/velocity/lifetime, performs low-level world contact through the
  generic `queryWorldRay` primitive and actor contact through generic
  health+transform queries, applies authoritative damage through the generic
  `damage.apply` capability, spawns impacts through `effect.spawn`, and destroys
  the entity. No projectile-type switch in the kernel.
- `tools/banana-launcher.cpp` is now composition-driven: on primary use it
  `entity.create`s a projectile entity and writes package projectile state +
  relationship, instead of allocating a kernel-container projectile. It no
  longer calls `projectile.spawn`.
- Proof: `--hot-combat-selftest` PASS, incl. "hot tool no longer spawns a
  kernel-container projectile", "hot tool spawned a composition-driven projectile
  entity", "hot projectiles.60 system simulates the projectile entity",
  "hot code applied real authoritative damage".
- Still cold: `server-projectiles.cpp` still simulates kernel-container
  projectiles used by rockets/other weapons; migrating every weapon type and
  deleting the kernel branch is the next slice. The hot path is proven for one
  tool end-to-end.

## Update 2026-09-14 — canonical hot projectile path for real weapons

- New `hot-reload/hot-projectile.h`: one shared `HotProjectileStateV1` layout +
  component id for every hot projectile weapon.
- New `hot-reload/modules/tools/hot-projectiles.cpp`: the single canonical
  `projectiles.60` system owns integration, world/actor contact, lifetime, splash
  damage, and effects for every hot projectile. `banana-projectiles.cpp` deleted
  (no parallel path).
- `tools/rocket-tool.cpp` migrated: the rocket launcher now spawns a
  composition-driven projectile entity (Transform/Velocity + HotProjectileState +
  ownership relationship) instead of calling the kernel `projectile.spawn`.
- `tools/banana-launcher.cpp` uses the same shared state/component.
- Proof: `--hot-combat-selftest` PASS, incl. "real rocket uses the canonical hot
  projectile path" and "hot projectiles.60 system simulates the projectile
  entity".
- Still cold: `server-projectiles.cpp` kernel-container sim remains for the NPC
  rocket path (`server-npcs.cpp` `Ecs::spawnRocket`) and any unmigrated weapon;
  the per-type explode branch still exists as the non-hot fallback. Next:
  migrate remaining spawners onto the hot entity path and delete the kernel
  policy branch; then hitscan/melee; then ammo/cooldown/equip component state.

## Update 2026-09-14 — grenade + request-path projectile attacks on canonical path

- `hot-projectile.h` gained generic bounce data (`restitution`, `maxBounces`,
  `bounces`, `HOT_PROJECTILE_BOUNCE_ON_WORLD`); the canonical `projectiles.60`
  system reflects velocity on world contact when set.
- New `tools/grenade-tool.cpp`: grenade launcher (network id 7) spawns the
  canonical projectile entity with bouncy fuse semantics (no new path).
- `server-attack.cpp` projectile request branch now routes through the hot tool
  seam (`LiveBehavior::dispatchToolUse`) with real origin/direction and the
  server player entity; handled + `outFire == 0` suppresses the kernel spawn.
  Rocket and grenade player attacks therefore use the canonical entity path, not
  the kernel container.
- Proof: `--hot-combat-selftest` PASS, incl. "grenade uses the canonical hot
  projectile path" and the generic fire-intent spawn check.
- NPC projectile firing now dispatches `ToolUsePolicyV1` with the NPC entity as
  `userEntity`; the hot rocket/grenade behavior creates the canonical entity
  and `projectiles.60` owns its simulation. `server-npcs.cpp` no longer creates
  a `ServerProjectile` or calls `Ecs::spawnRocket` for NPC firing.
- Still cold: legacy player projectile packet handlers/fallbacks in
  `server-projectiles.cpp`, client prediction/render compatibility paths, and
  old local `RocketLauncherState` ownership. Next: remove the remaining normal
  player/kernel producers and their per-type policy after replication parity.

## Update 2026-09-14 — legacy authoritative projectile execution disconnected

- Normal `AttackRequest` projectile fallback and held-fire projectile intent no
  longer call `handleGenericProjectileAttack`; missing hot behavior rejects
  instead of creating a second projectile architecture.
- Both server-loop calls to `tickServerProjectiles` were removed. No normal
  player or NPC producer now inserts an authoritative `ServerProjectile`.
- Remaining `server-projectiles.cpp` code is compatibility-only: the disabled
  legacy `PACKET_PROJECTILE_FIRE_REQUEST` body, its reject response, and dead
  low-level/container helpers. Client prediction/render compatibility still
  references `Ecs::spawnRocket`; the ECS slice selftest and local launcher do
  too. The file is not yet deletable because compatibility code remains
  compiled and the packet handler remains registered.

## Update 2026-09-14 — generic runtime state replication (components + relationships)

Replication stage map (server -> change detection -> serialization -> framing ->
transport -> client decode -> client state):

- Server authoritative state: `ServerPlayer`/`ServerNpc` structs (TYPE-SPECIFIC,
  COLD), `DynamicComponentStore` + `RelationshipStore` (GENERIC), entities
  (`EntityRegistry`, GENERIC).
- Change detection: `DynamicComponentStore::changeVersion` + removals
  (GENERIC); `RelationshipStore` per-edge `changeVersion` (GENERIC, new);
  players/NPCs have no dirty tracking (COLD).
- Serialization: one opaque `PACKET_DYNAMIC_COMPONENT` envelope
  (`dynamic-replication.*`, GENERIC) carrying schema descriptors, component
  upserts/removes, and relationship add/remove records. Player/NPC snapshots
  remain per-type wire structs (TYPE-SPECIFIC).
- Framing/transport: shared `PacketHeader` + reliable gameplay-event queue
  (GENERIC).
- Client decode: `mpTick` generic branch -> `dynamicReplicationDecode` ->
  `dynamicReplicationApply` (GENERIC, no type switch).
- Client state: `DynamicComponentStore` + `RelationshipStore` (GENERIC).

- New: generic relationship replication in the same envelope. A relationship
  type unknown to the EXE at startup becomes replicable the first time an edge
  is added (`RelationshipStore::add` sets policy `GAME_NET_ALL`), so
  `relationship.contains-item`, `relationship.equips-item`, and
  `relationship.fired-projectile` replicate with no cold registration or
  item-specific packet.
- Per-client sender now sends component **and** relationship upserts, detects
  removals by diffing its previously-sent set, `GAME_NET_OWNER` filters by source
  entity, and batches across multiple reliable packets so no changed record is
  silently dropped by truncation.
- `serverReplicateDynamicComponents` is now wired in the listen/host server tick
  as well as the dedicated server tick.
- `HotProjectileStateV1` is registered `GAME_NET_ALL`; the dynamic-replication
  selftest proves it flows through the generic path to a client store.
- Proof: `--dynamic-replication-selftest` PASS (component + relationship add,
  update, remove; schema distribution; payload-size rejection; v1->v2 migration;
  failed migration keeps last-good). `--hot-combat-selftest`,
  `--dynamic-lifecycle-selftest`, `--live-code-selftest`,
  `--capability-selftest`, `--gamemode-hot-selftest`,
  `--movement-parity-selftest`, `--hot-authoritative-selftest` PASS.
- Honest boundary: arbitrary runtime entities are not yet network-visible as
  entities; only their dynamic component and relationship records replicate
  (applied to the client store). Generic `ENTITY_CREATE`/`ENTITY_DESTROY`
  replication is still missing, as is live two-client network proof. No EXE
  rebuild or new per-feature packet was required.

## Update 2026-09-14 — generation-aware presentation + resource ownership

Presentation stage map (client state -> presentation selection -> traversal ->
render commands -> resource resolution -> GPU object -> draw):

- Client state: `Player`/`Npc`/`NetworkProjectile` structs (TYPE-SPECIFIC, COLD)
  plus `EntityRegistry` + `DynamicComponentStore` (GENERIC).
- Presentation selection ("what draws / representation / material"): hardcoded
  per-pass in `engineTickRender` and per-system `render()` (COLD); projectile
  `GAME_EVENT_PROJECTILE_PRESENT` (HOT policy seam).
- Render traversal / commands: `engine-tick-render.cpp` hardcoded sequence
  (COLD). New **HOT seam**: a generic `render.debug` capability lets a hot
  `render.frame` system submit lines / wire boxes / wire spheres / world labels;
  `capRenderDebug` (`live-behavior.cpp`) is the kernel mechanism, `DebugVis` the
  low-level draw. `LiveBehavior::flushRenderDebug()` flushes same-frame after the
  render domain.
- Resource resolution: `TextureStore`, `gMeshCache`, weapon/avatar caches,
  `gSoundFileCache`, font pages are per-owner, name/path-keyed, COLD, no content
  hash. New **GENERIC** `PresentationResourceProvider`
  (`src/project/presentation-resource.*`): logical id -> content hash -> immutable
  generation -> opaque handle, atomic swap, last-good on failure, immediate
  retire at the swap boundary.
- GPU object / draw: all low-level GL remains COLD (buffers, textures, shader
  compile, `glDraw*`) — intentionally the small mechanical kernel layer.

- New `GAME_CAP_RENDER_DEBUG` + `GameRenderDebugCommandV1` in `game-api.h`
  (append-only; no `GameplayContextV1` field). New hot module
  `modules/presentation/debug-presentation.cpp` registers a `render.frame`
  system and a `hotpresent` command; it presents any entity carrying the package
  `PresentationDebug` dynamic component. No Player/NPC/Projectile switch.
- New `Renderer::pollShaderReload()` routes `shaders/basic.*` through the
  provider: content-hash gated recompile, generation swap on success, last-good
  program preserved on compile/link failure. Called once per frame in
  `engineTickRender`.
- `hot-modules.json` globs gained `modules/presentation/*.cpp` (the discovery
  mechanism for the new module; no aggregator edit).
- Proof: `--hot-combat-selftest` PASS, incl. "render.debug capability resolves",
  "hot render.debug command reaches the kernel draw buffer", "hot render.frame
  system presents a generic entity", and the resource-provider generation / no-op
  / swap / last-good checks. Full suite PASS.
- Honest boundary: **no live visual proof was run** (no visible client in this
  session). The wireframe/shader live-edit falsification, texture/model loader
  wiring, and HUD hot composition remain. `PresentationDebug` state is
  local-only; it is not (yet) created by replicated generic entities.
- Concurrent-session blocker recorded, not redesigned: the other agent's new
  `src/network/npc-entity-selftest.cpp` was missing `ecs/relationship-store.h`
  and blocked the full build; one include was added.

## Update 2026-09-14 — generic presentation of runtime entities + real resources

- New generic `render.mesh` capability (`GAME_CAP_RENDER_MESH`,
  `GameRenderMeshCommandV1`; append-only, no context field). The command carries
  **logical** mesh/texture resource ids; the kernel resolves the current
  generation handle and draws. No Player/NPC/Projectile/weapon branch.
- New `PresentationState` hot-presentation schema (mesh/texture logical ids,
  flags, scale, color). New hot system `hot.presentation-mesh` presents any
  entity carrying it through `render.mesh`. `hot.debug-presentation` still
  presents wire shapes and now composes a **generic HUD panel** via
  `GAME_RENDER_DEBUG_HUD_TEXT` -> `uiDrawText`, so one UI composition path is
  hot (no gamemode switch).
- New cold generic presentation renderer `src/render/presentation-render.*`:
  real loaders for a procedural cube mesh and a PNG texture through the
  generation-aware `PresentationResourceProvider` (content hash, atomic swap,
  last-good on failure, retire at swap). `PresentationRender::poll()` re-applies
  changed file-backed resources each frame without recreating entities.
- `render.debug` gained `GAME_RENDER_DEBUG_HUD_TEXT`; hot modules resolve mesh
  and texture by logical id, never a raw GPU handle.
- Proof: `--hot-combat-selftest` PASS, incl. "render.mesh capability resolves",
  "hot render.frame presents a meshed generic entity via render.mesh", and the
  provider generation/no-op/swap/last-good checks. Full suite PASS.
- Honest boundary: **no live visual proof**; the procedural mesh + texture
  loader generation path is COMPILED INTEGRATION and provider semantics are
  SELFTEST PROVEN, but a real GL swap was not observed. GLB model loading,
  migrating a typed projectile render path, and replicated-entity-driven
  presentation remain. Replication of `PresentationState` exists (schema
  `GAME_NET_ALL`) but its end-to-end feed depends on the other agent's entity
  lifecycle replication.

## Update 2026-09-14 — real projectile presentation onto PresentationState

Projectile PRESENTATION audit (render-only; simulation named only as data
source):

| Caller | File:line | Class |
|---|---|---|
| Player rocket typed draw | `combat/weapon-system.cpp:639` -> `weapon-rocket-launcher.cpp:635` | A (real) + B (shares predicted rockets) |
| Grenade typed draw | `combat/weapon-system.cpp:647` | A (real, local-player gated) |
| NPC rocket typed draw | `npc/npc-combat.cpp:565` | A (real) |
| Network projectile typed draw | `network/multiplayer-projectiles.cpp:2175` | A (real) |
| Replay rocket typed draw | `engine-tick-render.cpp:415` | A (real replay) |
| `effect-part-render.cpp:383` `replay_rocket` | D (no producer) |
| `projectile-render.cpp:343` `clearProjectileMeshes` | D (no caller) |

- Server-side hot projectile entities (player/NPC rocket and grenade) now carry
  the shared generic `PresentationState` component with logical resource ids
  (`mesh.rocket`/`texture.rocket`, `mesh.grenade`/`texture.grenade`) plus a
  `Transform`. New shared hot header `hot-reload/hot-presentation.h`.
- `PresentationRender` now registers rocket (cylinder) and grenade (sphere)
  meshes and their textures through the same generation-aware provider, with
  per-frame content-hash polling.
- `hot.presentation-mesh` orients meshes along `Velocity` (falling back to
  `Transform.look`), so rocket/grenade orientation is generic data, not a
  renderer branch.
- Proof: `--hot-combat-selftest` PASS, incl. "rocket projectile carries generic
  PresentationState" and "grenade projectile carries generic PresentationState",
  plus the existing render.mesh and provider checks. Full suite PASS.
- Classification: the authoritative hot projectile path is now GENERIC (hot).
  The typed renderers (`weapon-system`, `npc-combat`, `multiplayer-projectiles`,
  replay) remain **compatibility-only** for the client's local predicted and
  legacy network/interpolated projectiles until the other agent's client entity
  lifecycle + EntityId mapping lets those entities carry `PresentationState`
  too. `projectile-render.cpp` is not yet deletable (it still serves those
  compatibility paths). `clearProjectileMeshes` and the `replay_rocket` branch
  are DEAD.
- Honest boundary: no live visual proof; resource swap while an entity is alive
  is provider SELFTEST PROVEN and COMPILED INTEGRATION but not visually observed.

## Update 2026-09-14 — client projectile presentation bridge removed

Client projectile identity map (from audit):

- Predicted: `provisionalProjectileId` = `0x80000000 | requestId`; lives in
  `MultiplayerContext::networkProjectiles`; `predicted=true`; reconciled to the
  authoritative `uint32 projectileId` via `fireSerial`/`requestId`.
- Network/authoritative: server-assigned `uint32 projectileId`
  (`server-projectiles.cpp`), held in `networkProjectiles`; no EntityId mapping
  before this pass.
- Replay: `gReplayRocketState` + `replayEventId` (`engine-tick-camera.cpp`).
- Generic lifecycle: `dynamicReplicationApply` uses `EntityRegistry::adopt` and
  the exact server packed EntityId; it replicates dynamic blobs/edges only, NOT
  typed Transform/Velocity.

- New `src/render/presentation-entities.*`: a client bridge that materializes a
  generic client entity (`ClientReplicated` + `Projectile` + projectile id) with
  typed `Transform`/`Velocity` and the `PresentationState` dynamic component for
  each live network/predicted projectile, updates it each frame, and retires it
  by mark/sweep when the projectile is gone/exploded/stale. Identity and lifetime
  are tied to the projectile id; presentation to the entity.
- `mpRenderNetworkProjectiles` now materializes the generic entity for migrated
  weapons (rocket/grenade) and **suppresses the typed `renderProjectile` draw**
  when the bridge owns the projectile (`has()` gate). The hot
  `hot.presentation-mesh` system draws it through `render.mesh`. This is a
  one-function, contained change; no prediction/interpolation math was touched.
- Double-draw: the typed path is skipped whenever the bridge has the projectile,
  so exactly one presentation owner. The one risk documented: if typed
  Transform/Velocity replication is later added for the server projectile entity,
  it would also draw; the bridge entity and the replicated entity must then be
  unified.
- Proof: `--hot-combat-selftest` PASS, incl. "projectile weapon maps to logical
  resources", "network projectile materializes a generic presentation entity",
  "generic projectile presentation submits a mesh", "generic presentation
  suppresses the typed projectile draw", "live projectile presentation persists
  across a frame", "terminated projectile presentation retires with the entity".
  Full suite PASS.
- Remaining typed owners: single-player `weapon-system.cpp` rocket/grenade and
  `npc-combat.cpp` (only populated when `mpContext` is inactive), and the replay
  rocket path. `projectile-render.cpp` remains a compatibility/fallback drawer
  for those; its DEAD members (`clearProjectileMeshes`, `replay_rocket` branch)
  are unchanged.
- Honest boundary: **no live two-client visual proof**. The bridge + generic draw
  + retire chain is SELFTEST PROVEN; the multiplayer visual handoff is COMPILED
  INTEGRATION awaiting a human run.

## Update 2026-09-14 — replicate projectile identity unified (authoritative EntityId)

Identity map:

- Server hot tool -> `entityCreate` (`createGeneric`, domain `None`, monotonic
  id) -> writes `HotProjectileState` + `PresentationState` dynamic components.
- Generic replication ships a CREATE with the packed EntityId (`adopt` on the
  client) plus the dynamic component blobs/edges. It does NOT ship typed
  Transform/Velocity.
- Legacy `networkProjectiles` (uint32 `projectileId`) is the prediction/legacy
  channel and carries no EntityId.

- New `PresentationEntities::projectReplicatedProjectiles()` (client, cold):
  enumerates `HotProjectileState` entities and projects `position`/`velocity`
  into typed Transform/Velocity on the **real replicated EntityId**. It never
  creates an entity; it skips entities the client has not adopted. The hot
  `hot.presentation-mesh` system then draws that same entity via the replicated
  `PresentationState`.
- `PresentationEntities::ensure()` is now used only for **predicted** (local)
  projectiles; remote/authoritative projectiles no longer get a parallel bridge
  entity. `PresentationEntities` is a prediction adapter, not an identity owner.
- Component ownership after: `HotProjectileState` = canonical replicated
  simulation state; `PresentationState` = canonical replicated presentation
  description; Transform/Velocity = client-side **projection** of the replicated
  state (temporary, acceptable); `networkProjectiles` = networking/prediction
  mechanism plus typed compatibility fields.
- Join-in-progress: projection needs only replicated state (no prediction
  history), so a late client materializes presentation from CREATE + components.
- Destroy/stale: `dynamicReplicationApply` DESTROY purges the entity shell and
  its components; projection skips non-alive entities; a stale component update
  cannot resurrect presentation because the shell is gone.
- Proof: `--hot-combat-selftest` PASS, incl. "replicated projectile projects onto
  the authoritative entity" and "authoritative replicated projectile presents via
  its own EntityId". Full suite PASS.
- Remaining: explicit predicted->authoritative **association** (a generic
  `PredictedEntityState`-style key) is NOT implemented. The provisional predicted
  bridge retires by mark/sweep when the predicted projectile ends, but for the
  local shooter the provisional predicted entity and the authoritative replicated
  entity can coexist transiently; unifying them needs the association key.
  Single-player and replay remain compatibility owners. No `ProjectileType`
  renderer switch or new game-api field was added.
- Honest boundary: no live two-client/visual proof.

## Update 2026-09-14 — generic predicted -> authoritative entity association

- New ECS-layer `PredictionRegistry` (`src/ecs/prediction-registry.{h,cpp}`): a
  type-agnostic map of prediction key -> {provisional entity, authoritative
  entity, status, tick}. It retires a provisional automatically once authority
  is known, handles authority-first, duplicates, destroyed provisional/authority,
  and stale keys, and never dangles (canonical falls back to the live side).
  No `RocketPredictionState`/`GrenadePredictionState`.
- New generic hot component `PredictionLink` (`hot-reload/hot-prediction.h`,
  `HotPredictionLinkV1.predictionKey`), registered `GAME_NET_ALL`. A prediction
  key links an authoritative entity back to the prediction that created it.
- Client: `PresentationEntities::ensurePredicted(key, ...)` materializes ONE
  provisional generic entity (Transform + Velocity + PresentationState +
  PredictionLink) and registers it with the registry;
  `PresentationEntities::associateByLink()` associates any entity carrying a
  PredictionLink (authoritative becomes canonical, provisional retires). Called
  once per render tick alongside `projectReplicatedProjectiles()`.
- `PresentationEntities` is now a thin projection/prediction helper; it does not
  own a normal-projectile identity namespace.
- `networkProjectiles` remains the prediction/interpolation/network mechanism
  only.
- Proof: `--hot-combat-selftest` PASS, incl. non-projectile (`WorldObject`)
  prediction-first, authority-first, destroyed-authority, stale-prune, and the
  client link association. Full suite PASS.
- **Missing integration hook (documented, owned by the server-combat agent):**
  the authoritative projectile entity must carry `PredictionLink.predictionKey`
  so a real client can associate its predicted provisional with the replicated
  authoritative entity. Concretely, `ToolUsePolicyV1` should carry a generic
  `predictionKey` (set from the attack request id) and the hot projectile tools
  should write `PredictionLink` on the created entity. Until that is added, the
  client association only fires for authoritative entities that carry a link;
  the local shooter can transiently draw both provisional and authoritative.
  This is a data/lifecycle addition, not a redesign, and adds no context field.
- Honest boundary: no live two-client/visual proof.

## Update 2026-09-14 — real prediction key transport (minimal server integration)

- `ToolUsePolicyV1` gained a generic `predictionKey` (append-only). No
  `GameplayContextV1` field, no weapon-specific field.
- `server-attack.cpp` populates it from `req->requestId` on the projectile
  request path (preliminary generic tool fact and the projectile branch) and from
  `held.intentId` on the held-fire paths — the exact correlation the client
  already uses. Non-predicted actions leave it 0.
- `tools/rocket-tool.cpp` and `tools/grenade-tool.cpp` write the generic
  `PredictionLink` dynamic component on the created authoritative entity when a
  key is present. Replication carries it through the existing generic
  dynamic-component envelope; no new packet.
- New generic non-projectile hot tool behavior `hot.predicted-test`
  (`modules/presentation/debug-presentation.cpp`) creates an arbitrary entity and
  writes `PredictionLink` from the action key, proving the server path has no
  projectile assumptions.
- Client handoff: the existing `PresentationEntities::associateByLink()` +
  `PredictionRegistry` resolve the replicated link; the authoritative EntityId
  becomes canonical and the provisional retires.
- Join-in-progress: a client that never predicted receives `PredictionLink` as an
  ordinary replicated component; it is harmless, and the authoritative entity
  presents via `projectReplicatedProjectiles()` without a registry entry.
- Proof: `--hot-combat-selftest` PASS, incl. "authoritative projectile carries the
  prediction key" and "non-projectile predicted entity carries PredictionLink".
  Full suite PASS.
- Classified redundant (not removed yet to avoid live-interpolation risk): the
  old `PresentationEntities::ensure()` bridge (superseded by `ensurePredicted`),
  and the write-only `predictedProjectileIds` set. `networkProjectiles`
  fireSerial/requestId reconciliation remains the interpolation mechanism.
- Honest boundary: no live two-client/visual proof. GLB loading not reached this
  pass.

## Update 2026-09-14 — GLB mesh loading through the resource provider

- `PresentationRender` now registers `mesh.demo.glb` as a GLB loader in the same
  `PresentationResourceProvider` (no GLB subsystem). The loader validates the
  GLB container (magic `glTF`, version 2, length) before parsing, then reuses the
  existing map loader (`loadGLB`, tinygltf) and uploads through the same
  `GpuMesh`/`uploadMesh` path as the procedural meshes. Retire uses the same
  `retireCubeMesh`.
- `PresentationState` still stores logical ids only (`meshResourceId`,
  `textureResourceId`); the renderer resolves the current generation handle at
  draw time. No VAO/VBO/GL id in hot/entity state. No entity recreation on swap.
- `PresentationRender::validateGlbFile(path)` exposes the validate stage
  headlessly.
- Proof: `--hot-combat-selftest` PASS, incl. "valid GLB container is accepted"
  and "malformed GLB is rejected (last-good preserved)". Full suite PASS.
- Old resource paths: `map/map_loader.cpp::loadGLB` and the weapon/avatar model
  caches remain (A low-level reusable parse / B compatibility consumers); no
  duplicate generation/reload ownership was added. `TextureStore`, `gMeshCache`,
  weapon/avatar caches still exist for their owners.
- Honest boundary: no live visual proof; GLB GPU upload/swap is COMPILED
  INTEGRATION; GLB content-hash polling is currently init-time only (not polled
  each frame). Hot HUD widget tree not reached this pass.

## Update 2026-09-14 — generic hot HUD/UI composition

HUD ownership audit before: composition/policy was COLD (`engineTickUI` ->
`engineTickUIHUD`/`engine-tick-ui-hud.cpp` -> `MatchTimer`/`MatchLeaderboard`);
the immediate-mode backend (`gui/ui-system.*`: `uiDrawText`/`uiDrawRect`/
`uiDrawImage`, font rasterization) is a reusable COLD mechanism.

- New generic `render.ui` capability (`GameUiCommandV1`: TEXT / PANEL / BAR /
  IMAGE with rect/color/value/scale/logical resource id). No gamemode UI type.
- New `GAME_DOMAIN_UI` (`ui.frame`) run once per frame in `engine-tick-ui.cpp`:
  `LiveUi::beginFrame` -> hot `ui.frame` systems -> `LiveUi::endFrameAndDraw`
  draws primitives through the cold UI backend. The engine only knows "run
  ui.frame".
- New hot module `modules/ui/hud.cpp` (`hot.match-hud`): composes the match
  timer, score panel, phase text, and a round bar from a generic replicated
  `MatchHudState` dynamic component (`hot-ui.h`). Layout/labels/formatting live
  in hot source; edits hot-swap live.
- Cold ownership transfer: the cold `MatchTimer` draw in
  `engine-tick-ui-hud.cpp` is gated on `!LiveUi::hotOwnsHud()`, so when a hot
  ui.frame system composes the HUD the cold composition yields (compatibility
  fallback, not an additional hook).
- Proof: `--hot-combat-selftest` PASS, incl. "render.ui capability resolves",
  "render.ui capability buffers a widget", "hot ui.frame fails safe without match
  HUD state", "hot ui.frame system emits HUD widget commands", "hot HUD
  composition is deterministic". Full suite PASS.
- Honest boundary: no live visual proof. `MatchHudState` is written by the mode
  package only once that integration lands (currently the component is
  registered and readable; the gamemode package does not yet populate it). UI
  IMAGE logical-resource resolution via the provider is not implemented (the
  command carries a path fallback). Stack/anchor/row are hot-side arithmetic, not
  kernel primitives. No gamemode-specific ABI/UI enum was added.

## Update 2026-09-15 — UI image resources + actor-like generic presentation

- `GAME_UI_IMAGE` now resolves a generation-aware logical resource id through
  `PresentationResourceProvider` (`LiveUi::resolveUiImageHandle`), drawing the
  current texture handle via a new cold `uiDrawTexture(GLuint, rect, color)`
  backend primitive. Path remains a fallback. Same resource-generation model as
  mesh/texture/shader; no UI-only hot-reload subsystem.
- Actor presentation: a typeless entity with Transform + Health +
  PresentationState (team color as data) is presented by the hot
  `hot.presentation-mesh` system via `render.mesh`. No Player/NPC/Monster type
  and no new kernel renderer category.
- Classified (not yet migrated): the typed actor renderers
  (`npc/npc.cpp::NpcSystem::render` -> `render-player.cpp`, `Player::render` /
  `renderCurrentPose`) remain the owners for local player and client NPC
  replicas; migrating them needs the client NPC replicas to carry
  PresentationState (analogous to the projectile bridge). Documented as the next
  presentation slice; not a second permanent owner.
- `MatchHudState` population by the mode package is still owned by the
  gamemode/server agent; the hot HUD consumer and cold-yield already exist.
- Proof: `--hot-combat-selftest` PASS, incl. "ui image resolves a
  generation-aware resource handle" and "typeless actor-like entity presents via
  render.mesh (team color as data)". Full suite PASS.
- Honest boundary: no live visual proof; no NPC/player renderer removal yet.

## Update 2026-09-15 — real remote-NPC presentation bridge (cold build pending)

- New `PresentationEntities::ensureActor/beginActorSync/endActorSync/
  actorMeshReady`: projects a real client NPC replica (interpolated pos/yaw) into
  a generic `ClientReplicated/Npc` entity with Transform + PresentationState
  (team color as data). Authoritative/network state stays the existing NPC
  snapshot/interpolation; Transform here is a projection, not a second
  simulation.
- `engine-tick-render.cpp` remote-NPC loop: when `actorMeshReady()` (the
  `mesh.actor` GLB loaded via the provider), the typed
  `renderNetworkPlayer`/remote-weapon draw is skipped for that NPC and the hot
  `hot.presentation-mesh` system draws the generic entity. Ownership gate yields
  to typed when the mesh is not ready, so an NPC is never invisible. Agent
  `mesh.actor` registered as the same GLB loader logical id (no NPC asset cache).
- Typed dependencies reclassified: `NpcSystem::render`/`render-player.cpp`/
  `Player::renderCurrentPose` are COMPATIBILITY fallback (local single-player
  NPCs and the not-ready path) and remain the animation bridge; the low-level GL
  stays a cold mechanism. No dead code deleted.
- Runtime monster-like entity proof retained (Transform + Health +
  PresentationState, no MonsterType).
- **Cold build blocked:** `mimita.exe` (pid 30680) was running during the pass, so
  `build_agent.py` correctly refused with `HOT_RELOAD_BOUNDARY_VIOLATION`; the
  running executable was not killed. Evidence so far: `live-build.py` generation
  17 DLL, and `-fsyntax-only` clean for every changed cold TU
  (`presentation-entities.cpp`, `engine-tick-render.cpp`, `engine-tick-ui.cpp`,
  `engine-tick-ui-hud.cpp`, `live-ui.cpp`, `live-behavior.cpp`,
  `hot-combat-selftest.cpp`). Cold link and `--hot-combat-selftest` for this
  pass are PENDING a no-process window.
- **VALIDATED (2026-09-15):** in a later no-process window `build_agent.py`
  returned `Status: SUCCESS`, and `--hot-combat-selftest` + the full suite (9
  selftests) PASS, including "real NPC replica projects to a generic presentation
  entity", "live actor presentation persists across a frame", and "untouched
  actor presentation retires with the entity". Remote-NPC generic presentation is
  now SELFTEST PROVEN.

## Update 2026-09-15 — generic hot animation policy

Animation ownership audit:
- movement/combat state -> animation selection: `Player::updateProceduralAnimation`
  / `player-animation.cpp` — animation POLICY (typed, COLD).
- playback/time/transitions: typed Player (COLD).
- pose generation + skeleton apply + draw: `Player::renderCurrentPose`
  (`entities/player-render.cpp`) — pose/skeleton (B) + cold GL mechanism (D).
- `animation.update` capability (`capAnimationUpdate`) is the existing
  temporary hot->typed bridge.

- New generic `AnimationState` (`hot-animation.h`,
  `HOT_ANIMATION_STATE_COMPONENT`, `GAME_NET_ALL`): `clipId`, `playbackTime`,
  `playbackRate`, `loop`, `flags`. Logical clip ids only.
- New hot module `modules/presentation/animation-policy.cpp`
  (`hot.animation-policy`, `render.frame`, priority 1): selects the clip from
  generic actor state (Velocity, Health) — idle / move / dead — and advances
  playback. This moves "which animation" into hot code for the migrated path.
- Cold mechanism untouched: skeleton decode, bone matrices, skinning, draw stay
  in the EXE.
- Proof: `--hot-combat-selftest` PASS, incl. "hot animation policy selects move
  for a moving actor", "... selects idle for a still actor", "... selects death
  for a dead actor". Full suite PASS.
- Remaining: attack/jump transitions; the typed `updateProceduralAnimation`
  still owns pose generation/skinning for the local player and client NPCs
  (compatibility + cold mechanism); animation clips/skeletons are not yet
  logical provider resources. No Player/Npc/Monster animation type and no new
  game-api field.

## Update 2026-09-15 — hot pose generation via skeleton.apply (cold build pending)

Pose path audit:
- clip selection/playback: `hot.animation-policy` (HOT, done last pass).
- `animation.update` -> `Player::updateProceduralAnimation` (E compatibility
  bridge / A+B typed pose).
- `Player::renderCurrentPose` -> skeleton/bone transforms (B/C), skinning + GPU
  upload + draw (D cold mechanism).

- New generic `PoseState` (`hot-pose.h`, POD bone offsets, GAME_NET_NONE) and hot
  `hot.pose-generation` (`render.frame`, priority 2). It reads `AnimationState`
  and generates idle/move/dead local bone poses, publishing them through the
  existing generic `skeleton.apply` capability. The kernel stores them as a POD
  `PoseState` on the entity (`capSkeletonApply`), so no pointer into DLL memory
  is retained; the local-player skeleton path is unchanged.
- `LiveBehavior::skeletonApplyCount()` is the headless evidence hook.
- No `applyNpcPose`/`applyPlayerAnimation` and no Player/Npc/Monster pose type.
- **Cold build blocked again:** `mimita.exe` was running, so `build_agent.py`
  refused (`HOT_RELOAD_BOUNDARY_VIOLATION`); not killed. Evidence so far:
  live-build generation 18 DLL; `-fsyntax-only` clean for `live-behavior.cpp` and
  `hot-combat-selftest.cpp`. Cold link + `--hot-combat-selftest` (animation
  policy + pose checks) are PENDING a no-process window.
- Remaining: `animation.update` is still used by the local-player/unmigrated
  typed path (B); retiring it needs hot pose to drive the real per-entity
  skeletons (GK scene skeletons), which is the next structural slice.
