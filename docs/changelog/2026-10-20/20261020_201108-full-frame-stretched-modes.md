# Full-frame stretched fullscreen modes

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-20 20:11:08 EDT (2026-10-21T00:11:08Z)
- Branch: `20261011zt`
- HEAD before this change: `0b5145b26796b94af55c1744c9b00f86cc44bd94`
- Scope: add modes 5 and 6. Both render the full game frame at exactly 1024x768 and stretch the combined world and GUI image to the display; mode 5 is exclusive and mode 6 is borderless.

## Changes

`docs/specs/graphics/graphics-resolution.md`:

- Scope and exclusions (lines 11-20): before, GUI scaling/aspect behavior was outside this spec; after, GUI layout remains in the GUI spec, while this spec defines that modes 5 and 6 include the completed GUI image in the stretch.
- Mode table (lines 24-36): before modes ended at 4; after adds mode 5 exclusive full-frame and mode 6 borderless full-frame.
- Stretch behavior (lines 38-62): before modes 3/4 said only that the game image stretches; after explicitly keeps modes 3/4 world-only and defines modes 5/6 as a fixed 1024x768 composition of world, GUI, HUD and 2D crosshair, stretched together without bars or cropping.

`src/video/video-mode.h` (lines 9-40): before the enum ended at `BorderlessStretched = 4`; after it also defines `ExclusiveFullFrameStretched = 5` and `BorderlessFullFrameStretched = 6`. Added mode names and shared helpers to classify stretched and whole-frame-stretched modes.

`src/video/video-settings.h:29`: before `// Set fullscreen mode (0-4). Calls apply() internally.`; after `// Set fullscreen mode (0-6). Calls apply() internally.`

`src/video/video-settings.cpp`:

- Lines 57-60: before setter validation capped at `BorderlessStretched` and reported 0-4; after it caps at `BorderlessFullFrameStretched` and reports 0-6.
- Lines 135-137: before config parsing accepted numeric modes through `BorderlessStretched`; after it accepts numeric modes through `BorderlessFullFrameStretched`.

`src/video/video-commands.cpp`:

- Lines 43-62: before help and validation exposed modes 0-4; after the usage is `fullscreen <0-6>` and documents world-only modes 3/4 plus full-frame 1024x768 modes 5/6.
- Lines 70-83: before `stretched` only recognized modes 3/4; after shared classification includes modes 5/6 and the structured command event adds `whole_frame_stretched`.

`src/renderer/renderer.h:31`: added `stretchesWholeFrame()` to expose whether the applied mode composes UI into the scene target.

`src/renderer/renderer.cpp`:

- Lines 339-343: before the frame-size update explicitly excluded only modes 3/4; after it updates scene size only when the applied mode is not stretched, covering modes 3-6.
- Lines 414-417: added full-frame classification and fixed internal dimensions (`1024` by `768`) for modes 5/6, independent of the selected resolution setting.
- Lines 443-460: before the exclusive and borderless stretched cases were modes 3/4; after each shares its display behavior with mode 5/6 while selecting the correct scene dimensions.

`src/gui/ui-system.cpp:75-81`: before `uiBeginFrame` always used the physical framebuffer dimensions; after it uses the scene dimensions for modes 5/6. The existing coordinate system still maps the real window cursor into those virtual scene coordinates, keeping interaction aligned with stretched controls.

`src/debug/debug-visuals-labels.cpp:50-55`: before projected labels always mapped into output dimensions; after modes 5/6 map them to scene dimensions so world labels, nameplates, chat bubbles, hit markers and physical-aim crosshair align before the combined frame is stretched. Modes 3/4 keep output-coordinate mapping for UI drawn after world scaling.

`src/engine/engine-tick-render.cpp`:

- Lines 827-842: before world rendering always resolved PostFX to the display before UI; after modes 5/6 leave the scene FBO bound through the remaining world/debug overlay work, while modes 0-4 retain the prior resolve order. The shadow-map overlay uses scene dimensions when it is inside the full-frame target.

`src/engine/engine-tick.cpp`:

- Line 35: added `#include "perf/perf-gpu.h"` for the frame-end GPU timing scope.
- Lines 134-135: bind/clear the scene FBO after `beginFrame` in modes 5/6, including menu-only frames.
- Lines 335-341: after terminal/UI rendering, resolve the complete scene FBO to the output before swap.

## Documents and review

- Read `docs/ROUTER.md`, the graphics resolution and GUI specs, terminal command and JSON configuration architecture, the runtime scenario workflow, debug logging spec, build and task completion operations, and `spec-behavior-review-v1`, `terminal-command-checker-v1`, and `logging-checker-v1`.
- The router's `docs/doc-review-09-03-2026.md` target is absent in this checkout.
- Specification review: PASS. Modes 3/4 remain world-only stretch; modes 5/6 explicitly include UI and use fixed 1024x768 scene composition.
- Command review: PASS by source inspection. The command parses exactly one integer in 0-6, saves the selected numeric mode, and records actual applied mode and stretch type.
- Logging review: PASS by source inspection. The mode command event records selected/applied mode, scene/output sizes, and full-frame stretch classification.

## Validation and remaining evidence

- Focused `git diff --check`: PASS (only line-ending normalization notices).
- First build failed at `src/engine/engine-tick.cpp` because the new GPU timing scope lacked `perf/perf-gpu.h`. Added that include; the next canonical build succeeded.
- Canonical build: `python devscripts/dev-loop.py --request build --wait-for-build --build-timeout 240` published build `2705`.
- Identity probe: `C:\mimita-v9\.dev\builds\2705\mimita.exe --versioninfo` recorded `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_201050/events.jsonl`. That journal contains `versioninfo.executed` for build path `.dev/builds/2705/mimita.exe`, PID 17616, run id `20261020_201050`.
- No tests were added or run.
- Runtime execution of commands 5 and 6 and visual confirmation remain outstanding. The dev loop currently has a visible client on build 2704 (PID 30192), with build 2705 published. This work left the active client untouched. Human review should confirm the complete image, including the GUI and 2D crosshair, becomes wider at 16:9 and that the UI remains clickable.

## Other workspace changes preserved

The working tree already contained unrelated changes to configuration, actor/gamemode/networking/weapon specifications, gameplay/networking/duel sources, and changelogs. They were preserved. The previous fullscreen modes 0-4 implementation in this checkout was extended rather than replaced.
