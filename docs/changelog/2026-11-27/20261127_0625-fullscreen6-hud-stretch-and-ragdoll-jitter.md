# Fullscreen 6 stretched HUD/crosshair and local ragdoll render jitter

- Task/agent: `hud-fs6-crosshair`
- Date, time, timezone: 2026-11-27T06:25Z, ISO 8601
- Status: implemented, built, runtime-proven for the logged events; human visual acceptance still needed
- Linked issues: `issue-8237d36d84d0` (done), `issue-5872865040d4` (done)

## Pre-existing changes

The shared checkout already contained extensive uncommitted work from other agents
(vehicle/Rocket League, Bloodtest presentation, jitter probe, build scripts, configs,
and docs). This session preserved it, edited only the files below, and coordinated
through the shared brain. Builds went through the single root dev-loop (`--request
batch-build`) after the user directive that agents share one build time.

## Requested behavior

1. Fullscreen mode 6 must stretch the whole HUD with the 1024x768 frame, not draw a
   crisp output-resolution HUD.
2. The crosshair must follow the mode's crosshair rule; Fun Counter Strike uses
   `aim.mode=physical`, so the reticle belongs at the weapon aim point, not pinned
   to screen centre.
3. Local ragdoll body parts must move smoothly with no per-position jitter; the
   client pose and the server-accepted pose must agree.

## Specification alignment

- `docs/specs/graphics/graphics-resolution.md:51-56`: modes 5/6 compose world, GUI,
  HUD, and the 2D crosshair at 1024x768 and stretch the whole image. This is the
  authoritative contract and matches the user's request.
- `docs/specs/graphics/graphics-post-processing.md` and
  `docs/specs/gamemodes/bodycam.md` were reconciled to the same contract; the old
  "crisp HUD by default in modes 5/6" text and the dead `ui_in_scene_target`
  cross-reference were removed.
- `docs/specs/animations/animations.md`: `renderPoseInterpolation` blends the last
  two fixed poses; the ragdoll fix restores that behavior for live ragdoll ticks.

## Exact implementation changes

1. `src/engine/engine-tick.cpp`: `uiInScene = wholeFrame` instead of
   `wholeFrame && lensModeMakesUiScene(...)`. Whole-frame modes always draw the
   gameplay HUD/console into the 1024x768 scene target before PostFX.
2. `src/gui/ui-system.cpp`: `uiBeginFrame()` uses `sceneWidth/sceneHeight` for
   whole-frame passes without requiring the gamemode `scene` layer, so HUD and menu
   UI share the same target size instead of being clipped.
3. `src/engine/engine-tick-ui-game-hud.cpp`: removed the whole-frame forced-centre
   crosshair. The crosshair uses `aim_source=physical` (projected
   `weapons.mPhysicalAimPoint`) when the mode aim is physical, otherwise centre.
   Diagnostics now log `aim_source`, `centered`, and `center_offset_x/y`; the
   verdict compares against the resolved rule instead of asserting centre.
   `JitterProbe::sampleHud` receives `!physicalAimUsed` so HUD jitter still samples
   centred rules only.
4. `src/render/postfx-commands.cpp`, `src/render/post-fx.h`, `src/render/post-fx.cpp`:
   removed the now-dead `lensModeMakesUiScene()` routing helper and updated the
   presentation probe to report `ui_in_scene = whole_frame`.
5. `src/ragdoll/ragdoll-mode.cpp`: `RagdollModeSystem::update()` advances
   `player.movementSimulationTick` once per live ragdoll fixed tick. This field was
   previously frozen while ragdoll ran (the movement kernel is skipped), so
   `Player::updateModelWorldTransforms()` never captured `previousWorldTransform`
   and `playerRenderPartTransform()` blended from a stale pre-ragdoll pose.

## Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `src/engine/engine-tick.cpp` | whole-frame HUD routed into scene | restore `wholeFrame && lensModeMakesUiScene(...)` |
| `src/gui/ui-system.cpp` | scene-size HUD for whole-frame passes | re-add `gHudSceneLayer` condition |
| `src/engine/engine-tick-ui-game-hud.cpp` | physical-aim crosshair + diagnostics | restore forced-centre block |
| `src/render/postfx-commands.cpp` | probe uses whole-frame routing | restore lens-mode helper call |
| `src/render/post-fx.{h,cpp}` | delete dead routing helper | restore function |
| `src/ragdoll/ragdoll-mode.cpp` | advance live ragdoll tick | remove the increment |
| `docs/specs/graphics/graphics-post-processing.md` | document stretched modes 5/6 | restore old text |
| `docs/specs/graphics/graphics-presentation.md` | update crosshair/HUD space note | restore old note |
| `docs/specs/gamemodes/bodycam.md` | reconcile mode-6 HUD line | restore old line |

## Build and runtime evidence

- Build: `--request batch-build` published generation 779 with
  `engine-tick-ui-game-hud.cpp`, `post-fx.cpp`, `postfx-commands.cpp`; later
  generation 781 published `ragdoll-mode.cpp`. Both builds succeeded through the
  root dev-loop.
- HUD proof, run `20261127_060325` (generation 779, Fun Counter Strike,
  dust2cyberiav4, client-01):
  - `ui.scene.draw_path`: `whole_frame=true`, `ui_in_scene=true`, `postfx_hud=false`.
  - `ui.scene.presentation`: framebuffer `1024x768`, output `1920x1080`, `stretched=true`.
  - `hud.crosshair.center`: `fullscreen_mode=6`, `hud_space=scene`,
    `physical_aim_used=true`, `centered=false`, live offsets from centre (for
    example x=506.1, y=395.6 against 512/384), `hud.crosshair.verdict matches=true`.
- Ragdoll proof, run `20261127_061933` (generation 781, Bloodtest Slaughterhouse 1,
  client-01, ragdoll toggled on):
  - Before (run `20261127_054945`, generation 777): local actor
    `actor.pose.interpolation` previous/current steps of 17.9-56.1 m rejected by
    the >8 m guard, i.e. raw per-tick stepping.
  - After: no `actor.pose.interpolation` rejections after activation; steady
    `jitter.verdict matches=true` with `actor accel_max_m=0.00119`,
    `raw_lag_max_m=0.00097` over 1812 samples; HUD probe `error_max_px=0`.
  - Activation still produces one intentional first-pose snap (max 0.245 m in the
    activation window), which is not per-frame jitter.

## Not claimed

- Human visual acceptance of the stretched HUD look or ragdoll smoothness was not
  performed by the agent from a clean, unfocused window capture. The user should
  confirm in-game.
- Ragdoll networking is owner-authoritative by design (client sends its pose; the
  server applies the replicated pose in `server-physical-contact-actors.cpp`); no
  server-side ragdoll simulation change was made.

## Follow-ups recorded, not fixed

- `presentation.hud.layer` is now effectively a no-op for whole-frame modes; the
  field is still parsed for compatibility. A tech-debt note belongs with the HUD
  policy owner if the layer setting is meant to keep an opt-out.
- The presentation probe's mode-2 verdict now checks `worldCa && !uiCa` because the
  UI is always in the scene in whole-frame modes.
