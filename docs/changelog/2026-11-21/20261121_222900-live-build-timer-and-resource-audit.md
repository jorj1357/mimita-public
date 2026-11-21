# Live build timer and resource audit

- Fixed the DLL dashboard timer so one prominent attempt clock ticks from the
  attempt start and phase/subphase clocks are derived from their own start
  timestamps. Removed the misleading attempt-age-as-phase-elapsed text.
- Confirmed the refreshed dashboard visually while the existing game pair and
  root build remained active; no build, compiler, game configuration, or game
  architecture was changed.
- Captured attempt
  `game-dll-20261121T222152398558Z-ad2cd0`: manifest snapshot took 409.415s
  for 1,008 source entries; source snapshot took 111.612s and hardlinked 2,899
  files. The build published generation 436 in 903.413s. Object scan was
  7.403s, 14 units compiled and 628 skipped, linker 30.683s, publish 8.633s.
- Identified all 11 additional wrapper compiles: each invalidated by the
  generation/revision-specific `module-identity.h`. The existing live-
  development architecture note proposes a runtime identity path; it is not
  implemented in this change.
- Captured a live 15s CPU sample after the DLL build: 85.3% system CPU across
  12 logical CPUs, with three game processes using 20.27%, 10.53%, and 12.56%
  of total machine capacity. The client journal confirms `maxFrames=999`,
  `vsync=false`; recent measured frame windows were 36.5-71.3 FPS. This proves
  substantial load during the observed run but does not establish an idle
  baseline or isolate one hot function.
- Recorded a 32.855s gap between source-snapshot completion and worker input
  validation, plus a 16.689s mismatch between top-level attempt time and the
  nested worker result field. Both need reconciliation.
- Created CPU investigation issue `issue-ed3ac775283d` and added the general
  project note that agents should prefer reliable Python/API/CLI automation
  over Computer Use, using UI automation when visual or direct interaction is
  needed.
- Added a detailed measured plan and acceptance gates in
  `docs/reports/20261121_222900-live-build-timer-and-resource-audit.md` and
  recorded the findings on issue `issue-136d4553ff92`.
- Validation: direct timer helper checks, live attempt view evaluation,
  `py_compile`, and `git diff --check` passed. The captured compile/link
  attempt completed successfully, but its run journal did not prove generation
  436 activated in-game.
