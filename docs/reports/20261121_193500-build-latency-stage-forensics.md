# DLL build latency: stage forensic evidence

**Captured:** 2026-11-21, 19:22–19:39 UTC.

**Scope:** Read-only inspection of completed root-dev-loop attempts `game-dll-20261121T192237066244Z-ab2591` (generation 427) and `game-dll-20261121T193255143084Z-1b7951` (generation 428), plus the worker/dashboard code and one live resource sample. No build or game process was started, stopped, or modified by this investigation.

## What is taking the time?

The measured delay is in building and publishing the DLL candidate. The attempt record does not include DLL activation in the existing game. Generation 428 was published successfully, but the captured runtime status did not prove that the running server loaded it or that a changed behavior became visible.

The recent attempts varied substantially even though both used `jobs=10` and processed a 642-translation-unit manifest:

| Attempt | Result | Total wall time | Compiled / reused | Main measured stages |
|---|---|---:|---:|---|
| Generation 427, `ab2591` | Published | 131.141 s (2m 11s) | 13 / 629 | Object scan 65.390 s; preflight compile 17.781 s; compile phase 10.563 s; link 12.172 s; publish phase 4.344 s |
| Generation 428, `1b7951` | Published | 377.329 s (6m 17s) | 12 / 630 | Object scan 144.375 s; changed-source preflight 60.266 s; remaining compile phase 21.250 s; link 29.766 s; publish phase 27.766 s |

Both builds reused the large majority of cached objects. Generation 428 did **not** compile all 642 units. It compiled the changed `src/actor/player-ai.cpp` plus 11 further stale candidates, but the attempt does not record why those 11 were stale. Generation 427 compiled 13 units and skipped 629. This is evidence that incremental caching works in these attempts, but its invalidation decisions still need a per-file explanation.

Generation 427 is the “pretty quick” run, but it was still 131 seconds from accepted request to published DLL. Generation 428 was 377 seconds for one recorded changed C++ source. Neither timing is a save-to-visible-behavior measurement.

## Detailed timing for generation 428

Attempt `1b7951` started at `2026-11-21T19:32:55.144Z` and completed at `19:39:12.503Z`, successful as generation 428. The exact attempt files are `.dev/build-timings/game-dll-20261121T193255143084Z-1b7951.json` and `.jsonl`.

| Work | Wall time | Share of 377.329 s | Evidence |
|---|---:|---:|---|
| Before worker entry | about 93.3 s | 24.7% | Difference between end-to-end time, worker time, and post-timer finalization; includes code generation and snapshots |
| Packet registry code generation | 49.344 s | 13.1% | `stage.completed` for `codegen:packet_registry_codegen.py` |
| Manifest snapshot | 15.250 s | 4.0% | `stage.completed` event |
| Source snapshot | 14.812 s | 3.9% | `stage.completed` event |
| Changed-source preflight | 60.266 s | 16.0% | `src/actor/player-ai.cpp` alone took 60.150 s to compile |
| Object-cache scan | 144.375 s | 38.3% | `phase.completed` and worker summary |
| Additional compile phase | 21.250 s | 5.6% | 11 additional stale candidates; parallel TU durations exceed this wall interval |
| Link | 29.766 s | 7.9% | Linker phase |
| Coherence validation | 0.281 s | <0.1% | Validation phase |
| Final publish/finalization phase | 27.766 s | 7.4% | Worker phase duration; see mismatch below |

The phase totals overlap the other aggregate fields, so they should not be added to the aggregate `compile_seconds` or `worker_total_seconds`. The worker summary reports `compile_seconds=81.501` (60.266 seconds preflight plus 21.250 seconds later compilation), `worker_total_seconds=257.219`, and `total_seconds=377.329` at the root attempt level.

### A concrete 116-second scan hot path

The 144.375-second object-cache scan has a `scan_prepass_elapsed_s` of **116.466 seconds**. The same attempt reports:

- 642 sources and 160,547 dependency-token occurrences in the persisted 11.77 MB dependency index (about 250 tokens per source; 1,698 distinct tokens in the index).
- `scan_parse_ms=131.992`, `scan_stat_ms=344.604`, and `dependency_check_ms=26,916.737` (the last is the sum of per-translation-unit checks, not a separate wall interval).
- The prepass resolves and classifies every cached token at `game_build_worker.py:1322-1327`. `dependency_relative()` calls `Path.resolve()` at `game_build_worker.py:804-815`; it is called inside that per-token loop. Thus, the current code can do roughly 160,547 path resolutions per attempt even though there are only 1,698 distinct tokens.

