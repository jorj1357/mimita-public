# Subsecond live C++ build plan

- Inspected G432 stage timing and current build/snapshot/cache code paths.
- Recorded the 98.861 s object scan breakdown, wrapper identity invalidation, 11.022 s link, and 26.383 s pre-worker cost with explicit boundaries and uncertainty.
- Added an ordered measurement and optimization plan with acceptance targets for warm leaf edits, headers, wrappers, compile profiling, link alternatives, and runtime proof.
- Confirmed `src/pch.h.gch` exists (121,165,108 bytes); no compilation/build/game launch was performed.
- No compiler, main build path, runtime architecture, or game configuration was changed.
- Evidence source: G432 timing attempt `game-dll-20261121T201417234689Z-2d9df4`; report is `docs/reports/20261121_162227-subsecond-live-cpp-build-plan.md`.
