# C++ latency snapshot benchmark and build failure containment

## Scope

Continue the active edit-to-live latency goal, record the latest one-source DLL timing, and clarify which failures can safely fail open. No compiler, linker, source behavior, or build architecture was changed in this turn. The persistent Codex goal remains active.

## Generation 497 measurement

Root dev-loop attempt `game-dll-20261122T140600949534Z-cecbf8` published game-DLL generation 497 in 51.062 seconds. Its queued build note was `501c5851385b`; the single edited path was `src/effects/zombie-tower-effects.cpp`.

| Stage | Duration | Evidence |
|---|---:|---|
| Manifest snapshot | 2.094 s | 1,008 C++ inputs enumerated; cached watcher state used |
| Manifest validation | 0.015 s | Completed |
| Source snapshot | 0.974 s | Junction overlay: 59 directory junctions, 32 hardlinks, 1 copied changed file |
| Packet registry codegen | 2.800 s | Resident-state codegen was a cache hit (0 s reported) |
| Initial source coherence | 2.559 s | Source set checked before worker work |
| Changed-source preflight | 31.330 s | Changed TU compile took 30.720 s |
| Object-cache scan | 1.084 s | Dependency-index prepass was 0.571 s |
| Identity-provider compile | 0.387 s | Only generated wrapper/provider TU; no extra module wrappers |
| LLD link | 3.957 s | 644 object inputs, about 480.6 MB |
| Coherence validation | 0.209 s | Completed |
| Publish | 0.656 s | 224,893,440-byte DLL published |

The earlier real snapshot benchmark was 0.755, 0.774, and 0.828 seconds, with changed and unchanged file hashes verified. The focused snapshot suite passed 19 tests and both changed Python files passed `py_compile` in the previous validation. A mismatched-file hash cleanup test and junction target retention test were included. A real root fallback attempt was not forced, so fallback behavior remains partially proven by tests, not a deliberately induced failed build.

Generation 496 took 118.006 seconds, including 46.877 seconds changed-source preflight, 35.335 seconds linking, and 4.829 seconds object-cache scan. Generation 497 is much faster, but it is not a matched resource-pressure comparison: the intended server PID 26748 exited before the status check. Therefore do not attribute the improvement to process priority or lower PC load. The server has no process left at that PID, so no priority restoration action was possible. Root dev-loop status reported the selected run stopped. Another active task owns the shared NPC live-pressure probe, so this session did not create a competing game instance. Generation 496 had previously been observed in the existing server journal; generation 497 publication is proven, but its runtime activation and behavior are not.

## Failure boundary

Attempt `game-dll-20261122T134127939129Z-7896ef` failed on required changed source `src/network/multiplayer-tick.cpp`: `avatarPending` and `fallbackModelLoaded` were undeclared at lines 724-725, followed by a JSON initializer error at line 731. The candidate did not publish. `build/game/current.json` stayed on successful generation 493, and the existing server continued reporting generation 493. Later attempts could publish generations 495-497.

That is the safe boundary for required C++ build work: reject the bad candidate, preserve and continue running the last-good DLL, and let the coordinator accept a later fixed revision. Reusing an old object for a changed TU and marking the new DLL successful could silently keep the broken behavior and should not be used as fail-open. Optional presentation/report-only helpers may fail open because they do not change correctness of the primary artifact; their failure must remain visible and separately recorded. Existing optional presentation-DLL suppression and goal-continuity evidence are documented in `docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md` and the gold handoff. The linked `dll-fail-soft-measurement` task owns the question whether a required failed candidate blocks later revisions; this evidence was appended to the existing latency issue rather than changing publication rules.

## Next measurable target

The next dominant unit of this attempt is the 30.720-second compile of `src/effects/zombie-tower-effects.cpp`; cache snapshot, object scan, link, and publish are already below 4 seconds here. Compare equivalent one-source builds under the same live Juggernaut/AI pressure profile, then profile this TU's frontend/preprocessor/include cost before deciding on PCH or code-boundary changes. Do not treat a 0.97-second source snapshot as a sub-10-second edit-to-live result.

## Changed files and validation

- Earlier work in this same goal session changed `devscripts/dev_loop/game_build.py` and `devscripts/quality/test_game_build_snapshot.py` to create small-delta immutable junction-backed snapshots, retain their targets, and fall back to full snapshot construction when the overlay cannot be used.
- Focused validation: `py_compile` passed; 19 snapshot tests passed; `git diff --check` passed with line-ending warnings only.
- This turn appended exact measurements and failure-boundary evidence to issue `issue-136d4553ff92` and added this changelog. No game source or compiler input was changed in this turn.
- The active edit-to-live goal is still open. No <=10-second result, low-CPU result, generation-497 runtime activation, or NPC behavior proof is claimed.

