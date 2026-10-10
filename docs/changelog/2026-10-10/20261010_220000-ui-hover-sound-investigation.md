# UI hover sound investigation

## Scope

Investigation only. No gameplay, UI, audio, configuration, build, or executable files were changed.

## Routed guidance used

- `docs/ROUTER.md`
- `docs/specs/gui/guiv2.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/live-development/hot-audio-contract.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/asset-checker-v1.md`

## Finding

- Severity: high
- Type: implementation/runtime ownership conflict
- Expected behavior: a UI hover sound should be emitted once when the logical hovered widget changes, not once per render pass or frame.
- Code path: `src/gui/ui-system-buttons.cpp:42-47` assigns the current widget to `gHoverOwnerKey`; `src/gui/ui-system.cpp:88` clears that key at every `uiBeginFrame`; `src/gui/ui-system.cpp:141-154` compares it with the persistent previous key and plays `playMenuHover()` on each detected enter.
- Actual behavior: the same widget repeatedly enters and exits when a later UI pass begins with no hovered widget. That later pass changes the shared previous-hover state to empty, so the next gameplay UI pass sees a fresh enter.
- Confirming runtime evidence: build 1915 client run `20261010_215428`, console log `logs/10-10-2026/Gameterminal_log_175437.txt`; while over `leaveButton`, the log records 1,809 enters, 1,809 exits, and 2,899 `ui/hover` starts. The first sequence is `UI HOVER ENTER leaveButton` -> `SOUND playing event=ui/hover` -> `UI HOVER EXIT leaveButton` -> next-frame enter.
- Likely triggering owner in the reported dead/pause scenario: `DevOverlay::render()` at `src/devtools/dev-overlay.cpp:40-66` opens a separate UI pass with no widgets after `engineTickUI` at `src/engine/engine-tick.cpp:161-164`. Its `uiBeginFrame` clears the hover key and its `uiEndFrame` commits the empty state. The run initialized dev-overlay notifications at console-log lines 63-65, so this pass was active during the first repeated-hover interval.
- Related reproducers: `Terminal::render()` and `drawDebugLabels()` also use the same global UI begin/end protocol. The console log later shows the terminal opening at line 16843 and closing at line 18826; the repeated hover pattern is therefore not specific to the pause-menu button geometry.

## Evidence boundaries

- The supplied canonical journal `logs/2026-10-10/20261010_215428/events.jsonl` identifies the exact client executable (`.dev/builds/1915/mimita.exe`) and records the pause cursor transitions, but it does not capture the UI hover `printf` or audio-start events.
- The console log is the direct runtime evidence for the repeated hover/audio sequence.
- No new build, self-test, diagnostic, or human playtest was run because the request was investigation-only and the existing executable/log already contain the failure.
- No code fix is recommended as landed. The narrow fix boundary is to make hover-edge state belong to one complete interactive UI frame/pass, or otherwise prevent non-interactive overlay passes from resetting the canonical hover owner; the exact implementation choice remains for the next approved change.

## Status

Root cause is source-confirmed and runtime-confirmed: hover-edge state is shared across independent UI passes, and an empty later pass resets it. Human acceptance after a future fix remains required: dead/spectator state -> open pause menu -> hover and click Leave/Reset/Help without repeated sound or blocked clicks.
