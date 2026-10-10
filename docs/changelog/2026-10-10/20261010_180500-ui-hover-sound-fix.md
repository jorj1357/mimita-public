# UI hover sound edge fix

## Scope

Implemented the confirmed UI hover-sound regression. Existing unrelated working-tree edits were preserved.

## Routed documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/gui/guiv2.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/live-development/hot-audio-contract.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/asset-checker-v1.md`

## Change

The old contract was `uiBeginFrame(GLFWwindow*, const char*)`; every UI pass cleared `gHoverOwnerKey`, and every `uiEndFrame()` compared and committed hover transitions. The new contract is `uiBeginFrame(GLFWwindow*, const char*, bool ownsHoverInput = false)`.

`src/gui/ui-system.cpp:47,69-90,144` now tracks whether the current pass owns interactive hover state. Only an owning pass clears the current hover key and emits hover enter/exit transitions. `src/engine/engine-tick-ui.cpp:92` and `src/gui/gui-main.cpp:664` mark the gameplay/menu passes as owners. Overlay, debug-label, terminal, and notification passes retain their existing rendering behavior but cannot erase menu hover state.

This keeps the hover sound edge-triggered: entering a button plays once, staying over it is silent, and moving to another button creates the next edge.

## Validation

- `python build_agent.py`: `BUILD SUCCESS`, 83 compiled, 466 skipped, return code 0.
- Fresh executable: `.dev/builds/1920/mimita.exe`.
- `--versioninfo` completed successfully and reported `RUN_ID=20261010_180430`; the printed journal path was `logs/10-10-2026/20261010_180430/events.jsonl` in the process output.
- `git diff --check` found no whitespace errors in the changed files; it reported pre-existing whitespace warnings in unrelated modified files.
- No live gameplay or human click acceptance was performed in this session. The existing running executable was not restarted or replaced.

## Status

PASS_WITH_HUMAN_REVIEW. Source and build evidence are complete. Human review remains: use build 1920, enter the dead/spectator pause menu, hover Leave/Reset/Help, confirm one sound per actual hover transition, and click Leave successfully.
