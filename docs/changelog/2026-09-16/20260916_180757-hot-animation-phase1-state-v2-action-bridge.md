# Hot animation ownership — phase 1: animation state v2, action-state bridge, legacy fallback gate

Date: 2026-09-16 14:07 EDT (UTC 2026-09-16T18:07:57Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_PENDING_COLD_BUILD`

## Task

Begin the fully hot-reloadable C++ animation migration. Phase 1 is the cold
mechanism pass: make the hot animation/pose path the single owner of visible
gameplay animation, publish generic action facts, version the replicated
animation state, and keep the legacy procedural animator as a gated fallback.

## What changed

### 1. Generic action-state component (`src/hot-reload/hot-action.h`, new)
- `HOT_ACTOR_ACTION_COMPONENT = gameHash("ActorActionState")`, POD
  `HotActorActionStateV1` with explicit `version` + `byteSize`.
- Generic facts only (flags, weaponKey, lifecycleGeneration, meleeAction,
  equip/reload/fire/shoot timers, speed, ammo, isReloading). No STL/pointers.
- Logical action ids (`HOT_ACTION_IDLE/WALK/JUMP/FALL/LAND/DASH/DOWN_DASH/
  FREEZE/EQUIP/EQUIPPED_IDLE/SHOOT/RELOAD/SLASH/LUNGE/HURT/DEATH/RESPAWN`).
- Added to `src/hot-reload/hot-modules.json` headers.

### 2. `AnimationState.v2` + migration (`src/hot-reload/hot-animation.h`)
- Kept `HotAnimationStateV1` unchanged (not enlarged).
- Added `HotAnimationStateV2 { version, byteSize, actionId, weaponKey,
  sourceEventSeq, loop, actionPhase, lifecycleGeneration, flags,
  playbackTime, playbackRate, blendWeight, blendDuration }`.
- `animation-policy.cpp` now registers `AnimationState.v2`, the generic
  `ActorActionState.v1` schema, and a deterministic `v1 -> v2` migration via a
  new `MimitaHotPackage::MigrationRegistrar` (`hot-package.h`). The migration
  defaults every new field; `GenericRuntime::activate` routes it into
  `DynamicComponentStore` before `applySchemaUpdate`.
- Updated writers/readers: `pose-generation.cpp`, `debug-presentation.cpp`,
  cold seed in `presentation-entities.cpp`, `hot-combat-selftest.cpp`.

### 3. Cold action-state bridge (`src/render/presentation-entities.cpp`)
- `writeActionState(EntityId, const Player&)` publishes generic facts from
  existing cold state (network weapon-state bits, grounded, dead, spawn
  generation, equipped tool hash, weapon runtime timers) onto the actor
  EntityId. Bridge only; it never selects an animation.
- Called from `projectLocalPlayer` and `projectActorOverlayState`.

### 4. Legacy procedural animator gated (fallback only)
- New central switch `gHotAnimationOwnsGameplay` (default true) in `player.h`,
  defined in `player-config.cpp`, toggled by the `hotanim <0|1>` terminal
  command (`debug-commands.cpp`).
- Gated call sites: `physics-mini.cpp`, `client.cpp`,
  `multiplayer-interpolation.cpp`, `live-behavior.cpp::capAnimationUpdate`.
- `LiveBehavior::animationUpdateCount()` added as fallback-invocation evidence.
- Avatar-preview menu (`menu-avatar-preview.cpp`) intentionally left on the
  legacy animator for phase 1 (standalone preview, not a live gameplay actor);
  it is now the only remaining reader of `config/animations.json`. Tracked as
  debt.

### 5. `skeleton.validate` kernel primitive (`game-api.h`, `live-behavior.cpp`)
- `GAME_CAP_SKELETON_VALIDATE` + `GameSkeletonValidateV1` (required part hashes
  in, present mask/missing count/valid out). Registered via
  `registerKernelCapability`; no new `GameplayContextV1` field. Mechanism for
  phase 3 model-swap validation and candidate self-tests.

## Pre-existing changes (not mine)

`src/config/movement-config.h`, `src/hot-reload/hot-movement-policy.h`,
`src/hot-reload/modules/movement-system.cpp` were already modified in the
worktree before this session and were not touched.

## Evidence

- Source: listed files only.
- Hot DLL build: `python build_game_dll.py` -> `DLL build success:
  build\mimita-game.dll` (53 sources).
- Cold compile check: `g++ -fsyntax-only` (project flags + PCH) on all changed
  cold TUs (`presentation-entities.cpp`, `player-config.cpp`,
  `live-behavior.cpp`, `debug-commands.cpp`, `physics-mini.cpp`, `client.cpp`,
  `multiplayer-interpolation.cpp`, `hot-combat-selftest.cpp`) -> exit 0.
- Cold EXE build (`python build_agent.py`): NOT RUN.
  `HOT_RELOAD_BOUNDARY_VIOLATION` is guaranteed: two `mimita.exe` processes are
  running (PIDs 20660, 23824). Relinking a running EXE is forbidden by the
  invariant, so the cold build must wait until those instances are closed.
- Runtime/visual/human acceptance: NOT performed.

## Selftest assertions added (pending run at cold build)

`--hot-combat-selftest`:
- policy selects walk/idle/death for the generic actor;
- `AnimationState.v2` carries explicit version + byte size;
- `AnimationState` schema version is 2 and the v1 -> v2 migration is registered;
- legacy animation bridge invocation count is 0 under hot ownership;
- cold action-state bridge publishes reloading flag, reload state, lifecycle
  generation and equipped tool hash on an actor EntityId.

## Blockers / follow-up

1. Cold build required before any runtime proof; blocked on the two running
   `mimita.exe` instances.
2. Then run `--hot-combat-selftest` and `--live-code-selftest`.
3. Avatar-preview JSON reader to be removed in phase 2/3.
4. Remote actors still carry only `networkWeaponState` bits (no timers); the
   server-side generic action-state bridge is a later item.

## Classification

- COMPILED: hot DLL + all changed cold TUs (syntax-only).
- NOT RUN: cold link, selftests, runtime, visual, human acceptance.
