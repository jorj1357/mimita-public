// 2026-11-28 15:10 EST
/* purpose
* record Phase 0 of the complete-CPU-accounting plan requested by the human
* separate source, build, runtime, and unproven claims
* preserve the exact numbers, run IDs, journals, and build attempt
* this file DOES NOT claim a CPU reduction from the instrumentation itself
* this file DOES NOT rewrite the CPU issue history
*/

# Phase 0 CPU accounting: named threads, per-thread sampler, persistent maxframes

# Task

- Task ID: work-75c9d5739d70
- Owner: npc-cost-and-measurement
- Session: session-bccfd6d19d90
- Focus: performance / CPU accounting (user request 2026-11-28)
- Branch: checkpoint/one-checkout-condensation-20261111
- Build request: `build-request-5230f25bcfd7` (approved `auto:resource_pressure_idle`)
- Build attempt: `game-dll-20261128T200150...` → generation **827**
- Test run: `20261128_200454` (counter_strike just map / dust2cyberiav4 / tdm-validation), stopped after measurement

# Requested behavior

The human asked for complete CPU accounting: every process, every named thread,
function and source line, CPU vs wall time, inclusive/exclusive costs, and an
automatic collector. Phase 0 is the attribution layer only; optimization is a
later phase.

# Source changes

## `src/utils/thread-name.h` (new, header-only)

`mimita::threads::nameCurrentThread(name, role)` sets the OS thread description
(SetThreadDescription resolved dynamically) and writes one
`runtime.thread.started` journal event with `thread_id`, `thread_name`,
`thread_role`. `ThreadNameGuard` adds `runtime.thread.stopped` on thread exit.
Header-only so neither build list changes; no hot-path call.

Wired at: `audio-sound-loader`, `audio-music-predecode`, `perf-watchdog`,
`world-rebuild`, `duel-poll` (game DLL).

## `src/video/video-commands.cpp`

The `maxframes` console command now calls `VideoSettings::instance().save()`.
Before this, the cap applied only to the live pacer and silently reverted to
`config/video-settings.json` on the next generation init
(`main-systems.cpp:1258`).

## `src/gui/menus/settings-menu.cpp`

The Frame Rate Limit slider now calls `gFramePacer.setMaxFrames(...)` as well as
saving the value. Previously the slider changed the saved value but never the
live pacer, so it did nothing until restart.

## `devscripts/perf-watch.py` (rewritten, backward compatible)

- per-thread CSV (`--threads` / `--threads-csv`): thread id, OS name, thread
  creation UTC, user/kernel/total CPU seconds, delta, core %, host %, host CPU.
- `--run <run_id>` discovers server/client PIDs from the run manifest.
- `--pid` accepts multiple targets; native counters only (no PowerShell/WMI).
- actual measured interval per sample (not the configured interval), so a slow
  sample cannot inflate percentages.
- JSON summary with collector PID, host CPU mean/max, sample count.

## `devscripts/cpu-report.py` (new)

Prints the accounting table for a run: per-thread `core%`/`host%`, user/kernel
seconds, thread start UTC, per-process sums, and journal phase numbers via
`devscripts/measure-events.py` (presets `server-tick` and `frame`) rather than a
second journal parser.

## `devscripts/dev_loop/runtimes.py`

Every dev-loop launch now starts the per-thread collector automatically and
writes `<run>/cpu-threads.csv`, `<run>/cpu-process.csv`,
`<run>/cpu-threads.summary.json`, `<run>/cpu-threads.log`.
Disable with `MIMITA_RUNTIME_CPU_SAMPLE=0`. The collector exits when the target
processes exit or after 15 minutes.

## `config/hot-reload/packet-ledger.json` (regenerated)

`python devscripts/packet_registry_codegen.py --check` failed on a stale ledger,
which failed every game-DLL build in the pre-worker codegen stage. Regenerated
with the sanctioned generator; `--check` then passed. No packet source changed.

# Build evidence

