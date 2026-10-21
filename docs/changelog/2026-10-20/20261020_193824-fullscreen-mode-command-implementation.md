# Fullscreen mode command implementation

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-20 19:38:24 EDT (2026-10-20T23:38:24Z)
- Branch: `20261011zt`
- HEAD before changes: `0b5145b26796b94af55c1744c9b00f86cc44bd94`
- Scope: implement `fullscreen <0-4>` and stretched game rendering to match `docs/specs/graphics/graphics-resolution.md`; preserve GUI behavior outside the stretch pass.

## Source changes

New file `src/video/video-mode.h` (lines 1-22): no prior text. Added `FullscreenMode` values `Windowed=0`, `Exclusive=1`, `Borderless=2`, `ExclusiveStretched=3`, `BorderlessStretched=4` and `fullscreenModeName()`.

`src/video/video-settings.h`:

- Line 30: before `void setFullscreen(bool enabled);`; after `bool setFullscreenMode(int mode);`.
- Lines 32-33: added legacy on/off compatibility entry point comment and `void setFullscreen(bool enabled);`.
- Lines 41-42: before `bool fullscreen() const { return mFullscreen; }`; after `bool fullscreen() const { return mFullscreenMode != FullscreenMode::Windowed; }` and `FullscreenMode fullscreenMode() const { return mFullscreenMode; }`.
- Line 60: before `bool mFullscreen = false;`; after `FullscreenMode mFullscreenMode = FullscreenMode::Windowed;`.

`src/video/video-settings.cpp`:

- Lines 55-77: before the bool setter assigned `mFullscreen = enabled`, applied, and saved; after `setFullscreenMode(int)` validates 0-4, applies/saves the selected enum, and the compatibility setter maps true/false to modes 1/0.
- Lines 103-106: before applied the bool mode and initialized FBO from output dimensions; after stores the renderer's actual/fallback mode and initializes the FBO from scene dimensions.
- Lines 126-149: before parsed `fullscreen` only as bool; after reads legacy booleans and validated integer values 0-4, defaulting invalid values to windowed.
- Lines 177 and 190-193: before saved/logged the bool; after saves/logs the numeric mode and readable name.

`src/video/video-commands.cpp`:

- Lines 43-94: before accepted `fullscreen <0|1>` as a toggle; after strictly parses `fullscreen <0-4>`, reports the active mode, applies the selected mode, and logs `video.fullscreen.mode_applied` with requested/applied mode, stretch flag, scene/output dimensions, and terminal request correlation.
- Lines 97-111: before `video_info` reported only ON/OFF; after reports mode name and scene/output sizes.

`src/renderer/renderer.h`:

- Lines 10-19: added `sceneWidth` and `sceneHeight` alongside output dimensions.
- Line 33: before `void applyVideoMode(int w, int h, bool fullscreen);`; after `FullscreenMode applyVideoMode(int gameWidth, int gameHeight, FullscreenMode mode);`.
- Lines 43-50: added applied-mode, saved window position, and cached desktop-mode state.

`src/renderer/renderer.cpp`:

- Lines 148-149: initialize scene size with renderer size.
- Lines 170-171: remember the window position for returning to windowed mode.
- Lines 339-345: keep stretched scene dimensions while updating output dimensions from the actual framebuffer.
- Lines 379-487: before had a bool branch for exclusive/windowed; after handles all five modes, restores windowed placement, uses cached native display dimensions for borderless modes and desktop-resolution exclusive output for stretched mode, tracks scene versus output size, logs the applied mode, and keeps VSync off.

`src/render/post-fx.cpp`:

- Lines 290-307: before bypass always rendered directly to default framebuffer and FBO dimensions followed output; after renders the world at scene dimensions when needed and sets the scene viewport.
- Lines 416-437: before bypass skipped the pass; after the pass scales the scene image to the output framebuffer, including when post effects are bypassed, then returns to the output viewport before GUI rendering.

The following game-world projection call sites now use `sceneWidth`/`sceneHeight` instead of output width/height so their world image shares the selected game aspect ratio. For each file the exact before text was `camera.getProj((float)gRenderer->width, (float)gRenderer->height)` (or the equivalent `cam`/`engine.renderer` spelling), and the after text substitutes `sceneWidth`/`sceneHeight`:

- `src/combat/projectile-render.cpp:253`
- `src/combat/revolver-system.cpp:215`
- `src/combat/weapon-viewmodel.cpp:528`
- `src/debug/debug-visuals-labels.cpp:41`
- `src/debug/debug-visuals-lines.cpp:62,94-97,584-587`
- `src/debug/debug-visuals-tris.cpp:62`
- `src/effects/effect-part-render.cpp:62`
- `src/engine/engine-tick-render.cpp:472-474`
- `src/impact/destructible-render.cpp:99-100`
- `src/physics/physical-entity.cpp:1247-1248`
- `src/render/render-player.cpp:149`
- `src/render/render-world-mesh.cpp:282`
- `src/render/render-world-sky.cpp:85`
- `src/render/skybox.cpp:417-421`: before used a hard-coded 1280x720 projection; after uses renderer scene dimensions with the same dimensions as a no-renderer fallback.

UI rendering stays after the world stretch pass. `src/gui/menus/menu-avatar-preview.cpp:66` keeps its preview viewport projection and was not changed.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/graphics/graphics-resolution.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/live-development/live-development.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/terminal-command-checker-v1.md`, and `docs/skills/logging-checker-v1.md`.
- The router points to `docs/doc-review-09-03-2026.md`, which is absent in this checkout.
- Spec review: PASS. The five command values and scene/output split implement the spec's 4:3-to-16:9 horizontal stretch; GUI is drawn after the stretch and outside its scope.
- Command review: PASS by source inspection. Input must be one full integer in 0-4; invalid/extra values are rejected.
- Logging review: PASS by source inspection. Successful mode selection records a bounded, correlated important event. It uses the enabled General category so `rendering` being off does not suppress the event.

## Validation and remaining review

- Targeted `git diff --check` on touched tracked implementation files: PASS (only line-ending normalization notices).
- Canonical build: first attempt failed before compilation while updating the dev-loop status file (`WinError 5`); retry succeeded. `python devscripts/dev-loop.py --request build --wait-for-build` published build `2694` successfully.
- Executable identity: `C:\mimita-v9\.dev\builds\2694\mimita.exe --versioninfo` reported build time `Oct 20 2026 19:37:00` and `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_193756/events.jsonl`. That journal contains `versioninfo.executed` for this executable and PID 11256.
- No tests were added or run.
- Runtime fullscreen commands and visual stretch are not yet verified. The dev loop reports an existing visible client on build 2692 (PID 17956). Replacing it requires the dev-loop launch action to stop that client, so this session left it untouched. Human visual review must run modes 0 through 4 on build 2694 and inspect that 4:3 world geometry fills 16:9 while UI stays un-stretched.
- Repository-wide whitespace checking reports pre-existing issues in unrelated files; only the focused touched-file check is claimed here.

## Pre-existing changes preserved

At task start, unrelated edits included configuration files, actor/network/weapon specs, the actor-collision configuration source, `src/pch.h.d`, and other gameplay sources. The pre-existing untracked graphics specification and its specification changelog were also left unchanged. `src/network/server-npcs.cpp`, `src/sim/simulate-tick.cpp`, and `src/physics/movement/actor-pair-collision.*` remain outside this change. The current visible dev-loop client was not stopped.
