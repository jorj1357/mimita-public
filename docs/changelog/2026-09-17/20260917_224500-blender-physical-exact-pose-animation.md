# Blender physical exact-pose animation runtime (phases 0-8)

- EST timestamp: 2026-09-17 22:45:00 -04:00
- UTC timestamp: 2026-09-18T02:45:00Z
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Why

Add a second animation path beside the existing procedural hot animation: a
Blender-driven 60 Hz exact-pose system where physics sweeps the actor's body
parts and weapon through the world, slides/bounces on static geometry, pushes
dynamic actors, and returns the rejected force to the animating actor. Tools
request animations through the existing shared `tool.action` events. Animation
owns pose/sweep/contact/reaction; the tool owner still owns damage/ammo/cooldown.

## Phase 0 decision (pose baseline)

The canonical hot pose unit is the per-part **local delta from the rest pose, in
the actor's local axes, degrees**, converted to radians only at the
`skeleton.apply` boundary. This matches both the existing procedural library and
`capSkeletonApply` (`restLocal * poseOffset`, `live-behavior.cpp:1354-1357`). The
exporter bakes each authored bone transform as `restLocal^-1 * sampledLocal` and
converts the delta Euler to degrees, so no rest data is needed at runtime.

Known divergence (recorded, not fixed here): the generic flat skeleton applies
pose parts as absolute local TRS (`skeleton-instances.cpp:90-93`) rather than
rest-relative. Exact clips are therefore consistent on the typed player path;
unifying the flat path needs a skeleton-metadata capability and is future work.

## Changes (all hot / DLL-side unless noted)

- `src/hot-reload/hot-animation-blender.h` (new)
  - `AnimationMode { CurrentHot, BlenderPhysical }`, `PoseAuthority { Wished, Exact }`.
  - `BlenderClip` (tick-indexed, actor-relative, 60 Hz), `BlenderMarker`
    (weapon/part probe with local offset + radius), one clip registry, exact
    `sampleBlenderClipAtTick` (loop wraps, one-shot holds), and
    `findBlenderClipForAction(action, weaponKey)`.
  - One coordinate-conversion owner `blenderToMimita` (identity until a
    rest-pose check proves a forward-axis difference).
- `src/hot-reload/hot-animation-physical.h` (new)
  - `ActorPoseV1`, `PoseReactionStateV1`, `PhysicalAnimationStateV1`
    (target + resolved + reactions), `PhysicalContactResultV1`,
    `PhysicalContactResponse`, `PhysicalAnimationRecipeV1`,
    `PoseReactionRecipeV1`, `ToolAnimationSetV1`.
  - Dynamic component ids (`PhysicalAnimationState`, `AnimationModeState`) and
    the generic animation fact ids (body/weapon contact, static reaction, actor
    push, pose blocked/completed).
- `src/hot-reload/modules/presentation/animation-physical.cpp` (new)
  - Fixed-tick (`postmovement.60`) `hot.animation-physical`: follows the hot state
    machine's selected action, resolves an imported clip (else hands the actor
    back to the procedural generator), samples the exact tick pose, sweeps each
    configured body part + the weapon marker through `collision.main`, applies
    the static-world reaction and dynamic-actor push through `physics.impulse`,
    integrates the additive sway/impact layer, writes the state, and emits facts.
  - Render-frame (`render.frame`) `hot.animation-physical-pose` converts the
    resolved pose to radians and calls `skeleton.apply`.
  - Registers the two component schemas, a `physanim 1|0` command (global
    default is BlenderPhysical), and capability requirements for `collision.main`
    and `physics.impulse`. Probe solves use `entityId = 0` so a real actor's
    collision/ground memory is never perturbed.
- `src/hot-reload/modules/presentation/animation-blender-clips.cpp` (new)
  - Includes generated clip headers so their static registration runs in the DLL.