## Build-worker scheduling priority finding

Static inspection after the gen497 timing revealed a resource-control setting not exposed by the dashboard: `devscripts/dev_loop/game_build.py:1633` passes `--priority high` to every DLL worker. `devscripts/game_build_worker.py:106-109` maps that choice to Windows priority class `0x00000080`, and `set_process_priority` at lines 154-160 applies it with `SetPriorityClass` to the worker process. The worker docstring also says high process priority by default. This is a plausible source of foreground/game scheduling pressure; it is not evidence that it caused the measured 30.72-second TU compile or that the game itself is using high priority. Generation 497 had `jobs=10` and the high-priority worker setting. A matched high-vs-normal experiment should measure host CPU, worker CPU, game/server CPU and tick health, build wall-time, and responsiveness. This finding is appended to issue `issue-136d4553ff92`; no shared build policy was changed because other tasks have concurrent edits in the build path.

## Generation 497 runtime and CPU follow-up

A later read-only root dev-loop status found a new active run, `20261122_141131`, started after the earlier stopped-run observation. Server PID 33260 and client PID 11908 both map the loaded module `build/game/generation-497/game-g497.dll`; this confirms generation 497 activation. The server journal's `game.hotness.summary` reports 19 NPCs, 1 player, 0 server failures, and 0 violations at tick 27613 / t=472.6s. This confirms the DLL loaded, not that the comment-only canary changed gameplay.

A matched five-second psutil sample measured 12 logical CPUs and 94.5% host CPU utilization. Server PID 33260 used 102.8% of one core (8.57% of total machine capacity); client PID 11908 used 220.2% of one core (18.35% of capacity), or about 26.92% combined. Their working sets were 63.8 MB and 285.0 MB, and both game processes were at Windows Normal priority class 32. This is active gameplay with 19 NPCs and one player, so it is not an idle CPU measurement and cannot answer whether an idle MiMITA process meets the <=1% goal.

Other sampled processes included FL64.exe at 8.80% of total CPU capacity, cpptools.exe at 5.63%, pythonw.exe at 4.14%, obs64.exe at 3.15%, and ChatGPT.exe at 2.64%. Host utilization was high, but MiMITA alone did not account for it. No game-DLL worker was active during this sample. A separate 5-second diagnostic across all processes was used to identify candidates, then the second sample measured only the selected process IDs and host CPU to reduce enumeration-window bias.

## Generation 499 resource-pressure measurement and correction

Root dev-loop attempt `game-dll-20261122T142258012829Z-326bd4` published generation 499 in 137.010 seconds while run `20261122_141131` was active. `build/game/generation-499/generation.json` reports one changed source (`src/network/server-npcs.cpp`), two compiles total (the changed TU and identity provider), 641 cached units skipped, jobs=10, priority=high, and LLD. Measured costs: manifest 0.750 s, source snapshot 1.042 s, packet registry codegen 2.279 s, changed-source preflight 67.460 s (changed TU compile 67.005 s), object scan 4.470 s, identity-provider compile 4.999 s, link 36.797 s, and publish 1.457 s. This is a pressure-run measurement, but not a matched comparison with gen497 because the changed TU and concurrent workload differ.

During the build window, a five-second host/process sample measured 100% host CPU across 12 logical CPUs. MiMITA client PID 11908 used 378.0% of one core (31.50% machine capacity) and server PID 33260 used 141.7% (11.81%); combined MiMITA use was about 43.31% of total capacity. The sample did not identify a compiler child or align its interval to an exact build phase, so it does not prove the DLL build caused the remaining saturation. Worker PID 32524 was present with Windows Normal priority 32 in the same broad build window while the worker command requested `--priority high`; this suggests the best-effort `SetPriorityClass` result needs explicit read-back and error logging. It does not yet prove the priority-setting failure caused latency or system load.

Correction: the earlier statement that generation 497 was runtime-activated based on `psutil.memory_maps()` was too strong. Memory maps establish that the generation DLL was loaded/mapped, not which version is currently executing; the shared server and client now map generations 497, 498, and 499. Exact active-generation evidence requires a journal/API signal that identifies the module generation at a function call. No generation-499 behavior change is claimed.
