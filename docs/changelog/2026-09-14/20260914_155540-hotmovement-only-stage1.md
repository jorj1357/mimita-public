# Stage 1: hotmovement is the only movement (hot, no restart)

- UTC timestamp: 2026-09-14T15:55:40Z (EST 2026-09-14 11:55:40 EDT)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + live build); runtime human playtest pending

## What changed

Stage 1 makes the hot movement system the single local-player movement path and
fixes the reported movement/network bugs without relinking the EXE.

1. `src/hot-reload/modules/movement-system.cpp`
   - WASD is now camera-relative. The old code rotated the intent by `tf.yaw`
     even though `input-poll.cpp:136-139` already builds `frame.moveX/moveY` as
     camera-relative WORLD XY. The double rotation was removed.
   - The module is the only movement path: the `GAME_MODE_FLAG_LEGACY_MOVEMENT`
     early return is gone, so the built-in `physicsMainUpdate` step no longer
     runs. This keeps `player.movementSimulationTick` at 0, so the client reports
     the advancing `ctx.clientSimulationTick` instead of a frozen tick, which is
     what wedged the server at `stale-client-tick` after death.
   - Added a `MovementMode` table with `source` (default) and `default` ported
     from `config/movement/movement-source.json` and
     `config/movement/movement-default.json`; one mode changes the whole tuning
     set at once. Added the live `movementmode <source|default|index>` command.
   - Dash is additive, works grounded and airborne, edge-triggered, and falls
     back to camera-forward when no WASD is held. Freeze now stops horizontal
     movement only and leaves vertical motion active (spec §11).
2. `src/hot-reload/modules/editor-behavior.cpp` — removed the `hotmovement`
   command (there is no legacy movement path to opt into anymore).
3. `src/hot-reload/game-api.h` — removed the unused
   `GAME_MODE_FLAG_LEGACY_MOVEMENT` constant.

## Cold source staged for the Stage 2 seam (not built, running EXE unchanged)

These were written for B1 and remain on disk for the Stage 2 "actor tick +
animation" seam relink. They are dormant while the game runs:

- `src/sim/simulate-tick.cpp` — advances `movementSimulationTick` as the
  single owner.
- `src/physics/physics-mini.cpp`, `src/npc/npc.cpp` — removed the per-call
  increment (NPC tick advanced by its caller).
- `src/network/movement-validation.cpp` — a newer sequence with an older tick is
  a client clock reset, not a permanent `stale-client-tick` wedge.
- `tests/movement-network-integration-test.cpp` — expectation updated to match.

## Evidence

- Source: `g++ -fsyntax-only` clean for `movement-system.cpp` and
  `editor-behavior.cpp` (DLL flags). **PASS**.
- Build: `python devscripts/live-build.py` -> `DLL build success`
  (`build/hotreload/mimita-live-g000005.dll`). **PASS**.
- Cold build: `python build_agent.py` was not run; it refuses to relink a
  running `mimita.exe`, and Stage 1 does not require it. **NOT RUN (by design)**.
- Runtime: pending human playtest in the running game (movement, WASD-relative
  to camera, dash, mode switch, and no stuck position after death).

## Honest limitations

- **Animation is still static.** The only local-player animation call site is
  inside `physicsMainUpdate` (EXE), which hot movement skips. This cannot be
  fixed hot; it needs the Stage 2 animation seam.
- **Knockback external impulse is zeroed** by the hot override in
  `simulate-tick.cpp:123` (`sim.player->externalImpulse = 0`). The override
  capability does not carry external impulse. Stage 2 item.
- Movement formulas are temporarily implemented in the hot module rather than
  calling the shared kernel. Per the approved plan, Stage 2 adds
  `GameMoveStepFn` and routes hot movement through the single shared kernel.

## Next

- Stage 2 (single generic seam relink, or parallel `mimita-next.exe`): actor
  tick primitive, `GameMoveStepFn`, and `GAME_EVENT_ANIMATION_UPDATE`.
- Stage 3 (hot): port the animation state machine into
  `src/hot-reload/modules/animation-system.cpp`; route movement through the
  shared kernel; delete the interim in-module math.

## Files

Changed: `src/hot-reload/modules/movement-system.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`, `src/hot-reload/game-api.h`.
Staged cold (unbuilt): `src/sim/simulate-tick.cpp`,
`src/physics/physics-mini.cpp`, `src/npc/npc.cpp`,
`src/network/movement-validation.cpp`,
`tests/movement-network-integration-test.cpp`.

---

# Stage 2 (same session): generic capability seams + hot animation system

- UTC timestamp: 2026-09-14T16:21:47Z (EST 2026-09-14 12:21:47 EDT)
- Branch: `8292026stash`; commits: none; Result: `PASS` (builds + headless selftests)

