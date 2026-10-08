# Spectator cursor investigation

Time: 2026-10-08T09:56:29-04:00 (America/New_York)
Branch: `afad20a-rebuild`
Commit inspected: `90a61a67`
Working tree: pre-existing edits in `config/actor-presets/juggernaut_fighter.json` and `config/analytics.json`; preserved.

## Scope and result

Investigation-only review of the reported Juggernaut dead/spectator freecam behavior:
pressing L unlocks the mouse for about one frame, and opening the Escape pause menu allows button activation while the cursor is not visible.

Result: the L symptom has a source-confirmed cause. The Escape symptom is not fully source-confirmed; the code requests a normal cursor when the pause menu opens, so a short runtime trace is needed to distinguish a later cursor-mode overwrite from a custom-cursor presentation problem.

## Documents and focused skills read

- `docs/ROUTER.md`
- `docs/specs/gui/guiv2.md`
- `docs/specs/gamemodes/juggernaut.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/regressions/README.md`

## Findings

### L unlocks for one frame in spectator freecam

The death/spectator path in `src/engine/engine-tick-camera.cpp:290-315` treats `Dead`, `Respawning`, and `Spectating` as a live-round freecam state. When keyboard input is enabled, `anyFreecam` is true. The same function then unconditionally reapplies `GLFW_CURSOR_DISABLED` at `src/engine/engine-tick-camera.cpp:900-904`.

The L handler in `src/engine/engine-tick.cpp:246-253` calls `MouseLock::toggle`, which correctly changes the shared lock flag and requests `GLFW_CURSOR_NORMAL`. On the next camera pass, spectator freecam immediately requests `GLFW_CURSOR_DISABLED` again. That exactly explains the observed one-frame unlock. No new log is required to establish this code-level cause, although runtime confirmation should still be separate from source evidence.

### Escape menu cursor is not explained completely by the current source

`PauseMenu::toggle` at `src/gui/menus/pause-menu.cpp:145-153` disables gameplay keyboard input and requests `GLFW_CURSOR_NORMAL`. The camera freecam condition at `src/engine/engine-tick-camera.cpp:314-315` should therefore become false while the pause menu is open, so the spectator freecam overwrite should stop.

The menu can still receive button clicks through `drawGuiElement` in `src/gui/menus/pause-menu.cpp:170-234`, which proves the UI hit-test path is active but does not prove that the native/custom cursor is visually present. A custom GLFW cursor is installed at `src/renderer/renderer.cpp:186-188` and `src/renderer/renderer.cpp:240-295`; the asset is `assets/textures/cursor.png`. The current source has no structured record of every cursor-mode writer, its final mode, or custom-cursor installation state. Therefore the remaining possibilities are:

1. another per-frame path reapplies disabled mode after Escape;
2. GLFW reports normal mode but the custom cursor is not visually rendered; or
3. the cursor is visible but remains at an unexpected position after leaving disabled mode.

## Validation

- Repository search and source inspection completed.
- `git status --short` checked; unrelated pre-existing edits were preserved.
- No source/config gameplay code was changed.
- No build was run.
- No newly named executable or live `events.jsonl` was captured.
- No visual or multiplayer acceptance was claimed.

## Smallest next diagnostic

If runtime confirmation is desired, add bounded `StructuredLogger` records at the existing cursor owners, not a new test: `MouseLock::set`, `PauseMenu::toggle/close`, the spectator freecam cursor branch, and the final per-frame cursor reconciliation. Each record should include requested and actual GLFW cursor mode, `MouseLock::locked()`, `PauseMenu::isOpen()`, keyboard-enabled state, `freecamEnabled/anyFreecam`, replicated actor state, and whether the custom cursor was successfully installed. Reproduce: dead Juggernaut player -> spectator freecam -> L -> Escape -> move over a pause-menu button. The first record showing `GLFW_CURSOR_NORMAL` followed by `GLFW_CURSOR_DISABLED`, or normal mode with a successfully installed cursor but no visible pointer, will separate the two remaining causes.

## Human review still required

A human should reproduce the exact Juggernaut round and inspect the live journal while pressing L and Escape. The L cause is already source-confirmed; the Escape cursor visibility cause remains pending runtime evidence.
