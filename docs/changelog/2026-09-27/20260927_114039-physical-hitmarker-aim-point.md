# Physical hitmarker aim-point alignment

Time: 2026-09-27T11:40:39-04:00 (EST)
Branch: current working branch (not changed)

## Request

In physical aim mode, show the hitmarker at the same world-projected screen
location as the crosshair instead of always at screen center. Keep the current
normal PNG appearance for now, with code-generated presentation deferred.

## Pre-existing working-tree changes

The following changes existed before this session and were preserved:

- `build.py`
- `config/analytics.json`
- `config/weapons.json`
- `config/accounts/default.json` (observed after validation and preserved)
- `docs/regressions/2026-09-20/cold-build-required-REG.md`
- `src/network/server-packet-chat.cpp`
- `docs/changelog/2026-09-27/20260927_112700-linker-stale-objects.md`

## Source change

- `src/engine/engine-tick-ui-game-hud.cpp:163-180` now keeps the projected
  physical-aim crosshair coordinates and passes those same coordinates to
  `drawHitmarker`. The marker still falls back to center when no alternate
  aim point is available.
- `src/ui/hitmarker.h:13-15` and `src/ui/hitmarker.cpp:43-72` accept optional
  screen coordinates while retaining the existing duration, fade, size, and
  `assets/crosshair/crosshairhit.png` rendering.
- `src/engine/engine-tick-ui-replay-hud.cpp:84` no longer draws the marker
  before the gameplay HUD, preventing a center-screen duplicate in the same
  frame. The gameplay HUD now owns the single draw call.

Old renderer behavior:

```cpp
float cx = uiScreenW() * 0.5f;
float cy = uiScreenH() * 0.5f;
uiDrawImageRotated("assets/crosshair/crosshairhit.png", cx, cy,
                   hitmarkerSize(), 0.0f, color);
```

New behavior:

```cpp
const float cx = screenX >= 0.0f ? screenX : uiScreenW() * 0.5f;
const float cy = screenY >= 0.0f ? screenY : uiScreenH() * 0.5f;
uiDrawImageRotated("assets/crosshair/crosshairhit.png", cx, cy,
                   hitmarkerSize(), 0.0f, color);
```

## Specification and skills

- `docs/ROUTER.md`
- `docs/specs/gui/guiv2.md`: shared screen/world-space presentation and no
  duplicate feature-specific renderer.
- `docs/specs/weapons/weapons.md`: aim-mode and weapon presentation context.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW; source
  behavior now matches the requested projected aim point, but visual runtime
  acceptance remains pending.
- `docs/skills/asset-checker-v1.md`: PASS; the existing tracked asset is
  present at `assets/crosshair/crosshairhit.png` and its runtime path is
  unchanged.
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

- `python build_agent.py`: SUCCESS; the first incremental pass compiled the
  HUD caller and linked. Because the first pass incorrectly skipped the
  changed hitmarker object, its source timestamp was refreshed and a second
  pass compiled `src/ui/hitmarker.cpp` and linked successfully.
- `git diff --check`: PASS; no whitespace errors.
- Asset check: PASS; `assets/crosshair/crosshairhit.png` exists and is tracked.
- Active MiMITA processes were not stopped, restarted, or replaced.

## Human review still needed

Run the active game in physical aim mode, hit a world target away from screen
center, and confirm the normal hitmarker appears on the projected crosshair.
No in-game visual acceptance was performed in this session.
