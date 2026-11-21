# DLL generation timers and live activation proof

Date: 2026-11-21 15:29 EST  
Issue: `issue-136d4553ff92`

## Changes

- Added a prominent whole-game DLL elapsed/total timer and recent generation history to the dev-loop dashboard.
- Added separate latest-published and latest-in-game-activated generation display. The activation lookup reads bounded portions of the current and rotated server journal files, then caches the latest proof and checks new events.
- Changed dependency scan progress text to show the processed depfile count and percentage when that denominator exists.
- Added worker subtimers for dependency prepass/path fan-out/stat checks, link response-file preparation and linker execution, and publication rename/hash, metadata, external dependency collection, and cache-marker writing.
- Added exact cache invalidation reasons, per-TU dependency-check min/average/max and slowest paths, compile time summed across TUs and min/max, and link input counts/bytes.
- Made the progress heartbeat write at 2Hz while running so phase/subphase elapsed timers continue updating while the worker is busy.
- Preserved phase/subphase/scan/compiler diagnostics in future attempt summaries.
- Documented requirements in `docs/specs/dev-loop/build-time-observability.md` and `docs/specs/dev-loop/dashboard.md`.
- Added detailed timings, cause analysis, limits of current CPU/RAM evidence, live activation proof, and Blink/cr.h/Flecs/Ninja comparison in `docs/reports/20261121_152900-dll-generation-latency-and-activation.md`.

## Evidence

- Existing root dev-loop attempt G432: `game-dll-20261121T201417234689Z-2d9df4`, 172.333s end to end, 12 TUs compiled and 630 reused.
- Dependency prepass was 85.309s; path/fan-out loop was 84.749s. The loop calls `dependency_relative()` and `Path.resolve()` once for each dependency token. There are 160,547 tokens in the 642-entry index.
- All 11 stale object reasons named the generated `build/game/identity/module-identity.h` as newer than the object.
- Link took 11.022s over 642 objects; publish phase took 1.674s with nested costs recorded.
- Server `server-01` PID 7556 activated G432 at `2026-11-21T20:17:11.211Z`, about two seconds after publish completion.
- G430's 50.096s publish phase is not retrospectively attributable; G432's nested publish timings show a 1.674s publish in the later attempt.
- Python AST parsing passed for `devscripts/dev_loop/gui.py`, `devscripts/dev_loop/game_build.py`, and `devscripts/game_build_worker.py`. The dashboard data reader returned G432's activation identity and generation-history rows.
- No additional game process was launched; no C++ source, compiler selection, or main build configuration was changed.

## Limitations

- The activation event proves the DLL generation loaded in the existing server process; it does not prove a visible NPC behavior change for this source edit.
- G432 did not include contemporaneous system CPU/RAM or compiler-process-tree samples. Host-resource causality remains unproven.
- The 84.749s scan timer measures path normalization plus external classification/fan-out. A new field isolates `dependency_relative()` timing from external classification on the next relevant build; it does not retroactively separate G432's 84.749s.
- The running dashboard and daemon require controlled restart to load the new Python code. The healthy game server must remain in place through that restart.