- `tools/blender_export_animation.py` (new, offline)
  - Blender mode samples an armature at exactly 60 Hz, actor-relative to
    `plrOrigin`, converts coordinates, and emits the canonical JSON asset + the
    generated hot C++ header. Standalone mode expands a sparse key spec into one
    pose per fixed tick.
- `tools/animation_specs/katana_slash.json` (new) and
  `assets/animations/animation.katana_slash.json` (new) - the exported asset.
- `src/hot-reload/generated/hot-anim-katana-slash.h` (new, generated) - 18
  tick-exact frames, full-body mask, weapon-edge + right-hand markers.
- `src/hot-reload/modules/presentation/tool-visuals.cpp`
  - Added `makeKatanaVisual()`: a brand-new hot tool that reuses the shared
    `TOOL_BEHAVIOR_MELEE` behavior and registers in the tool registry. No one-off
    katana damage or renderer branch. Model is a documented placeholder.
- `src/hot-reload/modules/presentation/pose-generation.cpp`
  - Skips actors whose `PhysicalAnimationState` is active, so the exact runtime is
    the single pose writer for them; all other actors are unchanged.
- `src/hot-reload/modules/presentation/animation-selftest.cpp`
  - Added exact-pose candidate checks: clip registered/60 Hz/tick-exact, full
    mask, deterministic tick sampling, one-shot final-tick hold, finite fixed-tick
    arm velocity, action->clip selection, weapon marker, coordinate conversion,
    versioned state contract, and force-transfer recipes.
- `src/hot-reload/hot-modules.json`
  - Added `src/hot-reload/generated/**/*.h` to `headerGlobs` so generated clip
    changes participate in change detection and code hashing.

## Evidence

- Source/build: `python build_game_dll.py` -> `[HOT RELOAD] DLL build success:
  build\mimita-game.dll` (`sources=81`). No `mimita.exe` write.
- Cold compatibility: `g++ -fsyntax-only` on the two new hot TUs without
  `MIMITA_GAME_DLL` exits 0 (they compile out of the EXE cleanly).
- Runtime (deterministic data path): a standalone harness compiled against the
  same headers ran the same assertions as the candidate self-test and printed
  `PASS: 18 exact-pose runtime checks` (clip registry, 60 Hz, tick-exact frames,
  deterministic/one-shot sampling, fixed-tick arm velocity, action->clip
  selection, marker, coordinate conversion, state contract, recipes).

## Human verification still required

- Run the game with this DLL and confirm gameplay shows the katana
  `animation.katana_slash` exact pose (equip `katana`, click to slash), instead of
  the procedural `HOT_ACTION_SLASH` clip. `physanim 0` must restore the procedural
  path for the same session.
- Confirm collision behavior next to static geometry (expected: slide/bounce, no
  world push, a reaction impulse on the player) and against a dynamic actor
  (expected: target push + self reaction). These were not executed here because
  they need a live world.
- Confirm the candidate self-test still activates (the new checks run at DLL
  activation) and that an invalid clip rejects the candidate and keeps the
  previous generation live.
- Confirm the `animation.weapon-contact` facts are visible to the tool owner. In
  this slice katana damage is still applied by the shared `TOOL_BEHAVIOR_MELEE`
  behavior on `tool.primary-use` (the behavior's existing `damage.apply` path);
  wiring the tool to consume the animation weapon-contact fact instead is the
  remaining end-to-end step.

## Notes

- BlenderPhysical is the default clip-preference mode; actors without an imported
  clip fall back to the procedural path, so existing behavior is unchanged until
  a clip is authored. This is the safe reading of "default globally".
- The katana model path is a placeholder (`mimita-hafs-v1.glb`) until a katana
  mesh exists; the contact shape follows the authored weapon marker regardless.
- `AnimationModeState` is registered but not yet written; per-actor mode override
  is future work.
- Unrelated pre-existing working-tree modifications were preserved and are not
  claimed by this change.
