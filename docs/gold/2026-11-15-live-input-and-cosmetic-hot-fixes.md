# Gold: live input and cosmetic fixes in the hot client

Date: 2026-11-15
Context: jorj1357 working with DeepSeek v4.1 Flash, 2026-11-15 11:45

## What this proves

A normally cold-looking client problem was fixed in the running hot client
without closing or replacing the EXE.

- The game DLL had its own static GLFW copy. It could not see the resident
  window's focus, key, or mouse state.
- A hot host-key bridge plus Win32 pointer fallback restored movement, terminal
  input, and a path for mouse/UI input.
- The cosmetic renderer was reusing shader uniforms from the previous
  body/outline draw. It now sets view/projection and resets part/instance/
  outline state before drawing.
- Generations 194–208 activated in the same running clients; g201–g208
  activated in PID 24736 while the client kept running.

## Evidence

- `input.host-key` reached the game DLL.
- `walkforward_held=true` and the local actor collision position changed after
  an injected `W`.
- `input.state.terminal_open=true` after Grave.
- `hot-status.notice` published the generation change live.
- `agent.input look/equip/terminal_toggle` worked without window focus.
- `game.generation.activated` shows generation 194, then 199, in PID 26648.
- `cosmetic.transform` shows the Jason attachment at normal scale with
  `use_part_models=0`, `use_instance_model=0`, `outline_pass=0`.

## Why this matters

The team wants bigger and more complex fixes to happen live, with visible proof,
same PID, and no restart. This is a reference for that workflow.

## Links

- `docs/changelog/2026-11-15/20261115_162500-hot-input-bridge-cosmetic-state-hot-status.md`
- `docs/regressions/gameplay/black-font-glyph-boxes-REG-2026-11-15.md`
- `docs/specs/input/input-router.md`
- `docs/specs/hotreload/hot-client-live-fix.md`
