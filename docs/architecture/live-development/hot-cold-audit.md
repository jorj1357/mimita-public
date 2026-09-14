// 09 14 2026
/* purpose
* Maintain the HOT / WARM / COLD map for major source areas and the criteria a
* subsystem must meet to count as HOT. Update this file every migration pass.
* this file DOES NOT define gameplay formulas
* this file DOES NOT replace hot-kernel.md or the live-code feature spec
* this file DOES NOT count a file as hot merely because it compiles into the DLL
*/

# Hot / warm / cold audit

Last updated: 2026-09-14 (generic dynamic-component + relationship replication:
a component type invented after startup is network-visible to clients without
the EXE knowing its type; generic damage across entity types; generic item
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
