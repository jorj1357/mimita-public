# Phase 2 + 3 — hot explosion/projectile visuals + hit effects on existing primitives

- UTC timestamp: 2026-09-16T19:44:32Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated animation edits preserved)
- Result: `PASS` (cold build SUCCESS; all relevant selftests PASS; live/human pending)
- Evidence class: source + hot build + cold build + headless selftests

## Task

Phase 2: make the rocket/grenade projectile and explosions visible, and compose
the detonation from the hot projectile simulation. Phase 3: restore world hit
effects (blood, bullet holes, cracks, impact spheres, damage numbers, tick burst)
to the original textured-PNG look, but live-editable in hot C++, by exposing the
existing cold `EffectPart`/decal primitives to hot rather than inventing new ones.

## Phase 2 — projectile + explosion

- `src/hot-reload/modules/presentation/effect-composition.cpp`: explosion
  composition refactored into one `composeExplosion(ctx, type, pos, scale)`
  (flash/smoke/debris/sound); the generic `effect.explosion.*` branch calls it.
  Added the external `hotComposeExplosion` entry used by the hot projectile sim.
- `src/hot-reload/modules/tools/hot-projectiles.cpp`: `explode()` now composes the
  shared rocket/grenade detonation when the process has a local view (client /
  listen host); a dedicated server does not author client-only effects. The
  projectile's `PresentationState` is materialized from the recipe when
  replication did not deliver it, so the client always has something to draw.
- `src/hot-reload/hot-tool-visual.h` / `tool-visuals.cpp`: `findProjectileVisual`
  maps network projectile ids (rocket 5, grenade 7) to the recipe registry.
- `PresentationState` schema version bump to v2 was **deferred**: the struct
  already grew (append-only `scaleXYZ`) and bumping the version needs a migration
  path, otherwise the hot-reload activation check rejects it. Tracked as follow-up.

## Phase 3 — hit effects on the existing primitives

Cold generic additions (reuse the exact existing renderer):
- `game-api.h`: `GAME_CAP_EFFECT_PART` + POD `GameEffectPartV1` mirroring
  `EffectPart` (textured billboards, sticky/flat decals, beams, boxes, gravity,
  tick lifetimes). `EffectRequestV1` v3 append-only hit fields (damage,
  directness, hitDistance, hitEntity, victimName, spawnDamageNumber).
  `GameSurfaceEffectV1` gained a decal texture + `decalKind`.
- `src/live-code/live-behavior.cpp`: `capEffectPart` (kernel pool primitive),
  `capSurfaceEffect` textured/kinded decals, capability registration, and the
  `effect.request` schema bumped to v3.
- `src/effects/effect-part.h` / `effect-part-render.cpp`: `SurfaceDecal` gained an
  explicit `texturePath`/`textureScale`; the decal renderer prefers the hot
  texture, else the kind-based JSON texture. `spawnGenericSurfaceDecal` unchanged.
- `src/effects/hit-effects.cpp`: populates the v3 hit fields on the request.

Hot migration:
- New `src/hot-reload/modules/presentation/hit-visuals.cpp`: `hotComposeHit`
  reproduces the cold `HitEffects` look — red entity impact sphere, gray world
  impact sphere, textured blood spray billboards (`hitfx_particle` +
  `assets/textureshq/sblood1.png`), blood splat decals, bullet hole, cracked
  surface strips (`crackground1.png`), red damage streak, damage number, and a
  tick-based impact burst (`hot.hit-burst` system + `HitBurstState`, lifetime
  defined in ticks). All values copied from `config/hitfx.json` +
  `config/impact_decals.json`, so the result matches the pre-migration behavior
  while being editable live in this `.cpp`.
- `effect-composition.cpp`: the cube-based `effect.hit.*` branch now calls
  `hotComposeHit`. JSON remains the fallback when the hot handler is absent.

## Incidental fix

- `src/live-code/live-behavior.cpp`: added the missing `#include "network/server.h"`
  for the concurrent session's `MimitaNet::HeadlessWorld` use, which was breaking
  the cold build. No behavior change.

## Validation

- `python build_game_dll.py` -> DLL build success.
- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --hot-combat-selftest` -> **PASS**, including:
  - `effect.part capability resolves (existing EffectPart primitive)`
  - `explosion fact composes flash/smoke/debris [before=19 after=24 handled=1]`
  - `hot blood hit composes textured decals`
  - `hot world hit composes a bullet hole + cracks`
  - `real hit/blood fact reaches the hot effect owner`
- `--live-code-selftest`, `--production-loop-selftest`, `--glb-consumer-selftest`,
  `--tool-entity-continuity-selftest`, `--content-resource-selftest` -> all PASS.
- `git diff --check` clean.

## Human confirmation still required (live)

- Weapon models visible in first AND third person (Phase 1).
- Fire the rocket launcher: launcher model + travelling projectile + explosion
  flash/smoke/debris/sound visible.
- Get hit / shoot a wall: blood spray + splats, bullet holes, cracks, impact
  spheres, damage numbers, and the tick impact burst look like the old PNGs.
- Edit values in `tool-visuals.cpp` / `hit-visuals.cpp` and confirm the change
  appears in the same running world.

## Pre-existing changes

The working tree contains unrelated concurrent-session animation/headless-collision
edits (`hot-action.h`, `hot-animation*.h`, `network/server*`, movement/NPC files).
They were preserved untouched (apart from the one missing-include build fix above).

## Follow-ups

- `PresentationState` schema v2 + migration.
- Exact parity for the hit-burst timeline shapes (current version is the
  tick-sphere/spray approximation); delete now-unused JSON visual fields once the
  hot path is confirmed live.
