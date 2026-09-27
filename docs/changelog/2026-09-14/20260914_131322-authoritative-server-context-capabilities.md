# Authoritative server-context capabilities: hot projectile spawn + damage

- EST timestamp: 2026-09-14 13:13:22 EDT (UTC 2026-09-14T17:13:22Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--hot-combat-selftest` + full suite)

## What this unblocks
Hot code could not mutate authoritative world state because the server keeps
`players`/`npcs`/`projectiles` as function locals. This pass adds a transient,
generic server-context handle plus two generic capability primitives so hot
behaviors can perform real authoritative actions without a weapon/projectile
callback or raw container access.

## Generic primitives added (no weapon-specific ABI)
- `ServerContextV1` (`network/server-context.{h,cpp}`): a transient handle to
  the live server state, installed for the server run. Hot code never sees the
  containers; it only gets capability ops.
- `GAME_CAP_PROJECTILE_SPAWN` (`projectile.spawn`) with a generic
  `GameProjectileSpawnSpecV1` (owner, runtime type key, pose/velocity, radius,
  lifetime, gravity/drag/restitution/bounce, explode flags, optional generic
  splash). The kernel owns id allocation, simulation, collision, and
  networking; returns a stable projectile entity id.
- `GAME_CAP_DAMAGE_APPLY` (`damage.apply`) with a generic
  `GameDamageApplyV1` (victim/source entity, amount, generic source kind,
  knockback; out applied/killed/healthAfter). Maps entities to actors and routes
  through the shared actor damage boundary; no weapon callbacks.
- `ServerProjectile` gained generic `genericMotion`/`genericTypeId` so a
  package projectile uses the shared physics path and carries its runtime type
  key to the generic impact policy. No projectile enum.

Resolved through the existing per-id capability resolver, so `game-api.h` gained
no context fields and no subsystem-specific module/callback.

## Real weapon migrated end-to-end
`modules/tools/rocket-tool.cpp` now owns the rocket: its tool behavior
suppresses the built-in spawn (`outFire = 0`) and authoritatively spawns the
rocket through `projectile.spawn` with generic area parameters. The existing hot
impact policy (`modules/tools/rocket-policy.cpp`) and hot damage policy then
apply. The cold `handleGenericProjectileAttack`/`projectileConfig` path no longer
runs for the rocket when the hot package is active.

## Behavior bindings / composition
`tool.primary-use`/`tool.alt-use` are now dispatched with origin/direction and a
runtime tool key; the DLL routes to the per-tool behavior table. The concurrently
developed composition path (entity + dynamic projectile state + `projectiles.60`
system) and per-entity behavior-binding dispatch are integrated and covered by
the same self-test.

## Evidence
- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --hot-combat-selftest` -> **PASS (16/16)** including:
  - hot tool use handled with origin/direction; a brand-new composition-driven
    projectile entity spawns with no kernel-container projectile;
  - the **real rocket spawn is owned by the hot tool path**
    (`projectile.spawn` creates a real authoritative projectile entity);
  - `damage.apply` resolves and **a hot behavior applied real authoritative
    damage** (victim health drops);
  - per-entity behavior binding owns the tool use; the `projectiles.60` system
    simulates the hot projectile; unknown tools/projectiles fall back to cold.
- Full self-test suite PASS (13/13): combat, gamemode-hot, dynamic-lifecycle,
  movement, movement-parity, entity-slice, hot-authoritative, live-code,
  project, phase456, telemetry, creation, ragdoll-slice.
- `-fsyntax-only` clean (real build flags) for all changed cold sources.

## Files changed
`src/hot-reload/game-api.h` (generic spec/result structs + capability ids, use
origin/direction), `src/network/server-context.{h,cpp}` (new),
`src/network/server-projectiles.cpp` (`serverSpawnGenericProjectile`, generic
impact entity/type, generic motion), `src/network/server-damage.cpp`
(`serverApplyEntityDamage`), `src/network/server-attack.cpp` (tool-use key +
origin/direction), `src/network/server.h` (`genericMotion`/`genericTypeId`),
`src/network/server.cpp` (context install), `src/live-code/live-behavior.cpp`
(kernel capability registration for the two primitives), `src/hot-reload/
modules/tools/rocket-tool.cpp` (new), `src/network/hot-combat-selftest.cpp`;
docs + this changelog.

## Honest limitations
- `damage.apply` supports player victims; NPC victims still need the NPC damage
  boundary migration.
- Item spawn/containment and ammo/reload as generic component state are not yet
  done; the rocket hot path bypasses the cold ammo/cooldown runtime.
- No live single-process or visual acceptance this pass.
- The tree is being concurrently edited by a gameplay agent; their banana
  hot-entity projectile path and behavior bindings are integrated and green.

## Next
- NPC damage boundary; generic input/action routing to the equipped entity;
  finish real `BehaviorBindingsComponent` dispatch; entity inventory/equip;
  dynamic component replication; fully hot projectile simulation.
