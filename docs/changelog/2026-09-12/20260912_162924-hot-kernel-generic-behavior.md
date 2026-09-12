# Hot kernel: generic authoritative event/behavior path (P0-P4)

- EST timestamp: 2026-09-12 12:29:24 EDT (UTC 2026-09-12T16:29:24Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Move toward the Version 3 model (Entity + Components + Events + Behaviors +
hot-swappable code) and make `src/hot-reload/modules/rocket-behavior.cpp` real
authoritative gameplay code, not a cosmetic/local-only layer.

## Root cause of the stuck 150

Authoritative rocket damage was computed and applied in the kernel:
`server-projectiles.cpp::explodeProjectile` used `projectile.splashDamage`
(JSON `rocketDirectDamage: 150`). The server path never dispatched to hot code.
Player victims were additionally silently clamped `std::clamp(damage,1,500)` in
`server-damage.cpp::applyPlayerDamageLegacy`, and replication used the
unclamped value.

## What changed

### Generic ABI (`src/hot-reload/game-api.h`)
- `GameEventV1 { typeId, payloadVersion, payloadSize, sourceEntity,
  targetEntity, projectileEntity, tick, payload }`.
- `GameplayContextV1 { host, tick, generation, codeHash, emitEvent,
  findEntities }`.
- `DamagePolicyV1` (base/out damage, knockback, source, entity ids, handled).
- `GAME_EVENT_DAMAGE_POLICY` and `GameDamageSource`.
- `GameGameplayModuleV1` now exposes `onEvent`; the parameter-only
  `explosionParameters` bridge was removed.

### Kernel
- `src/network/server-damage-policy.*`: `serverResolveDamagePolicy` dispatches
  the event, falls back to base when unhandled, and applies the explicit limit.
  `serverAuthoritativeDamageLimit()` is `0` (unlimited) on the dev branch.
- `src/network/server-damage.cpp`: replaced the silent `clamp(1,500)` with the
  explicit limit.
- Wired the generic policy before damage application for: explosion player and
  NPC victims (`server-projectiles.cpp`), player hitscan (`server-attack.cpp`),
  NPC hitscan (`server-npcs.cpp`), melee (`server-melee.cpp`), physical contact
  (`server-physical-contact.cpp`).
- `src/live-code/live-behavior.*`: EXE bridge for `GAME_EVENT_DAMAGE_POLICY`.

### Hot gameplay
- `src/hot-reload/modules/rocket-behavior.cpp`: `onEvent` owns the damage
  policy (`outDamage`, knockback); identity default.
- Removed the old `LiveGameplay::explosion` parameter path and its call sites.

### Cold-restart reminder
- `HotReloadSystem::pollColdBoundary` records the violation and, while a cold
  change is pending, emits an in-game notification every ~10 seconds
  (`LiveCodeEvents::notifyColdRestartPending`).

### Tests
- `--hot-authoritative-selftest`: kernel dispatches to the hot behavior, the
  kernel applies exactly the hot result, large values are not clamped, the dev
  limit is unlimited, and the journal evidence exists.
- `--live-code-selftest` now checks the generic damage-policy dispatch.

### Docs
- New `docs/architecture/live-development/hot-kernel.md` (kernel/hot table,
  event/behavior boundary, delegation rule, safety limits, cold-start reminder,
  private-dev-only detour/hot-patching note, long-term direction).
- New `docs/architecture/live-development/hot-kernel-next-steps.md`.
- Linked from `docs/architecture/ecs-entity-etc/ecs.md` and `ecs-migration.md`;
  `docs/ROUTER.md` updated.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS` (bootstrap cold build; no game
  running).
- `mimita.exe --live-code-selftest` -> PASS.
- `mimita.exe --hot-authoritative-selftest` -> PASS (7/7), including
  "kernel applies the hot policy result" and "large hot damage is not clamped".
- `mimita.exe --entity-slice-selftest` -> PASS.
- `python devscripts/live-build.py` -> emitted `mimita-live-g000003.dll`
  without writing `mimita.exe`.
- `python devscripts/test-live-build-invariant.py` -> PASS.
- Headless hot-edit proof of the authoritative resolver (temporary
  `outDamage = 999999`, then reverted):
  `hot_damage_policy_result ... base_damage:5 -> out_damage:999999 result:"hot"`.

## Authoritative data flow now

```text
weapons.json (base)
 -> kernel computes base damage
 -> kernel emits GAME_EVENT_DAMAGE_POLICY with base values + entity ids
 -> hot behavior sets handled + outDamage (rocket-behavior.cpp)
 -> kernel applies the returned value (explicit limit only)
 -> replication sends the applied damage
```

## What still needs work

See `docs/architecture/live-development/hot-kernel-next-steps.md`. Highlights:
per-entity behavior bindings and content-hash behavior tables; kernel event
queue + nested emit capability; component read/write/find/spawn capabilities;
full explosion ownership in the behavior; deterministic ordering (replace
`unordered_map` iteration) and RNG streams; converge the ECS/store vs server
structs; performance caching; schema-driven events/components; rollout to
remaining weapons/gamemodes; long-term bytecode/IR and world hashing/replay.

## Cold boundary

The generic boundary required one bootstrap cold build (installed). New kernel
capabilities remain cold changes (`HOT_RELOAD_BOUNDARY_VIOLATION`). Gameplay
behavior in `src/hot-reload/modules/` is hot.

## Human proof still required

Launch the cold-built `mimita.exe`, enter the 1v1 vs one NPC, fire a baseline
rocket, edit `rocket-behavior.cpp` to `outDamage = 999999` for explosions, save,
confirm live activation and authoritative NPC health loss, confirm same
PID/session/EntityIds, then revert and confirm baseline returns.

## Files

New: `src/network/server-damage-policy.h/.cpp`,
`src/live-code/live-behavior.h/.cpp`,
`src/live-code/live-authoritative-selftest.h/.cpp`,
`docs/architecture/live-development/hot-kernel.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

Changed: `src/hot-reload/game-api.h`,
`src/hot-reload/modules/rocket-behavior.cpp`, `src/hot-reload/hot-modules.json`,
`src/hot-reload/hot-reload-system.h/.cpp`, `src/live-code/live-gameplay.h/.cpp`,
`src/live-code/live-code-events.h/.cpp`, `src/live-code/live-code-selftest.cpp`,
`src/network/server-damage.cpp`, `src/network/server-projectiles.cpp`,
`src/network/server-attack.cpp`, `src/network/server-npcs.cpp`,
`src/network/server-melee.cpp`, `src/network/server-physical-contact.cpp`,
`src/network/multiplayer-projectiles.cpp`,
`src/combat/weapon-rocket-launcher.cpp`, `src/game/game-cli.cpp`,
`docs/architecture/ecs-entity-etc/ecs.md`,
`docs/architecture/ecs-entity-etc/ecs-migration.md`, `docs/ROUTER.md`.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
