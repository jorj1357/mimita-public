# Rocket behavior behind the hot tool/projectile system

Time: 2026-09-24T19:31:20Z

## Scope

Moved rocket firing, NPC rocket use, projectile movement, collision, explosion,
damage, effects, sounds, and rocket logging behind the existing hot
tool/projectile system. After one cold bootstrap build, rocket edits compile into
the hot DLL only; the running EXE/world/players/NPCs/network session/entity IDs
stay alive.

## Changes

### Hot owners (canonical)

- `src/hot-reload/hot-projectile.h`
  - Append-only `detonated` / `explosionReason` / `fireSerial` /
    `weaponNetworkId` on `HotProjectileStateV1` plus reason constants.
- `src/hot-reload/modules/tools/hot-projectiles.cpp`
  - Single-detonation guard: an entity marked `detonated` is destroyed, never
    re-exploded.
  - `explode(...)` takes an explicit reason; broadcasts once; composes the local
    explosion once.
  - New `rocket.world_hit`, `rocket.actor_hit`, `rocket.explosion.before`,
    `rocket.explosion.after`, `rocket.destroy` records tagged with reason.
  - `rocket.tick.sample`: collects each rocket every tick and emits ONE
    consolidated record every 60 ticks (1 s at 60 Hz) with all sampled state.
- `src/hot-reload/modules/tools/rocket-tool.cpp`
  - `rocket.fire.request`, `rocket.fire.accepted`, `rocket.fire.rejected`,
    `rocket.spawn` records.
  - Hot cooldown/automatic-fire gate: a request while the tool state is cooling
    down or reloading is rejected, so `fire_delay`/`fire_mode` are hot-owned.
  - JSON * hot-C++ composability: named multipliers for speed/gravity/lifetime/
    radius/damage/splash/knockback, and a `worldHitMode` (explode/bounce/stop)
    with `maxBounceCount`/`bounceRestitution` support.
- `src/hot-reload/hot-projectile-event.h`
  - `hotBroadcastProjectileExplode(..., bool excludeOrigin)`: the origin process
    is excluded so it does not also apply its own broadcast (no double sound).

### Cold bridges (dispatch only, behavior stays hot)

- `src/combat/weapon-system.cpp` — player `fireRocketLauncher` now dispatches
  `ToolUsePolicyV1` and uses `WeaponRocketLauncher::fire` only when the hot
  router declines.
- `src/npc/npc-combat.cpp` — NPC `tryFire` projectile/rocket branch dispatches
  `ToolUsePolicyV1` and uses the cold launcher only when the hot router declines.
- `src/combat/weapon-rocket-launcher.{h,cpp}` — marked LEGACY cold compatibility
  fallback (comments only; no behavior change, nothing deleted).

## Validation

Static checks:

- All five canonical rocket hot files resolve via `modules/*.cpp` /
  `presentation/*.cpp` globs and are absent from `hot-modules.json` `cold`.
- `WeaponRocketLauncher::fire` remaining call sites are the two hot-gated cold
  fallbacks; `Ecs::spawnRocket` is only reachable from legacy/compat paths.

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T152930.exe
```

Automated tests (test evidence):

```text
--capability-selftest        PASS
--hot-combat-selftest        rocket checks PASS (canonical hot path, generic
                             PresentationState, hot impact policy); only the 25
                             known pre-existing animation/phase2 FAILs remain
                             (the pre-change build had 27)
--live-code-selftest         only the 4 pre-existing journal FAILs; log.event,
                             hot capability log, and cold-provider-route PASS
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-authoritative-selftest only the known pre-existing journal-evidence FAIL
```

Build evidence is separate from runtime evidence. The live checks from the task
plan (edit `weapons.json` speed, edit spawn offset, automatic fire delay, one
world-hit -> one event/sound/destroy, server alive) require a running session and
human observation and were NOT performed in this session.

## Honest boundary

- The live-edit and single-sound acceptance checks are pending human review.
- `rocket.fire.rejected` fires only when a tool entity exists and carries
  `ToolInstanceState`; a stateless fallback path may not log a rejection.
- The malformed-JSONL writer remains a separate cold infrastructure task (not
  edited here).