## Architectural constraint honored

The mission is that ordinary gameplay stops needing new `GameAPI` fields. Before
adding anything, each proposed seam was checked against the existing registry
(System / EventTypeId+schemaHash / dynamic component / capability id / Resource /
Entity+Relationship). Outcome:

- **GameMoveStepFn → capability `physics.move`.** Not a context field; resolved
  by id through one new generic `resolveCapability` bridge. It is a low-level,
  policy-free capsule+world collision primitive, reusable by vehicles/swimming/
  climbing.
- **GameSpawnEffectFn → capability `effect.spawn`.** One generic descriptor
  (`kind` hash + numeric fields + color), so footsteps, dash, freeze, and future
  blood/sparks/smoke/debris/muzzle use the same mechanism. No feature-specific
  VFX ABI.
- **Animation → hot registered system + generic `skeleton.apply` capability.**
  `animation.main` registers in a new `postmovement.60` domain the kernel already
  times generically. A temporary `animation.update` bridge runs the existing
  animator until the pose state machine is ported hot. No `AnimationInputV1/
  OutputV1` became a fixed ABI slot.
- **`GAME_EVENT_ANIMATION_UPDATE` was NOT added** to the legacy event enum.
  Animation is a system/capability, not a fixed event.

Only ONE new context field exists: `GameplayContextV1::resolveCapability`, a
generic resolver. It replaces the need for every future per-concept field.

## What changed

- `src/hot-reload/game-api.h` (ABI 7→8): `GameResolveCapabilityFn`;
  `resolveCapability` context field; PODs `GamePhysicsMoveFn` (via
  `MovementStateV1`), `GameEffectSpawnV1`, `GamePosePartV1`/`GameSkeletonPoseV1`;
  capability id constants `physics.move`, `effect.spawn`, `skeleton.apply`,
  `animation.update`; domain `postmovement.60`.
- `src/live-code/live-behavior.cpp`: kernel providers `capResolveCapability`,
  `capPhysicsMove` (runs the shared built-in collision pipeline on the local
  player; capsule-only fallback when headless), `capEffectSpawn` (generic pooled
  effect path), `capSkeletonApply` (applies per-part pose to the skeleton),
  `capAnimationUpdate` (temporary bridge). `makeContext` wires the resolver.
- `src/hot-reload/generic-runtime.cpp`: the three primitive ids are declared as
  kernel-provided capabilities.
- `src/hot-reload/modules/animation-system.cpp` (new): hot `animation.main` in
  the post-movement domain; resolves the animation capability and runs it after
  every movement path.
- `src/hot-reload/modules/movement-system.cpp`: resolves `physics.move` for
  collision (correct capsule size + built-in sweep/step-up/floor recovery),
  emits `effect.dash`/`effect.downDash`/`effect.freeze`/`effect.freezeTrail`, and
  freeze now stops vertical velocity too.
- `src/sim/simulate-tick.cpp`: populates the local player `BodyComponent`
  (`PLAYER_RADIUS`/`PLAYER_HEIGHT`) so capsule size has one source; actor tick
  advancement (from Stage 1).

## Evidence

- Cold: `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` ->
  `BUILD SUCCESS` (never touched the running `mimita.exe`).
- Hot: `python devscripts/live-build.py` -> `DLL build success`
  (`build/hotreload/mimita-live-g000006.dll`); `build/mimita-game.dll` refreshed
  to ABI v8.
- `mimita-next.exe --live-code-selftest` -> `PASS`; package registers
  `animation.main` + `movement.main`.
- `mimita-next.exe --movement-selftest` -> `PASS` (8/8).
- `mimita-next.exe --movement-parity-selftest` -> `PASS` (7/7).
- Runtime human/multiplayer proof still pending.

## Honest limitations / next

- Animation pose logic is still the EXE implementation behind the
  `animation.update` bridge. The hot `animation.main` owns when/how it runs and
  the `skeleton.apply` primitive exists; porting the state machine + config
  parsing into the DLL is the next hot step (no further ABI change needed).
- `physics.move` operates on the local player; server/dedicated contexts fall
  back to the capsule solve.
- Knockback external impulse is still zeroed by the hot override in
  `simulate-tick.cpp`; carrying it needs a capability, next.

---

# Stage 3 (same session): fully hot capability providers + ABI freeze

- UTC timestamp: 2026-09-14T17:05:00Z (EST 2026-09-14 13:05:00 EDT)
- Result: `PASS` (builds + capability/live/parity/gamemode selftests)

## What changed