- First attempt `game-dll-20261128T195351347755Z-52ab02` failed in pre_worker:
  `codegen check failed: packet_registry_codegen.py` (stale ledger).
- Second attempt, after the ledger fix and a new approved request, published
  generation 827: total 59.3 s, compile 43.2 s, link 4.9 s, 8 compiled / 639
  skipped. Postmortem resource: host CPU mean 89.9 %, minimum available memory
  **29.3 MB**; slowest TUs audio.cpp 38.9 s, duel-queue.cpp 28.1 s.
- Build-blocker mapping: the daemon marked the first retry `blocked` with
  "revision 95 failed" even after the source generation advanced; the request
  had to be released (`status=superseded`) and a new request filed. Recorded as
  build friction, not fixed here.

# Runtime evidence

Run `20261128_200454`, generation 827, client PID 8500 / server PID 29736,
sampled 1 Hz with the new collector:

- `runtime.thread.started` events present and named: server `world-rebuild`
  (tid 30324), client `perf-watchdog` (31292), `audio-sound-loader` (1184),
  `audio-music-predecode` (14548, followed by `runtime.thread.stopped`).
- Thread names are readable from outside the process. One pre-existing
  third-party name appeared: server tid 25664 `juice agent` (libjuice).
- Per-thread CPU over 10 s: client total ~155 % of one core in three hot threads
  (78.1 %, ~39 %, ~37 %); server total ~76 % of one core. The hottest client
  thread reported the string `0xdd100001`, which is not a name this change sets
  and is treated as an unknown/third-party label.
- The hottest threads on both processes are still unnamed because they are
  resident-EXE threads (main frame loop, listen server, network/ICE, replay,
  host job workers). Naming those needs a cold EXE build; that is the next
  experiment, not part of generation 827.

# What is proven / unproven

- Proven: named threads and start/stop journal events work in the game DLL;
  the external collector reads names, creation times, and user/kernel CPU;
  the report runs; the auto-start wiring compiles (`py_compile`).
- Proven (source): the console cap now persists and the settings slider reaches
  the pacer.
- Unproven (runtime): that the cap persists across a hot reload in a live
  session, because the test run had no agent command queue to type
  `maxframes 60`. Verification command for the next session:
  `maxframes 60` in the client console, then confirm `target_fps_max: 60` in
  `performance.fps` after a generation swap.
- Unproven: the empty-server generation-thread-leak hypothesis. The 9-hour
  baseline (0.64 core total, 0.16 core tick, 61 % in three long-lived threads
  created at generation swaps) is recorded in `issue-ed3ac775283d`; the
  confirmation needs the EXE-side names plus five forced reloads.

# Files changed

- `src/utils/thread-name.h` (new)
- `src/audio/audio.cpp`, `src/audio/music-manager.cpp`, `src/duel/duel-queue.cpp`,
  `src/network/server-authored-rebuild.cpp`, `src/perf/perf-frame.cpp`
- `src/video/video-commands.cpp`, `src/gui/menus/settings-menu.cpp`
- `devscripts/perf-watch.py`, `devscripts/cpu-report.py`,
  `devscripts/dev_loop/runtimes.py`
- `config/hot-reload/packet-ledger.json` (regenerated)
- `.dev/perf-captures/live-baseline-*`, `.dev/perf-captures/gen827-*` (artifacts)

# Next experiment

1. Name the resident-EXE threads (main frame loop, `listen-server-sim`,
   `server-room-register`, `ice-coordinator-poll`, `json-file-watch`,
   `replay-save-worker`, `log-capture`, `host-ready-watch`, host job threads) in
   one cold EXE build, then re-run the collector and confirm every hot thread is
   named.
2. Force five generation reloads on one idle server and compare live thread
   count and per-thread CPU before/after. Acceptance: no growth.
3. Then the highest measured single win: apply and prove the frame cap in a live
   session (expected main-thread drop from ~1 core to ~0.15 core at 60 FPS).
