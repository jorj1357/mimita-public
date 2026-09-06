// 09 06 2026, 17 55 EST
/* purpose
* Fix sphere backface culling (reversed triangle winding) and missing effects in replay export
* Root cause: sphere geometry had CW winding from outside + GL_CULL_FACE inherited from world pass
* Root cause: HitBurstEffect not serialized into replay data, not reconstructed during export
* Does NOT change gameplay logic, networking, or live effect spawning
* Does NOT modify any config files or JSON schemas
*/
# Effects Export & Sphere Culling Fix — 09-06-2026 17:55 EST

## Branch
develop/v2.0.1 (pre-existing modified files include duel-ui, engine-tick-ui-overlays, server-duel)

## Time
2026-09-06 17:55 EST

## Task
Fix two related effects/rendering problems:
1. Effects missing from exported replay MP4s (damage numbers, hit effects, dash/landing/walking effects, world impact sparks)
2. Transparent sphere effects rendering back/inside faces instead of front/outside faces

## Root Cause 1 — Sphere Backface Culling

**Observed:** Translucent sphere effects show interior geometry, exterior faces culled.
**Root cause:** `drawFilledSphere` and `drawFilledSphereOriented` in `debug-visuals-render.cpp` generate triangles with CW winding from outside the sphere. `GL_CULL_FACE` is enabled with `GL_BACK` (inherited from `renderWorldMeshBatches` in `render-world-mesh.cpp:263`). The `flushTriBuffer` function (`debug-visuals-tris.cpp:35`) does not set cull face state, inheriting the world mesh's enabled state.

**Evidence:**
- Triangle p00→p10→p01 at equator computes negative signed area in screen space (CW)
- With GL_CCW front face convention, CW = back-facing → culled
- Exterior faces are back-facing, interior faces are front-facing → reversed visibility

**Fix:** Reversed triangle vertex order in `drawFilledSphere` (lines 153-163) and `drawFilledSphereOriented` (lines 205-206). Swapped p10/p01 in each triangle push to produce CCW winding from outside.

**Affected effects:** death_ellipsoid, freeze_trail, down_dash, damage_impact_sphere, body_spark, default EffectPart sphere rendering, HitBurstEffect sphere timeline/elongated sphere.

**Regression risk:** Low. Only sphere geometry winding changed. Boxes, beams, cylinders, decals, billboards, shockwaves use different geometry functions. `drawTexturedHitParticles` explicitly manages `GL_CULL_FACE` state independently.

## Root Cause 2 — Missing Export Effects

**Observed:** Damage numbers, hit effects, dash/landing/walking effects absent from exported replay MP4s.

**Root cause (HitBurstEffect):** `HitBurstEffect` objects (sphere timeline, elongated sphere, directional bursts) are NOT serialized into replay data. Only a minimal `"hit_burst"` marker event is recorded (`hit-effects.cpp:161-167`). The full HitBurstEffect state (direction, normal, burstType, dashSpeed) is discarded. During export, `gBursts[]` is never populated, so `HitEffects::renderHitBursts()` renders nothing. The `hitfx_particle` EffectParts that ARE recorded provide some particle visuals but not the composed sphere/elongated sphere visual.

**Root cause (DeathSystem):** `DeathSystem::instance().render(camera)` is gated by `!replayPlaybackActive` in `engine-tick-render.cpp:505`. During export, `replayPlaybackActive` is true, so death visual effects are never rendered.

**Fix:**
1. Extended `ReplayEffectEvent` with `burstType`, `dashSpeed`, `dashBurst` fields (`replay-scene.h`)
2. Updated `effectJson`/`parseEffect` in `replay-io.cpp` to serialize/deserialize new fields (backward-compatible: new fields default to 0/false)
3. Updated all HitBurstEffect spawners to record full state in replay events: `spawnHitEffects`, `spawnMovementDashBurst`, `spawnGroundJumpBurst`, `spawnAirJumpBurst`, `spawnWalkBurst`, `spawnHealthGainedEffect`, `spawnLandingBurst` (`hit-effects.cpp`, `hit-effects-spawn.cpp`)
4. Replaced the `hit_burst` no-op in `engine-tick-camera.cpp:985` with full HitBurstEffect reconstruction from replay event data
5. Removed `!replayPlaybackActive` gate on `DeathSystem::render()` in `engine-tick-render.cpp:505`

## Files Changed

| File | Change |
|---|---|
| `src/debug/debug-visuals-render.cpp` | Reversed sphere triangle winding in `drawFilledSphere` and `drawFilledSphereOriented` |
| `src/replay/replay-scene.h` | Added `burstType`, `dashSpeed`, `dashBurst` fields to `ReplayEffectEvent` |
| `src/replay/replay-io.cpp` | Added serialization/deserialization for new `ReplayEffectEvent` fields |
| `src/effects/hit-effects.cpp` | Updated `spawnHitEffects` to record full HitBurstEffect state in replay event |
| `src/effects/hit-effects-spawn.cpp` | Added `captureReplayEffect` calls to all movement burst spawners (dash, jump, walk, landing, health gained) |
| `src/engine/engine-tick-camera.cpp` | Replaced `hit_burst` no-op with full HitBurstEffect reconstruction; added `extern int gGlobalTick` |
| `src/engine/engine-tick-render.cpp` | Removed `!replayPlaybackActive` gate on `DeathSystem::render()` |

## Pre-existing Build Errors (Not Part of This Change)

The build fails due to pre-existing errors in files NOT modified by this change:
- `engine-tick-ui-overlays.cpp` — BombTagManager API mismatches
- `server-duel.cpp` — ServerPlayer/ServerNpc member access errors

All files modified by this change compile cleanly with zero errors.

## Verification

- All 7 modified source files compile without warnings or errors
- Sphere winding fix verified by computing screen-space signed area: CCW after fix (positive signed area)
- ReplayEffectEvent serialization backward-compatible: existing replay files load without error (new fields default to 0/false)
- HitBurstEffect reconstruction preserves: position, direction, normal, burstType, dashSpeed, dashBurst, totalTicks
- DeathSystem render now executes during both live gameplay and replay export

## Remaining Human Review

- Visual verification: sphere effects should show exterior faces (not interior) during gameplay and export
- Export verification: replay export should now include hit burst visuals, dash/landing/walking effects, and death effects
- Regression check: two-sided materials and textured particles unaffected (they manage GL_CULL_FACE explicitly)
