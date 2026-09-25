# NPC lifecycle authority + single actor id allocator (fully-hot NPC Phase 3.5)

Time: 2026-09-24T18:46:36Z

## Scope

Phase 3.5 of the fully-hot NPC migration: make the hot `npc.lifecycle` provider
actually authoritative, seed generic origin/lifecycle at every birth path, and
unify the three NPC id allocators onto `EntityRegistry`. Phase 4 (generic actor
replication vertical slice) is next. Phases 5-6 remain deferred.

## Changes

- `src/live-code/live-behavior.cpp`
  - `GAME_CAP_NPC_LIFECYCLE` kernel entry is now registered `overridable=true`.
  - `capNpcLifecycle` is a dispatcher: it resolves
    `GenericRuntime::overrideCapability(GAME_CAP_NPC_LIFECYCLE)` per call and
    forwards to the hot package provider, otherwise runs the shared
    `HotNpcLifecycleImpl::evaluate` fallback. It logs the active provider
    generation on change. Previously the non-overridable kernel fallback shadowed
    the hot provider, so live edits to `npc-lifecycle-policy.cpp` had no effect.
- `src/ecs/entity-registry.{h,cpp}`
  - New `allocateLegacyId(realm, domain)`: the one typed-actor id allocator,
    backed by a per-`(realm, domain)` monotonic counter that skips live keys.
- `src/npc/npc.{h,cpp}`
  - `NpcSystem::nextNpcId()` now delegates to `EntityRegistry::allocateLegacyId`;
    the private `nextId = 100` counter is deleted.
- All server NPC id allocation routes through the registry:
  - startup `src/network/server.cpp`, listen-server startup same file;
  - manual `npc_spawn` and the `spawnNpcPressed` path in
    `src/network/server-packets.cpp`;
  - automatic reconciliation in `src/network/server-npcs.cpp`;
  - gamemode waves in `src/network/server-gamemode.cpp` (the local `100000`
    counter is deleted).
- Birth-path origin/lifecycle seeding (generic component authority):
  - `spawnWaveNpcs` seeds `GAME_NPC_ORIGIN_GAMEMODE` + `ActorLifecycleComponent`
    (previously defaulted to automatic and was eligible for reconcile deletion);
  - `serverRespawnAllActors` resets `ActorHealthState` + lifecycle component, not
    just the mirror;
  - listen-server startup and the `spawnNpcPressed` path seed
    origin/lifecycle at creation.
- `rebuildServerNpcMap` (`src/network/server-npcs.cpp`): deleted the legacy
  mirror-origin seed fallback; origin is read from `ActorOriginState` and seeded
  with the automatic default only for a body created outside the known paths.
- `src/hot-reload/hot-modules.json`: `server-npcs.cpp` legacy note updated.
- `src/hot-reload/modules/actor-net-state.cpp` (new, hot only): `actor.net-state`
  system that projects typed Transform/Velocity onto the versioned
  `ActorNetState` dynamic component for every server-realm NPC actor. This is the
  server half of Phase 4; generic replication now carries NPC position/velocity/
  aim/yaw/onGround/weapon with no NPC branch and no new packet type.

## Validation

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T183424.exe
```

Automated tests (test evidence):

```text
--npc-generic-slice-selftest  PASS
--npc-entity-selftest         PASS
--npc-actor-state-selftest    PASS
--dynamic-replication-selftest PASS
--dynamic-lifecycle-selftest  PASS
--actor-lifecycle-selftest    PASS
--capability-selftest         PASS
--gamemode-hot-selftest       PASS
--production-loop-selftest    PASS
--entity-slice-selftest       known pre-existing journal-evidence FAIL (reproduced
                              on mimita-20260924T181053.exe and T180948.exe)