- `src/hot-reload/generic-runtime.h/.cpp`: one generic capability table now holds
  kernel and package providers with `{id, signatureId, schemaHash, callable,
  providerPackage, providerGeneration}`. Added `registerKernelCapability`,
  `capabilityInfo`, `capabilityProviderGeneration`; `activate()` takes a
  generation and validates requirements (missing provider, signature mismatch,
  schema mismatch, duplicate provider) before committing. The hardcoded
  `kernelProvidesCapability` id list was deleted.
- `src/live-code/live-behavior.cpp`: `capResolveCapability` is now a pure generic
  lookup; the four kernel primitives are registered as ordinary registry entries
  with signatures. No `if (id == ...)` routing remains.
- `src/hot-reload/game-api.h`: package ABI 1→2; `GameCapabilityRequirementV1`
  carries a signature; requirements array retyped. `GameplayContextV1` (the ABI)
  did NOT grow this pass.
- `src/hot-reload/hot-package.h`: `addCapabilityRequirement(id, sig, schema)` and
  a `CapabilityRequirementRegistrar`.
- New `src/hot-reload/modules/banana-provider.cpp` (provides `banana.launch`) and
  `banana-consumer.cpp` (requires + resolves + invokes it).
- New `src/hot-reload/capability-selftest.{h,cpp}` + `--capability-selftest`.
- New `docs/architecture/live-development/game-api-freeze-rule.md`.

## Evidence

- Cold: `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` ->
  `BUILD SUCCESS`.
- Hot: `python devscripts/live-build.py` -> generation 7 (15 sources);
  `build/mimita-game.dll` rebuilt (package ABI 2).
- `--capability-selftest` PASS (21/21): new id resolves; provider generation
  tracked; missing provider fails with rollback; signature mismatch fails with
  rollback; replacement changes the callable; duplicate providers rejected;
  package deactivate retires providers while kernel primitives survive.
- `--live-code-selftest` PASS; the real `mimita.core` package registers
  `banana.provider` (id `banana.launch`) and the `banana.consumer` requirement,
  proving requirement validation across hot modules.
- `--movement-parity-selftest` PASS; `--gamemode-hot-selftest` PASS.

## Limitations

- The coarse DLL means provider and consumer ship in one generation; the
  registry is nonetheless generation-scoped and re-resolved per activation.
  Consumers must not cache raw callables across a swap (documented).
- The interactive same-PID live edit sweep (A-J) is proven at the registry level
  plus real-package registration; the full interactive loop remains a human
  playtest.
- Permissions are declared through `Project::CapabilityRegistry` but not yet
  enforced at every call site.


---

# Stage 4 (same session): match lifecycle policy owned hot

- UTC timestamp: 2026-09-14T17:40:00Z; Result: PASS (builds + selftests)

## What changed

- `src/network/match-lifecycle.h` (new): POD policy payload for the runtime event
  id `gameHash("match.lifecycle")`. Not a GameAPI field.
- `src/live-code/live-behavior.cpp/.h`: `dispatchGameplayEvent64` dispatches an
  arbitrary 64-bit runtime event with a valid `GameplayContextV1` host, so
  domain-scoped hot handlers can resolve capabilities/dynamic components.
- `src/network/server-gamemode.cpp`: `dispatchMatchLifecyclePolicy` publishes
  authoritative defaults and applies handled out-fields; the lifecycle path is
  entered by `d.activeModeDomain != 0` (no FFA/TDM string switch).
- `src/hot-reload/modules/gamemodes/ffa.cpp`: owns FFA lifecycle policy.
- `src/hot-reload/modules/gamemodes/tdm.cpp` (new): registers the `tdm` runtime
  mode descriptor and owns TDM lifecycle policy through the same event.
- `src/hot-reload/hot-modules.json`: `match-lifecycle.h` watched for change
  detection.
- `--gamemode-hot-selftest` extended with lifecycle-ownership checks.

## Evidence

- `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` -> BUILD SUCCESS.
- `python devscripts/live-build.py` -> generation 8; `mimita-game.dll` rebuilt
  (19 sources).
- `--gamemode-hot-selftest` PASS (20/20) incl. FFA/TDM lifecycle ownership and
  inactive-mode isolation.
- `--live-code-selftest` PASS; `tdm.mode-tick` registered. `--capability-selftest`
  PASS (21/21).

## Limitations / next

- The phase-transition *mechanism* remains kernel; only the policy (durations +
  respawn) is hot. TDM team scoring still kernel-side.
- Next cold owner: projectile/weapon/tool policy.

---

# Stage 5 (same session): per-entity behavior bindings are live

- UTC timestamp: 2026-09-14T18:20:00Z; Result: PASS (builds + selftests)

## What changed