This is the strongest current candidate for a fix in the existing build path. The attempt proves that the **prepass** took 116.466 seconds; it does not yet prove how much of that time is specifically spent in `Path.resolve()` versus Python iteration, index validation, or progress work. The next profile should time those operations separately. A safe optimization candidate is to normalize/cache project ownership once per distinct token, or maintain a reverse dependency index while idle, while preserving periodic correctness audits and header/PCH invalidation.

### Other large or unaccounted stages

1. **Packet registry generation (49.344 s).** The root pipeline invokes `packet_registry_codegen.py` on every DLL build (`dev_loop/game_build.py:617-618`). Its `scan_source_references()` walks all `src/**/*.cpp` files (`packet_registry_codegen.py:131-151`), reads each recognized source, then loops over every packet name for each source line. That is a full source-tree content scan for a build whose only reported changed source was `src/actor/player-ai.cpp`. The same stage took 2.672 seconds in generation 427, so the 49-second interval needs function-level timing and resource samples before its cause is certain. Make it input-driven or incremental after measuring it; do not remove correctness checks blindly.

2. **One expensive C++ translation unit (60.150 s).** The measured slowest compile was `src/actor/player-ai.cpp`, in changed-source preflight. This is compile time, not DLL reload time. The current record gives elapsed time per translation unit but not compiler front-end breakdown, CPU time for this compiler process, or memory pressure while it ran. Preserve this exact input/command and profile its include/preprocessor/front-end/code-generation/linkage costs before deciding whether to split it or alter flags.

3. **Publish timer omits most of its phase.** `publish_seconds` is **0.906 s**, but the `publish` phase from `progress.set_phase("publish")` to `progress.finish()` is **27.766 s**. The timer starts immediately before `os.replace` and `sha256_file` (`game_build_worker.py:1841-1858`); after that timer stops, the worker writes generation/current metadata and the cache marker (`game_build_worker.py:1908-1915`). Cache-marker preparation calls `collect_external_dependencies(entries)` (`game_build_worker.py:709-715, 1915`), which reparses each of the 642 depfiles rather than reusing the dependency-index tokens already loaded earlier. These operations are candidates for the 26.860-second gap; separate timings are needed to attribute it exactly.

4. **Root setup before C++ work.** In addition to packet generation, the attempt spent 15.250 seconds snapshotting the manifest and 14.812 seconds snapshotting sources. These are independent of the 60.150-second compile of `player-ai.cpp` and should remain separate in the dashboard/report.

## PC load and running game processes

The computer is an HP Victus 15-fa1xxx with an Intel Core i5-13420H (8 cores / 12 logical processors) and 16.0 GB installed RAM (15.65 GB visible to Windows).

During the active generation-428 attempt, one 10-second process sample showed about **13.79 GB used / 1.86 GB free**. In that sample the game processes collectively consumed about 68% of the machine's CPU capacity, while the build-worker Python process used about 7.2%, Blender about 7.4%, and `cpptools` about 6.2%. A later five-second system counter sample, during the build's publish phase, reported **98.3–99.1% total CPU utilization**. A separate memory snapshot reached 14.27 GB used / 1.38 GB free (about 91% used).

So CPU was near 99% in these observations; RAM was high but below 99%. The build worker was sharing the machine with several game processes and other applications. This makes system contention a credible contributor, but these short samples do not establish how much it added to any particular phase. The build timing records do not persist host CPU/RAM or per-worker process CPU, so historical attempts cannot be corrected for machine load.

At a 19:31 UTC process inventory, five `mimita.exe` processes were present: the long-running server PID 7556 plus two separate `AI Exploration` server/client pairs running an older `.dev/builds/2026-11-21T02-26-50.306117Z/mimita.exe`. The root `--status` snapshot showed only PID 7556 and no clients. The two extra pairs' owners and lifecycle were not established, and they were left untouched. This contradicts the assumption that no MiMITA executable was running at that point; it also means the CPU sample included gameplay workloads.

At the same time, the current DLL pointer reported generation 428, while the shared run shown by dev-loop status was pinned to an older EXE and had no client. The 428 attempt proves publication, not activation in a running game. The `--status` output also showed `game-build status=idle, generation=-` while `build/game/current.json` identified successful generation 428. That status/current-pointer discrepancy should be reconciled.

## Why the percentage sometimes disappears