```

Build evidence is separate from runtime evidence. The task's live acceptance
(edit `npc-lifecycle-policy.cpp`, swap the DLL, observe changed lifecycle in the
same running session) and the multiplayer two-client checks require a running
server and human observation and were NOT performed here.

Hot DLL build (hot module compile evidence, no EXE relink):

```text
python build_game_dll.py -> [HOT RELOAD] DLL build success: build\mimita-game.dll
```

## Honest boundary

- The hot lifecycle provider is now reachable through the overridable dispatcher;
  deterministic confirmation that the hot module (not the fallback) runs in a
  live session is still pending.
- Phase 4 is half done: the server-side `actor.net-state` projection is live and
  hot, but the client does not yet consume `ActorNetState` for remote NPC
  presentation; NPC position/aim/name/avatar still render from the compact
  `ENTITY_NPC` snapshot branch. Client binding + avatar-hash resolution + the
  parity probe are the remaining Phase 4 work.
- `ActorNetState` currently projects position/velocity/aim/yaw only; onGround,
  equippedSlot, and weaponState are left at defaults until the client consumer
  needs them (the compact snapshot still supplies them).
- The typed `Npc`/`NpcSystem` simulation remains the per-tick engine; Phases 5-6
  (hot behavior port + deletion of the typed path) are deferred.
- Unrelated pre-existing edits were preserved, not overwritten.

## Phase 4 completion: client binding + finished projection (same session)

### Changes

- `src/network/actor-state.{h,cpp}`: implemented the previously declared-only
  `actorStateWriteNetState`/`actorStateReadNetState`; added generic
  `ActorWeaponStateV1` (+ write/read) as the hot input for equippedSlot and the
  `NET_WEAPON_STATE_*` bits. Its schema is `GAME_NET_NONE` — `ActorNetState`
  remains the sole wire carrier.
- `src/network/server-npcs.cpp`: `rebuildServerNpcMap` writes the generic
  `ActorWeaponState` from the already-computed mirror slot/weapon bits.
- `src/hot-reload/modules/actor-net-state.cpp`: finished the projection —
  `onGround` from the generic movement runtime state, `equippedSlot`/`weaponState`
  from `ActorWeaponState`.
- `src/network/multiplayer-tick.cpp`: client binding. `mpApplyGenericNpcNetState`
  resolves the generic server entity (`EntityRegistry::find(Server, Npc, id)`),
  reads `ActorNetState`, and replaces the compact snapshot's movement/aim/ground/
  weapon fields before interpolation. This is the migration marker: present ->
  generic movement authority + rate-limited `[NPC GENERIC NET]` evidence; absent
  -> legacy compact movement. Membership/avatar/epoch still come from the compact
  snapshot, as the spec allows until full parity.
- `src/hot-reload/npc-generic-slice-selftest.cpp`: added deterministic
  round-trip checks for `ActorNetState` and `ActorWeaponState`.

### Validation

```text
Status: SUCCESS (incremental)  Executable: mimita-20260924T185829.exe
Hot DLL: build_game_dll.py -> up to date (rebuilt by the cold build)
--npc-generic-slice-selftest PASS (incl. net-state/weapon-state round-trips)
--npc-entity-selftest, --npc-actor-state-selftest, --dynamic-replication-selftest,
--dynamic-lifecycle-selftest, --actor-lifecycle-selftest, --capability-selftest,
--gamemode-hot-selftest, --production-loop-selftest  all PASS
```

Runtime/visual acceptance is NOT performed: two-client agreement on movement
from the generic path, and live DLL edit confirmation, still require a running
server and human observation. The compact `ENTITY_NPC` branch is intentionally
retained as the compatibility transport; retiring it requires proving parity in
a live session.

## Phase 5a: hot NPC movement/facing intent (same session)

### Changes

- `src/hot-reload/game-api.h`: new generic capability `npc.intent` /
  `sig.npc.intent.v1`, the `NpcIntentPolicyV1` POD (raw movement/action facts,
  facing context, in/out facing mode + RNG, out MovementIntent/AimIntent), and
  `GameNpcIntentFn`.
- `src/hot-reload/hot-npc-intent.h` (new): the shared stateless policy
  implementation (`HotNpcIntentImpl::evaluate`) moved verbatim from `npc.cpp`
  `buildInputState`: facing-mode timer, desired facing (aim target / travel /
  hold), turn-speed limiting, and the final yaw. RNG matches `random01`
  byte-for-byte so the sequence is preserved.
- `src/hot-reload/modules/npc-intent-policy.cpp` (new, hot): registers the
  `npc.intent` provider with a live-editable `kTurnSpeedMultiplier`.
- `src/npc/npc.cpp`: `buildInputState` now fills the request, dispatches to the
  hot provider (or the shared fallback), and applies the returned intent to the
  typed NPC + shared `InputState`. The old inline facing block and the obsolete
  `safePlanarNormal` helper are deleted.
- `src/hot-reload/hot-modules.json`: `hot-npc-intent.h` added to the header list.
- `src/hot-reload/npc-generic-slice-selftest.cpp`: deterministic checks for the
  intent policy (aim toggle, aim direction/yaw, movement passthrough,
  turn-speed limiting).

### Validation

```text
Status: SUCCESS  Executable: mimita-20260924T191400.exe ; hot DLL current
--npc-generic-slice-selftest PASS (incl. npc-intent checks)
--npc-entity-selftest, --npc-actor-state-selftest, --dynamic-replication-selftest,
--dynamic-lifecycle-selftest, --actor-lifecycle-selftest, --capability-selftest,
--gamemode-hot-selftest, --production-loop-selftest, --movement-parity-selftest
  all PASS
