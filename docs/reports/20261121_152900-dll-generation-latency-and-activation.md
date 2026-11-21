# DLL generation latency and activation forensics

Date: 2026-11-21 15:29 EST  
Issue: `issue-136d4553ff92`  
Scope: measure the existing MinGW game-DLL path, explain the current scan/compile/link/publish costs, and distinguish publication from in-game activation. No build architecture or game source was changed.

## Evidence sources

- G430 attempt: `game-dll-20261121T195311434653Z-0a1309`, summary `.dev/build-timings/game-dll-20261121T195311434653Z-0a1309.json`, JSONL beside it.
- G432 attempt: `game-dll-20261121T201417234689Z-2d9df4`, summary `.dev/build-timings/game-dll-20261121T201417234689Z-2d9df4.json`, live worker snapshot `build/game/progress.json`, and JSONL beside it.
- Published artifact: `build/game/generation-432/game-g432.dll`, revision `b16fc891cccfd3070536b57090d6be587e14047e8e6fef0fd2ab25568c622f6d`.
- Runtime activation: run `20261121_144350`, server `server-01`, PID `7556`, event in the rotated server journal `logs/2026-11-21/20261121_144350/server-01/events-0074.jsonl`.

The root dev-loop was already handling these requests when this investigation began. I did not queue a competing build or launch a game. G432's saved-source change was `src/actor/player-ai.cpp`; G430 also recorded that same changed path.

## Generation comparison

| Generation | Attempt total | Changed source | Compiled / reused | Preflight | Scan | Compile incl. preflight | Link | Publish phase | Result |
|---|---:|---|---:|---:|---:|---:|---:|---:|---|
| G428 | 377.329s | `src/actor/player-ai.cpp` | 12 / 630 | 60.266s | 144.375s | 81.501s | 29.766s | 27.766s | Published |
| G429 | 140.845s | `src/actor/player-ai.cpp` | 12 / 630 | 13.066s | 74.681s | 28.325s | 11.703s | 1.854s | Published |
| G430 | 316.709s | `src/actor/player-ai.cpp` | 12 / 630 | 71.160s | 83.174s | 88.606s | 42.691s | 50.096s | Published |
| G431 | 340.795s | `src/actor/player-ai.cpp` | 12 / 630 | 56.478s | 89.771s | 76.745s | 41.866s | 28.285s | Published |
| G432 | 172.333s | `src/actor/player-ai.cpp` | 12 / 630 | 14.225s | 98.848s | 32.645s | 11.022s | 1.674s | Published and activated |

Across these five attempts, total time averaged 269.6s, with a 140.845s minimum and a 377.329s maximum. This is a descriptive average, not a stable benchmark: the records do not have time-aligned host CPU/RAM samples, and overlap/resource conditions can differ. G430's 71.058s compiler time for `player-ai.cpp` and 50.096s publish phase are outliers against G432's 14.177s and 1.674s. G430's attempt predates the new subphase timers, so those outliers cannot be retrospectively attributed to exact internal operations.

G432's attempt summary is about 2.3s longer than the worker's own `total_seconds`. The outer dev-loop attempt timer is the end-to-end number shown above; the worker timer has a narrower boundary. Do not substitute one for the other.

## Where G432 spent time

G432 started at `2026-11-21T20:14:17.234Z`, published successfully at about `20:17:09Z`, and took 172.333s from attempt start to completion. It used MinGW `g++.exe`, `jobs=10`, and processed 642 source units.

| Stage | Wall time | Detail |
|---|---:|---|
| Request/setup/snapshots before worker phases | about 26.4s | Includes code generation, manifest snapshot, and immutable source snapshot. |
| Changed-source preflight | 14.230s | `src/actor/player-ai.cpp` alone used 14.177s compiler time; queue wait was only 28ms. |
| Object-cache scan | 98.861s (57.4% of attempt) | 85.309s dependency-index prepass, 235ms unique-stat prewarm, 13.274s object checks. |
| Later compile batch | 18.425s | Eleven more translation units compiled in parallel. |
| Link | 11.022s | One GNU linker invocation; 642 object inputs, 665 response-file entries, 47,212-byte response file, 480,254,044 bytes of object data, output DLL 242,552,947 bytes. |
| Coherence validation | 0.394s | Final source-coherence check. |
| Publish phase | 1.674s | 0.491s rename and DLL hash; 1.102s external dependency collection; metadata writes totaled about 15ms; cache marker 27ms. |

Compile wall time is 32.645s when the changed-source preflight and parallel compile phase are combined. Summed per-TU compiler time was 145.946s because the 11 later compiles overlap; it is not extra elapsed time to add to the attempt. Across the 12 compiled translation units, individual compiler times ranged from 5.909s to 14.177s. The fastest was not the changed source.

## The scan is the clearest current bottleneck

The G432 object-cache scan checked 641 translation units, reused 630 objects, and found 11 stale candidates. Its 85.309s dependency prepass breaks down as:

