// 09 12 2026
/* purpose
* Track the remaining work for the Version 3 kernel/hot-gameplay migration.
* Record what is implemented and exactly what is next, in order.
* this file DOES NOT define gameplay formulas
* this file DOES NOT replace hot-kernel.md or the feature specification
*/

# Hot kernel migration: next steps

Status as of 2026-09-12 (session `20260912_153000` and follow-ups).
See `docs/architecture/live-development/hot-kernel.md` for the architecture.

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