```

Live acceptance (edit `kTurnSpeedMultiplier`, swap the DLL, observe changed NPC
turn responsiveness in the same session) requires a running server and human
observation and was NOT performed. The provider is stateless and RNG-identical,
so the cold fallback and the hot provider produce the same sequence.

## Phase 5b: hot NPC target selection (same session)

### Changes

- `src/hot-reload/game-api.h`: new generic capability `npc.target-select` /
  `sig.npc.target-select.v1`, the `NpcTargetCandidateV1` + `NpcTargetSelectPolicyV1`
  PODs (cold-precomputed scored candidates; out chosen id/kind), and
  `GameNpcTargetSelectFn`.
- `src/hot-reload/hot-npc-target-select.h` (new): the shared stateless selection
  policy — scored aggregation, current-target stickiness, the anti-thrash switch
  threshold, and the legacy nearest-hostile fallback — moved from the cold
  `simulateSharedNpcs` selection branches.
- `src/hot-reload/modules/npc-target-select-policy.cpp` (new, hot): registers the
  `npc.target-select` provider with a live-editable `kSwitchThresholdMultiplier`.
- `src/network/server-npcs.cpp`: `simulateSharedNpcs` now enumerates hostile
  alive candidates (players + NPCs), precomputes each score via the existing hot
  `net.npc-targeting` score policy, calls `npc.target-select` (or the shared
  fallback), and maps the chosen id/kind back to the typed target. The generic
  `relationship.targets` override (written by the hot `npc-combat-ai` system)
  still wins, unchanged.
- `src/hot-reload/hot-modules.json`: `hot-npc-target-select.h` added.
- `src/hot-reload/npc-generic-slice-selftest.cpp`: deterministic checks (highest
  score, anti-thrash keep-current, legacy nearest).

### Validation

```text
Status: SUCCESS  Executable: mimita-20260924T191901.exe ; hot DLL current
--npc-generic-slice-selftest PASS (incl. target-select checks)
--npc-entity-selftest, --npc-actor-state-selftest, --dynamic-replication-selftest,
--dynamic-lifecycle-selftest, --actor-lifecycle-selftest, --capability-selftest,
--gamemode-hot-selftest, --production-loop-selftest, --movement-parity-selftest
  all PASS
