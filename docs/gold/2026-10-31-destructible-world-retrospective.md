# Destructible World Retrospective — 2026-10-31

**Status:** retrospective / decision record
**Decision:** world-geometry destruction is disabled by default
**Primary spec:** `docs/specs/destructible-world/destructible-world.md` section 72
**Implementation changelog:** `docs/changelog/2026-10-31/20261031_043037-destructible-world-physical-impact.md`
**Priority record:** `docs/specs/open-gameplay-issues.md` — “New intake — 2026-10-31”

## 1. Why this record exists

This is a gold reference for a project lesson, not a new specification. The
feature worked technically, was taken far enough to evaluate in real TDM play,
and was then deliberately turned off. The lesson is that technical success is
not the same as a good default game feel.

The intended outcome of this record is:

- preserve what was learned,
- keep the working code and evidence as a starting point,
- prevent someone from re-enabling map destruction without the missing pieces,
- avoid treating the decision to disable it as a failure.

## 2. What was built

The 2026-10-31 slice implemented:

- one persistent world-impact entry point for hitscan, projectile, melee,
  explosion, player, and NPC sources;
- exact imported map-solid identity and sparse map-node activation;
- authored-map client hide/restore and collision hiding without full-map
  rebuilds;
- shared Boolean cuts and physical fragments;
- cut-cause replication and TDM/Juggernaut match-boundary reset;
- radial explosion impacts with falloff, occlusion, and a node cap;
- physical weapon profiles (mass/density/size/speed/penetration) in the weapon
  master config;
- diagnostics and performance work:
  - cached/prewarmed GLB parse,
  - activation budget and deferred queue,
  - network receive budget,
  - off-thread client cut geometry,
  - incremental render/collision hiding.

## 3. Evidence used

Source/build/self-test:

- build `3878` for the main runtime evidence,
- later performance builds `3964`, `3971`, `3974`,
- `--destructible-selftest` PASS,
- `--destruction-stress-selftest` PASS.

Runtime journals:

```text
logs/2026-10-31/20261031_041931/events.jsonl   TDM/Juggernaut Dust2, ATDM, reset
logs/2026-10-31/20261031_142529/events.jsonl   incremental map hiding / timing
logs/2026-10-31/20261031_143828/events.jsonl   async client cuts / frame tail
```

Observed highlights:

- TDM Dust2 activated map solids, accepted concrete cuts, fired radial
  explosions, and restored `63` nodes at the match boundary.
- Juggernaut Dust2 restored `7` nodes at a round boundary.
- ATDM activated its own solids, proving the map path was not Dust2-specific.
- Foundation nodes activated but did not cut under the observed ~5062 J rocket
  blasts; concrete cut normally.
- Client map hide/restore measured ~0.003-0.05 ms.
- Server first activation after prewarm measured ~0.16-0.30 ms.
- Client async cut install measured ~0.008-0.18 ms; Boolean work moved
  off-thread.
- TDM debug run reached ~300+ FPS average, but occasional 4.5-9.8 ms frames
  remained. The strict zero-dip 240 FPS target was not met.

## 4. What went well

- The shared entry point avoided per-weapon wall-damage paths.
- Authoritative causes + deterministic clients converged without mesh transfer.
- Sparse activation kept hundreds of map solids from becoming always-active
  physics bodies.
- The map was not special-cased to Dust2.
- The performance work removed the worst activation, server-flush, and
  client-cut stalls.
- The reset lifecycle worked for TDM and Juggernaut.

## 5. What broke the experience

- Floors and slopes could be destroyed. The walkable world became inconsistent:
  the player could not reliably tell what was safe to step on.
- The automatic material heuristic only recognized thin horizontal slabs as
  foundation. Slopes fell through to concrete.
- Map Editor material assignment was not finished, so there was no practical
  way to mark ground/slopes before play.
- NPC hitscan used a local weapon-fire path that did not submit world impacts;
  visible NPC shots could enter a wall without cutting it.
- NPC collision/navigation used a separate snapshot and did not see holes while
  the expensive rebuild was disabled.
- Small floating pieces still appeared in some cases.
- The residual frame tail remained outside the optimized destruction path.

## 6. Root causes

| Symptom | Root cause | Evidence |
|---|---|---|
| Floors/slopes cut | material heuristic only tags thin horizontal slabs; slopes default to concrete | `src/network/server-world-destruction.cpp` older heuristic; section 72.4-B |
| No way to mark ground | Map Editor material UI/save path not finished; only `glb_objects[].material` data layer exists | section 72.4-A; `entity-editor.md` |
| Concrete tuning confusing | actual cut path uses `holeEnergyScale`/`maxCutRadius`/`cutThresholdJ`; `cutResistance`/`surfaceHardness`/`fractureThreshold` are parsed but unused | `src/impact/impact-system.cpp`; section 72.4-C |
| NPC hitscan misses world | `NpcCombat::tryFire` uses `WeaponFire::tryFireHitscanDir`, not the server `ImpactSystem` adapter | `src/npc/npc-combat.cpp`; section 72.4-E |
| NPCs ignore holes | `npcWorld` snapshot; `npcCollisionRebuildOnWorldCut` disabled for cost | section 72.4-F |
| Small floating pieces | fragment bounds threshold and cleanup policy | `config/destructible-world.json`; section 72.4-D |
| Frame tail | general engine/render/config-poll/replay/worker contention | 2026-10-31 changelog; section 72.4-H |

## 7. Lesson

> A destruction system can be correct, replicated, resettable, and still be the
> wrong default for a competitive mode if it makes the walkable world unclear.

The correct sequence is:

1. prove the engine capability,
2. prove the mode's play fantasy,
3. make the walkable surface readable and stable,
4. only then make it the default.

Making it work was valuable. Learning that it was not fun in TDM was also
valuable. Both are true.

## 8. Decision

- `config/destructible-world.json` `world.enabled: false`.
- TDM/Juggernaut/Jugger Horde inherit with `destructible_world: null`.
- Static authored map geometry is the default.
- The switch affects map geometry only; crates/props are separate.
- Destructible world is **not a priority** for TDM now.

## 9. Re-enable gate

Do not flip the global switch back on until section 72.4 is satisfied:

1. Map Editor material assignment and walkable floor/slope marking.
2. Floor/slope classification using the shared `walkableSlopeDot`.
3. Foundation kept finite and extremely strong (`cutThresholdJ 1.0e16`).
4. Small-fragment threshold set and verified.
5. NPC hitscan world impacts and NPC hole navigation.
6. Release-build 240 FPS zero-dip reference scenario.
7. Human confirmation that the mode is still fun and readable.

## 10. What to reuse later

The code and diagnostics are intentionally kept. Reuse:

- `ImpactSystem` as the single effect entry point,
- `server-world-destruction` map activation/reset,
- shared weapon world-impact profiles,
- replicated cut causes,
- incremental map hiding and async cut worker,
- `world.destructible-*` and `network.rx-budget` diagnostics.

Do not reuse the unfinished floor heuristic, the missing NPC hitscan adapter, or
the disabled NPC hole refresh as if they were complete.