- `src/live-code/live-behavior.cpp/.h`: new
  `runBehaviorBindings(entity, eventType, payload, payloadSize, tick)` reads the
  entity's `BehaviorBindingsComponent` and emits the bound `behaviorId` runtime
  event. `dispatchToolUse` now prefers the equipped tool entity's
  `on.primary-use` binding over the global tool-hash router.
- `src/hot-reload/modules/tools/banana-launcher.cpp`: sets `handled = 1` and
  registers `banana.launcher.use` as a binding target.
- `src/network/hot-combat-selftest.cpp`: asserts a per-entity binding owns tool
  use.

## Why this removes a cold owner

`BehaviorBindingsComponent` existed but was never consumed: tool routing was a
global keyed table. Per-entity bindings are now the owner of a tool's use
(`on.primary-use -> behavior X`), which is the composition path the architecture
mission requires. No new GameAPI field, enum, or switch.

## Evidence

- `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` -> BUILD SUCCESS.
- `python devscripts/live-build.py` -> generation 9; `mimita-game.dll` rebuilt.
- `--hot-combat-selftest` PASS (9/9), incl. "per-entity behavior binding owns
  tool use".

## Limitations / next

- Tool-use and projectile-impact routers are now hot and behavior/entity driven.
- Still cold and next: authoritative projectile simulation/damage fully hot,
  hitscan/melee hot path, ammo/cooldown/equip component state, and
  Equipped/ContainedBy relationships replacing string-keyed weapon maps.

---

# Stage 6 (same session): hot projectile lifecycle end-to-end

- UTC timestamp: 2026-09-14T19:05:00Z; Result: PASS (builds + selftests)

## What changed

- `src/hot-reload/modules/tools/banana-projectiles.cpp` (new): `projectiles.60`
  hot system owns projectile integration/contact/lifetime and applies
  authoritative damage (`damage.apply`) and effects (`effect.spawn`).
- `src/hot-reload/modules/tools/banana-launcher.cpp`: composition-driven spawn
  (entity.create + dynamic projectile state + relationship); removed the kernel
  `projectile.spawn` call.
- `src/network/hot-combat-selftest.cpp`: asserts the hot path, not the kernel
  container, owns the spawned projectile.

## Evidence

- `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` -> BUILD SUCCESS.
- `python devscripts/live-build.py` -> generation 10; `mimita-game.dll` rebuilt
  (21 sources).
- `--hot-combat-selftest` PASS (16/16); `--live-code-selftest`,
  `--capability-selftest`, `--gamemode-hot-selftest`,
  `--movement-parity-selftest` all PASS.

## Limitations / next

- `server-projectiles.cpp` still simulates kernel-container projectiles for
  rockets/other weapons. Next: migrate remaining weapon types onto the hot
  entity projectile path and delete the kernel policy branch; then
  hitscan/melee; then ammo/cooldown/equip component state.
- Hot projectile world contact uses a generic ray; a generic sphere/contact
  query capability would replace the transform-proximity actor check.

---

# Stage 7 (same session): canonical hot projectile path for the rocket

- UTC timestamp: 2026-09-14T19:45:00Z; Result: PASS (builds + selftests)

## What changed

- `src/hot-reload/hot-projectile.h` (new): one `HotProjectileStateV1` layout +
  component id for all hot projectiles.
- `src/hot-reload/modules/tools/hot-projectiles.cpp` (new): single canonical
  `projectiles.60` system (integration/contact/lifetime/splash/effects).
  Deleted `banana-projectiles.cpp`.
- `src/hot-reload/modules/tools/rocket-tool.cpp`: rocket launcher spawns a
  composition-driven projectile entity; removed the kernel `projectile.spawn`
  call.
- `src/hot-reload/modules/tools/banana-launcher.cpp`: uses the shared state.
- `src/network/hot-combat-selftest.cpp`: asserts the rocket uses the canonical
  hot path and the kernel container stays empty.
- `src/hot-reload/hot-modules.json`: watch `hot-projectile.h`.

## Evidence

- `MIMITA_EXE_NAME=mimita-next.exe python build.py build-only` -> BUILD SUCCESS.
- `python devscripts/live-build.py` -> generation 11; `mimita-game.dll` rebuilt.
- `--hot-combat-selftest` PASS (16/16); `--live-code-selftest`,
  `--capability-selftest`, `--gamemode-hot-selftest`, `--movement-parity-selftest`
  all PASS.

## Limitations / next

- NPC rocket spawn (`server-npcs.cpp`) and other unmigrated weapons still use
  the kernel-container projectile; `server-projectiles.cpp` still owns their
  policy. Next: migrate remaining spawners, delete the kernel per-type branch,
  then hitscan/melee, then ammo/cooldown/equip component state + relationships,
  then remove the global keyed router.
