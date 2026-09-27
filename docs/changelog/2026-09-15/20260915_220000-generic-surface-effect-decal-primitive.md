# Generic surface-effect / decal primitive (hot decal policy)

Date: 2026-09-15 22:00 EST (UTC 2026-09-16T02:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW`

## SUBSYSTEM
Client presentation - surface effects / decals.

## COLD OWNER REMOVED
Feature-specific decal policy for the migrated hit paths: the cold
`SurfaceDecalKind`-driven composition in `effect-part-blood.cpp`/`effect-part.cpp`
no longer runs when the hot hit handler handles the fact (`HitEffects::onHit`
yields). The cold path stays as mechanism + fallback.

## HOT OWNER ADDED
`hot.effect-composition` emits generic surface-effect requests for
`effect.hit.blood` / `effect.hit.world`; `hotsurfaceeffect` command creates a
runtime-unknown mark. Hot chooses color/size/lifetime/orientation.

## GENERIC PRIMITIVE ADDED
`GAME_CAP_SURFACE_EFFECT` (`surface.effect`) + `GameSurfaceEffectV1`
(position, normal, axis, color, radius, height, rotation, lifetime, fadeTime,
sourceEntity, flags). Kernel `capSurfaceEffect` builds a generic `SurfaceDecal`
and pushes it; `SurfaceDecal` gained a `generic` flag; the renderer draws generic
marks from color/size only (`DebugVis::drawFilledDecal`) with no feature kind.
No `BloodDecalRequest`/`BulletHoleType`/`ScorchRenderer`.

## COLD MECHANISM REMAINING
Decal storage (`mSurfaceDecals`), lifetime update, projection/orientation basis,
draw. (Projection/clipping for arbitrary surfaces is basic; acceptable for now.)

## COMPATIBILITY FALLBACK
Cold `SurfaceDecalKind` composition in `HitEffects::onHit` when no hot handler
handles the fact.

## REAL SHIPPING PATH MIGRATED
`effect.hit.blood` and `effect.hit.world` (real hit/blood/world impacts) now emit
the generic surface effect. Exactly one owner.

## RUNTIME-UNKNOWN PROOF
`hotsurfaceeffect` creates a mark the EXE never knew about through the generic
capability - no enum, no cold switch, no feature-specific ABI.

## SELFTEST PROVEN
"hot surface-effect policy creates a generic decal (no enum)"; "real hit/blood
fact reaches the hot effect owner"; full suite PASS.

## LIVE HOT-EDIT PROVEN
No (no screen).

## CONCURRENCY BOUNDARY STATUS
Clean: no movement/network files touched; movement policy is the other agent's
area and was only read to verify.

## BUGS DEFERRED
Muzzle flash, screen flash, camera shake; decal art/projection quality; remaining
audio policy; weapon presentation; visual tuning.

## WOULD THIS BUG STILL REQUIRE COLD RESTART?
Surface decal policy (whether/what/size/lifetime): **no** (hot). Muzzle-flash/
camera-shake/remaining-audio/weapon-presentation: yes until migrated. Renderer/
backend bugs stay cold.

## NEXT COLD OWNER
Muzzle flash (via generic tool/fire state + effect entities/audio), then
camera/screen effect policy, then weapon-fire/footstep audio, then weapon
presentation.

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.{h,cpp}`,
`src/effects/effect-part.h`, `src/effects/effect-part.cpp`,
`src/effects/effect-part-render.cpp`,
`src/hot-reload/modules/presentation/effect-composition.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