- 84.749s in the dependency path/fan-out loop;
- 18ms parsing depfiles;
- 135ms loading the 11.8MB dependency index;
- 171ms saving the index;
- 235ms prewarming stats for 1,513 unique dependency paths;
- about 0.24s of other loop work.

The index has 642 source entries and 160,547 dependency-token occurrences. The measured 84.749s is the loop that classifies every token and calls `dependency_relative()`. That helper uses `Path.resolve()` at `devscripts/game_build_worker.py:887`. The current attempt did not separately time `Path.resolve()` versus external-path classification, so the 84.749s is strong localization to that loop, not a claim that every millisecond was spent inside the single call. The worker now records those two costs separately for the next relevant attempt.

The per-object freshness checks took 13.274s wall across 641 TUs. Summing their individual dependency-check durations gives 12.959s: minimum 3.570ms, average 20.217ms, maximum 73.964ms. Thus repeatedly resolving 160,547 dependency tokens is the first scan optimization to prototype; the individual freshness loop is materially smaller.

All 11 stale-object reasons are now known: each reports `dependency newer than object` for the same generated input, `build/game/identity/module-identity.h`. The changed-source attempt rewrites the generation identity header, which invalidates 11 module wrapper objects unrelated to the one edited gameplay source. The compiler reused 630 of 642 existing objects, so caching is working, but this generated identity fan-out remains avoidable work to evaluate. A later prototype should test moving generation identity from compiled wrapper input to runtime metadata while preserving identity and hotness checks.

## Publication is not activation

The server journal records `game.generation.activated` for G432 at `2026-11-21T20:17:11.211Z`, with DLL `build\\game\\generation-432\\game-g432.dll`, old generation 431, run `20261121_144350`, instance `server-01`, PID 7556. This is direct evidence that the existing server process activated G432 without starting another game process. Activation occurred about two seconds after the attempt reported completion.

The attempt start to activation interval is about 174s. This is **not** exact save-to-activation latency: the build record lacks a timestamp for the user's physical save event. Future evidence should record first observed source change and report generation, request acceptance, worker start, publish completion, runtime activation, and expected behavior as separate boundaries.

The dashboard's new activation reader searches bounded chunks across the current and recently rotated server journal segments, caches the last activation, and inspects newly appended events. It displays `published G432` separately from `latest live activation G432`, so a future publish without activation remains visibly unproven.

## Machine resource evidence and gaps

The machine is an HP Victus 15-fa1xxx with an Intel Core i5-13420H (8 cores / 12 logical processors) and 15.65 GB of RAM visible to Windows. G432 did not record system CPU, memory pressure, or per-worker process CPU/working-set samples. Therefore it cannot establish whether its 98.8s scan or earlier G430 outliers were caused by CPU, storage, competing processes, or the algorithm itself. A separate earlier G428 attempt had a 10-second snapshot with about 13.79 GB used / 1.86 GB free; a later sample showed 98.3–99.1% total CPU and another memory snapshot reached about 91% used. See [`20261121_193500-build-latency-stage-forensics.md`](20261121_193500-build-latency-stage-forensics.md). That evidence is from a different build and must not be attached to G432 as if it were contemporaneous. `devscripts/perf-watch.py` samples a selected process and system memory, but it does not capture the worker's full compiler process tree or system-wide CPU. The next measurement should capture host CPU/memory plus the worker and its `g++`/`cc1plus` descendants every 1–2 seconds under the same attempt ID.

## What candidate technologies change

| Technology | Potential against these measurements | What it does not fix / decision |
|---|---|---|
| Blink | Function patching could avoid rebuilding and linking the 642-object game DLL for a compatible changed function. On G432, the current full linker step was about 11s; G430's linker was 42.7s. | It does not remove the 85.3s dependency prepass or compile the changed C++ function instantly. The measured 14.2s one-file compile remains. Blink requires the MSVC/PDB workflow recorded in [`../specs/hotreload/live-cpp-dependencies.md`](../specs/hotreload/live-cpp-dependencies.md), while the production DLL currently uses MinGW. Do not switch the main compiler from these timings alone; prototype one function with an isolated MSVC/PDB build and compare compile, patch, and activation separately. |
| cr.h | Could replace or simplify whole-plugin DLL swap/state callbacks and may help with safe reload boundaries. | The measured server activated G432 about two seconds after publish; the minutes were spent before activation. cr.h does not remove cache scanning, C++ compilation, or the GNU link. It is not currently justified as a build-speed fix. |
| Flecs | Could support dynamic component registration and explicit data conversion for future gameplay-state schema changes. | It neither patches C++ code nor speeds this build. It is a data/layout migration tool, not a function hot-reload system. |
| Ninja / build-graph work | A persistent depfile graph or cheaper invalidation index could avoid repeated whole-cache token normalization. | Benchmark it against the current pipeline first. The measured defect is in the custom Python scan, and changing build systems does not itself reduce the 11s linker or 14s changed-file compile. |