```

Live acceptance (edit `kSwitchThresholdMultiplier`, swap the DLL, observe NPC
target changes in the same session) requires a running server and human
observation and was NOT performed.

## Phase 5c: hot NPC scored weapon selection (same session)

### Changes

- `src/hot-reload/game-api.h`: new generic capability `npc.weapon-select` /
  `sig.npc.weapon-select.v1`, the `NpcWeaponCandidateV1` +
  `NpcWeaponSelectPolicyV1` PODs (per-weapon effRange/damage/pellets/behavior
  type/usable; behavior weights; out chosen weapon hash), and
  `GameNpcWeaponSelectFn`.
- `src/hot-reload/hot-npc-weapon-select.h` (new): the shared stateless scored
  weapon-selection policy (range fit, damage utility, safety, anti-thrash switch
  threshold) moved from the `npc.cpp` scored branch.
- `src/hot-reload/modules/npc-weapon-select-policy.cpp` (new, hot): registers the
  `npc.weapon-select` provider with a live-editable
  `kWeaponSwitchThresholdMultiplier`.
- `src/npc/npc.cpp`: the `npc.behavior.active` weapon-selection branch now
  enumerates loadout candidates and calls the hot policy; the explicit
  force-weapon and legacy distance branches remain cold.
- `src/hot-reload/hot-modules.json`: `hot-npc-weapon-select.h` added.
- `src/hot-reload/npc-generic-slice-selftest.cpp`: deterministic checks (close-
  range burst wins; current kept within threshold).

### Validation

```text
Status: SUCCESS  Executable: mimita-20260924T193147.exe ; hot DLL current
--npc-generic-slice-selftest PASS (incl. weapon-select checks)
--npc-entity-selftest, --npc-actor-state-selftest, --dynamic-replication-selftest,
--dynamic-lifecycle-selftest, --actor-lifecycle-selftest, --capability-selftest,
--gamemode-hot-selftest, --production-loop-selftest, --movement-parity-selftest
  all PASS
```

Live acceptance and the remaining combat-mechanic moves (fire gate, reload
decision, damage response in `tryFire`) are not done; the firing mechanics stay
in the cold shared weapon system for now.

## Phases 5d/5e/5f: hot combat decision, AI state selection, navigation goal

### Changes

- `src/hot-reload/game-api.h`: three new generic capabilities with PODs —
  `npc.combat-decision` (`NpcCombatDecisionPolicyV1`), `npc.state-select`
  (`NpcStateSelectPolicyV1`), `npc.nav-goal` (`NpcNavGoalPolicyV1`), plus their
  function typedefs.
- `src/hot-reload/hot-npc-combat-decision.h` (new): 5d fire gate (cooldown,
  weapon presence, range cap, ammo/reload, line of sight) and fire-aggression
  blend. The RNG-consuming cooldown roll stays cold so the RNG order is
  unchanged.
- `src/hot-reload/hot-npc-state-select.h` (new): 5e state scoring, randomness,
  anti-thrash current-state penalty, and stuck/hit/no-target guards; the stuck
  timer + RNG ride in/out.
- `src/hot-reload/hot-npc-nav-goal.h` (new): 5f mapping from brain state +
  bounded memory to an abstract navigation goal (reach/follow/maintain/flee).
- `src/hot-reload/modules/npc-combat-decision-policy.cpp`,
  `npc-state-select-policy.cpp`, `npc-nav-goal-policy.cpp` (new, hot): providers
  with live-editable knobs (`kAggressionBias`, `kStateRandomnessScale`,
  `kMaintainDistanceScale`).
- `src/npc/npc-combat.cpp`: `tryFire` consults `npc.combat-decision`; the inline
  fire-gate chain and `computeFireAggression` are deleted. Firing mechanics stay.
- `src/npc/npc-state-machine.cpp`: `pickNextState` consults `npc.state-select`;
  the inline `scoreState` and its constants are deleted.
- `src/npc/npc.cpp`: `makeNavGoal` consults `npc.nav-goal`.
- `src/hot-reload/hot-modules.json`: three shared headers added.
- `src/hot-reload/npc-generic-slice-selftest.cpp`: deterministic checks for all
  three policies (combat gate/reload/cooldown; state search/retreat; nav reach/
  flee/maintain).

### Validation

```text
Status: SUCCESS  Executable: mimita-20260924T194735.exe ; hot DLL current
--npc-generic-slice-selftest PASS (incl. 5d/5e/5f policy checks)
--npc-entity-selftest, --npc-actor-state-selftest, --dynamic-replication-selftest,
--dynamic-lifecycle-selftest, --actor-lifecycle-selftest, --capability-selftest,
--gamemode-hot-selftest, --production-loop-selftest, --movement-parity-selftest
  all PASS
```

Live acceptance (edit any hot knob, swap the DLL, observe the changed decision in
the same session) requires a running server and human observation and was NOT
performed. Navigation world queries and the firing/damage mechanics stay cold by
design.