The GUI intentionally only returns a progress-bar ratio for dependency prepass, object-cache scan, and compile (`gui.py:378-432`). During preflight, link, validation, publish, code generation, and snapshots it displays elapsed time with **“no completion percentage for this phase.”** During dependency prepass it shows a file counter, but the text says “no completion percentage until object checks” (`gui.py:426-427`). This is an explicit UI rule, not proof that the worker stopped.

There should not be a fabricated single overall percentage because a translation-unit count does not predict time: one source took 60 seconds, while other phases ranged from under a second to 49 seconds. The GUI can be more useful without a false ETA by showing honest per-phase progress, such as `430/642 depfiles (67%)`, `player-ai.cpp compile 60.2s`, and elapsed time for link/publish. For packet code generation and snapshots, add concrete completed/total inputs where those counts are meaningful.

## Evidence gaps to close

- **No invalidation reason per TU.** For each of the 11 stale sources beyond the direct changed `player-ai.cpp`, record the exact triggering input (changed header, PCH, flags/compiler identity, missing object, generated wrapper, or cache mismatch) and its path/hash.
- **No breakdown inside the 116.466-second prepass.** Add counters and exclusive timers for dependency-index load/validation, depfile stat/cache misses, dependency token iteration, `dependency_relative`/path normalization calls and time, index writes, and progress/event persistence.
- **No breakdown inside packet code generation.** Time recursive file discovery, manifest owner lookup, file reads, packet-name matching, dispatch scans, and generated-output writes separately.
- **No breakdown after the measured publish timer.** Time DLL rename/hash, current/generation metadata writes, cache-marker serialization, and external-dependency collection independently.
- **No resource telemetry tied to attempt IDs.** Sample host total CPU/RAM plus worker and compiler CPU/RSS at stage boundaries (and optionally a modest interval during active work). Keep this low-overhead; record logical processor count and whether competing DLL workers exist. Compare equivalent runs with the same job limit and ambient load.
- **No save-to-activation proof.** Pair a successful attempt with the same existing game PID, the loaded DLL generation/hash, a generation-activated event, and the first intended NPC behavior event/visible result. Publishing the candidate is only the midpoint.
- **No matched latency set.** Run a warm no-op, a one-function body edit, and a header-fanout edit through the root dev-loop, then report request-to-publication and request-to-first-runtime-result p50/p95 separately. Do not overlap build workers for the baseline.

## Recommended order

1. Keep MinGW, the current root dev-loop, and DLL architecture. Do not switch compilers or introduce Blink/cr.h to address the measured delays yet.
2. Instrument and then optimize the 160,547-token prepass; first candidate: remove repeated filesystem `Path.resolve()` for identical dependency paths and retain a correct reverse index.
3. Profile and avoid the unconditional full-source packet registry scan when its input hashes have not changed; keep generation's output validation.
4. Measure the hidden cache-marker/external-dependency work and reuse the dependency index rather than reparsing every depfile if the profile confirms the cost.
5. Separately profile `src/actor/player-ai.cpp` compile time and current machine contention. Keep job-count comparisons matched; the two attempts above are not a clean `jobs=10` versus another setting benchmark.
6. Improve the GUI to show phase-local counts and timers even when a trustworthy percentage is unavailable.
7. After build-stage latency is under control, run the isolated same-process NPC behavior edit and measure save-to-first-visible-result. Only then compare a function patcher or different reload library if the remaining measured cost is activation.

## Source map

- `devscripts/dev_loop/game_build.py:617-618` — code generators are run for each game DLL attempt.
- `devscripts/packet_registry_codegen.py:131-151` — packet source-reference scan across source files and lines.
- `devscripts/game_build_worker.py:709-715` — complete depfile scan used by external-dependency collection.
- `devscripts/game_build_worker.py:804-815` — dependency path classification and filesystem resolution.
- `devscripts/game_build_worker.py:1279-1434` — dependency prepass, cached object checks, and scan result timing.
- `devscripts/game_build_worker.py:1593-1605` — changed-source preflight compile.
- `devscripts/game_build_worker.py:1841-1858, 1908-1915` — measured DLL replace/hash versus later metadata/cache-marker work.
- `devscripts/dev_loop/gui.py:378-432` — phase-local progress ratio and phases with no completion percentage.

## Evidence boundaries

The records prove that generations 427 and 428 were built and published successfully, and the Windows snapshots prove high host CPU/RAM use at the sampled moments. They do not prove runtime activation, visible NPC behavior, a causal link from system load to the measured duration, or the exact fraction of prepass time spent in `Path.resolve()`. No code, compiler, build configuration, running game, build worker, or dev-loop process was changed by this report task.
