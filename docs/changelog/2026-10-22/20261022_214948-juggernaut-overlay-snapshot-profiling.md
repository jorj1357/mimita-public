# Juggernaut overlay and snapshot profiling

- UTC timestamp: 2026-10-22T21:49:48Z
- Eastern display time: 2026-10-22 17:49:48 EDT
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a`
- No commit created.

## Scope and pre-existing changes

Continued the live Juggernaut performance investigation. Preserved the existing uncommitted edits in `config/analytics.json`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/open-gameplay-issues.md`, `src/engine/engine-tick.cpp`, `src/network/multiplayer-packets.cpp`, `src/perf/perf-frame.cpp`, `devscripts/perf-watch.py`, and the earlier changelog files. This continuation used the already-present stale-readiness fix in `devscripts/dev-loop.py` and render overlay timers in `src/engine/engine-tick-render.cpp`.

New source changes in this continuation:

- `src/network/multiplayer-context.h:476`, `showDebugOverlay`: changed default `true` to `false`; existing F3 toggle remains.
- `src/network/multiplayer-tick.cpp:210-212, 415-421, 457-469, 551-568, 598-600`: added timers to separate snapshot entity-loop, avatar apply/fallback model load, interpolation insertion, initial replica pose, and membership cleanup.
- Added `docs/regressions/2026-10-22/dev-loop-stale-ready-suppressed-launch-REG.md` for the confirmed false-ready launch failure; it remains `ATTEMPTED FIX (1)` pending human review.

## Evidence and interpretation

- Release build 2853, run `20261022_214006`: 67 NPCs were active; the network debug overlay was absent from the frame scopes. Across 10,140 logged frame samples, 1,280 exceeded 4.17 ms (12.62%); worst frame 1,168 ms. Host samples over about 25 seconds showed private bytes rising from about 1.22 GB to 1.50 GB and available RAM about 5.02 GB to 5.08 GB.
- This run improved on release build 2852 with the overlay enabled: that run had 18.65% logged budget misses and a roughly 4,033 ms frame whose `Rendering::Debug::NetworkOverlay` max was roughly 3,999 ms. The NPC scene overlay itself measured about 0.1 ms per 60-frame window. These are separate live runs, so the comparison is indicative, not a controlled benchmark claim.
- Release build 2854, run `20261022_214339`: snapshot `EntityLoop` was far more expensive than `MembershipCleanup` (worst about 829 ms versus 0.19 ms). Avatar apply and fallback scopes were added because avatar load/rebind events coincided with the spike.
- Release build 2856, run `20261022_214656`: 67 NPCs. In the worst measured snapshot window, entity-loop max was about 679 ms while membership cleanup max was 0.19 ms; avatar apply took 7.37 ms total, fallback model loading 4.47 ms, and initial replica pose 4.62 ms. These isolated calls therefore do not explain the 679 ms loop time.
- Release build 2857, run `20261022_214759`: across 10,980 logged frame samples, 1,396 exceeded 4.17 ms (12.71%); worst frame was about 1,209 ms. The worst steady-state network window had `Network::SnapshotApply::EntityLoop` at about 707 ms over 55 snapshot calls, with about 3,685 interpolation insert calls totaling 0.9 ms, initial replica pose 4.62 ms, avatar apply 7.37 ms, fallback loads 4.47 ms, and membership cleanup 0.27 ms. This leaves most of the snapshot-loop CPU time unattributed and calls for another narrow profile inside the remaining per-entity work.
- The 1,209 ms worst frame was at initial frame 1 and was mostly startup `State`; treat it separately from steady-state actor simulation. The 707 ms network snapshot spike was later in the run.
- Host samples for build 2857 showed private bytes rising from about 1.10 GB to 1.44 GB over 12 seconds and available RAM staying near 2.5 GB. The test client was stopped after the bounded sample; the dedicated server remained alive.
- The 440 MB allocation delta in one resource window is process-wide timing instrumentation, not proof that snapshot code retained or allocated all those bytes. Work from concurrent avatar preparation can overlap the scope. Logger flush time was only 3.6 ms in that same window.

## Build and runtime validation

- Canonical release builds through `python devscripts/dev-loop.py`: 2853 success; 2854 success; 2855 failed because the temporary nested performance scope reused the macro's same-scope guard name; added an explicit nested block and build 2856 succeeded; build 2857 succeeded with the finer scopes.
- Exact tested executable: `.dev/builds/2857/mimita.exe`, release. Version info was captured before launch.
- Exact journal: `logs/2026-10-22/20261022_214759/events.jsonl`.
- Runtime: visible Juggernaut, map `dust2cyberiav4`, matching live client/server build IDs, 67 NPCs; client stopped through the dev loop after the bounded sample. Dedicated server stayed alive.
- `git diff --check` found no whitespace issue in the new source edits; it still reports pre-existing trailing whitespace in `docs/specs/dev-loop/dev-loop.md`.
- No unit tests were added or run. The release builds and real-runtime journals are separate evidence; no 60-second zero-miss acceptance was achieved.

## Next evidence step and review still needed

Do not treat the 240 FPS target as met. The overlay default-off change removed the measured 4-second overlay stall in the next run, but remaining runs still had about 12.6-21.7% budget misses and network snapshot spikes. Add bounded phase timing to the uninstrumented part of `processSnapshotEntities`, separating per-entity map/state updates and actor diagnostics from profiler/allocation instrumentation, then rerun the same 67-NPC release scenario. Human visual acceptance of the overlay default and the final performance result remain outstanding.

## Routed documents and focused skills

- `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/README.md`
- `docs/skills/efficiency-checker-v1.md`: PASS_WITH_HUMAN_REVIEW; supported owner/timing evidence recorded, 240 FPS remains unachieved.
- `docs/skills/logging-checker-v1.md`: PASS; timers use the existing perf scope owner and sampled journals.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW; debug-overlay default changed, F3 remains available, visual review is pending.