The best first prototype is a narrowly measured scanner experiment: deduplicate dependency tokens before normalization, use lexical normalization for ordinary project-relative paths, and avoid calling filesystem `Path.resolve()` for each duplicate token. Preserve external dependency freshness and symlink behavior; compare the same one-file edit and warm no-op through the root dev-loop. In parallel, test whether generated module identity can be runtime data so one changed `.cpp` does not rebuild 11 wrappers. Keep MinGW, the current DLL loader, and the live game architecture until those experiments show measured gains and preserve runtime proof.

## Instrumentation delivered

- The dashboard shows a large elapsed/total timer, recent per-generation timing rows, compiled/skipped counts, phase durations, rebuild reasons, and the latest live activation event.
- The worker now reports live nested scan, link, and publish subtimers; the next attempt also separates dependency path normalization from external dependency classification.
- Per-object stale reasons, dependency-check min/average/max and slowest paths, and summed/min/max compiler durations are included in progress and attempt summaries.
- Worker progress is refreshed at 2Hz even when no file counter changes, so elapsed phase and subphase timers keep moving.
- The parent attempt summary preserves worker phase/subphase details for later dashboard history.

No C++ source, compiler selection, main build command, source ownership, or game architecture was changed. No additional game process was launched. The subsequent isolated NPC behavior/effect proof is outside this timing investigation.

## Full attempt wall-clock reconciliation (2026-11-21 follow-up)

The root attempt clock is the best complete build-attempt boundary in this sample: `build.started` at `20:14:17.235Z` to `build.completed` at `20:17:09.579Z` is **172.333s**. The server's G432 activation event is at `20:17:11.211Z`, so attempt-start-to-activation is **173.976s**. This still is not save-to-activation: there is no timestamp for the physical source save or first file-watcher observation. No first visible NPC behavior event was captured either.

The parent timeline exposes more of the work before C++ compilation than the phase-only summary:

| Pre-worker activity | Wall time |
|---|---:|
| `resident_state_codegen.py` | 0.392s |
| `packet_registry_codegen.py` | 3.642s |
| manifest snapshot (1,008 inputs) | 11.581s |
| manifest validation | 0.011s |
| source snapshot | 6.901s |
| Gaps between recorded pre-worker stages | 3.856s |
| **Build start to worker input validation** | **26.383s** |

Some stage `duration_ms` values also exclude stage-event bookkeeping. For `source_snapshot`, the start/end event elapsed offsets span 7.078s, but the stage reports 6.901s; for `manifest_snapshot`, event offsets span 11.702s while the stage reports 11.581s. The code starts these custom timers after writing the `stage.started` event and stops them before writing `stage.completed`. The outer attempt clock includes that work; the component durations do not. Treat this as boundary/reporting overhead inside the 3.856s pre-worker gap budget until a follow-up splits it explicitly.

After setup, the recorded non-overlapping primary phases were: input validation 0.010s; initial source coherence 0.137s; toolchain setup 0.045s; generation/cache setup 0.010s (nested within setup); identity/cache 0.014s; changed-source preflight 14.230s; object-cache scan 98.861s; vendor compile 0.006s; later compile batch 18.425s; link 11.022s; coherence validation 0.394s; and publish phase 1.674s. The 14.230s preflight is the changed `player-ai.cpp` compile; the later 18.425s batch contains the 11 identity-wrapper rebuilds.

Adding the non-overlapping primary phase durations gives **167.345s**, leaving **4.988s** of the 172.333s attempt outside those phase records. About 3.856s of that is already visible between pre-worker stages. Worker phase transitions account for about 5ms of gaps. The publish-phase completion to root `build.completed` interval is another **1.137s**; the root logged 119ms handling the worker result, leaving roughly one second in process/result finalization not attributed to a named stage. Treat those residuals as real latency, not as zero-cost bookkeeping.

This audit also found a measurement boundary bug in `devscripts/game_build_worker.py`. The worker computes `publish_seconds`, `total_seconds`, and `worker_total_seconds` immediately after DLL rename/hash, before writing current/generation metadata, collecting external dependencies, and writing the cache marker. Thus G432's worker metric says publish **0.497s** and total **170.040s**, while the complete publish phase is **1.674s** and the parent attempt is **172.333s**. The parent attempt clock is authoritative for complete build-attempt elapsed time. The post-hash work is measured in the phase/subphase JSONL but omitted from those worker summary fields.

One additional repeat-work candidate is precise: `publish.external_dependency_manifest` reparsed **642 depfiles** and took **1.102s**, after the dependency prepass had already collected external dependencies. Reusing the scan result is likely cheaper, but the safe implementation must refresh dependencies for translation units whose `.d` files were rewritten during this attempt; do not simply use a stale precompile list without that check.

The full attempt still is not the full edit-to-effect picture. A complete next sample must start at the first source-save/file-watcher event, separate debounce and scheduler delay, include the parent/worker exit and publication completion, then stop at same-process activation and the first event that proves the intended behavior. Sample system CPU, available/committed memory, disk activity, and the worker plus compiler/linker process tree against the same attempt ID. G432 has no aligned host-resource sample, so this evidence cannot assign its scan or compile times to PC contention.
