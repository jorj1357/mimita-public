// 2026-11-16 14:58 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Honest frame accounting and host poll watcher

# Task

- Task ID: session-20261116-192845
- Session ID: session-20261116-192845
- Owner: opencode
- Focus: performance
- Summary: Make the perf overlay and journal report last-completed-frame host/game/swap/sleep/unaccounted, and move ready-manifest filesystem/JSON polling off the hot main thread.
- Status: BLOCKED
- Date, time, timezone: 2026-11-16 14:58:00 EST (2026-11-16T19:58:00Z)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7efe9c172acb80bdb834a87d6550a5384bd71358
- Final commit: 7efe9c172acb80bdb834a87d6550a5384bd71358
- Uncommitted follow-up: `src/host/host-runtime.cpp` `snapshot.error.c_str()` fix

# Pre-existing changes

At session start, `git status --short` already contained unrelated edits:

```text
 M config/accounts/default.json
 M config/analytics.json
 m devscripts/blender-ai
 M docs/changelog/2026-11-16/20261116_111211-host-cold-link-decoupling.md
 M docs/handoffs/hot-reload-cutover-checklist-20261114.md
 M docs/handoffs/hotness-cutover-direct-plan-20261116.md
 M docs/specs/hotreload/hotreload.md
 M docs/specs/open-gameplay-issues.md
 M src/game/player-core-migration.cpp
 M src/game/player-core-migration.h
 M src/main-systems.cpp
```

A concurrent agent committed those edits plus this session's source edits as
`7efe9c17` while this session was still working. The commit also added
`HostRuntime::activeGameModule()` in `src/host/host-runtime.h`, which this
session did not write. This session did not create, revert, or claim those
unrelated edits. End-of-session status:

```text
 m devscripts/blender-ai
 M src/host/host-runtime.cpp
?? docs/changelog/2026-11-16/20261116_192902-honest-frame-accounting-and-host-poll-watcher.md
```

# Requested behavior

1. Make the perf overlay account for all frame time instead of showing a large
   false `UNACCOUNTED`.
2. Show real host/game/swap/sleep buckets and the largest completed-frame
   scopes.
3. Keep the accounting change hot (game DLL, same PID, no EXE rebuild).
4. Stop running `std::filesystem`/JSON ready-manifest checks on the hot main
   thread; nothing should wait for that work.

# Specification alignment

- `docs/specs/performance/performance.md`: measure before optimizing; target
  consistent frame times; always-on `performance.fps` is aggregated once per
  second, not per frame.
- `docs/specs/debug-logging/debug-logging.md`: report unaccounted frame time;
  do not hide missing time; unaccounted frame time target is <= 0.20 ms; JSONL
  summaries include present/swap and unaccounted time.
- `docs/specs/hotreload/hotreload.md`: ordinary game behavior is hot in the
  replaceable game DLL; cold core keeps window/device/swap, loader/barrier,
  file-watch mechanism, and process lifetime.
- `docs/specs/hotreload/hot-client-live-fix.md` Phase 1: "No filesystem stat,
  open, or read runs on the hot main thread." The old 250 ms ready-file read
  violated this.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`: the root dev loop owns
  all builds. This session did not run a compiler or build request.

# Exact implementation changes

## File: `src/perf/perf-accounting.h` (new, game-only)

- Lines: whole file (31 lines).
- Old behavior: none.
- New behavior: `PerfFrameAccounting` holds `cycleMs`, `hostGapMs`, `frameMs`,
  `rootAccountedMs`, `unaccountedMs`, `swapMs`, `sleepMs`, top 16 scopes, and a
  copy of `PerfTimes`. `gPerfFrameAccounting` is the single extern instance.
- Reason: the overlay needs last-completed-frame data without changing
  `perf.h`, which resident/bootstrap sources include.
- Ownership: game only. No resident source includes this header.

## File: `src/perf/perf.cpp`

- Lines/functions: include; `gPerfFrameAccounting`; `finalizeFrameAccounting`;
  `Perf::beginFrame`; `Perf::addTime`; `Perf::addChildTime`.
- Old behavior: overlay subtracted the current partial frame's legacy
  `PerfTimes.total` from the previous full `gFramePacer` cycle. `Swap` and
  `Sleep` were never mapped from `FramePacing::BeforeSwap` /
  `FramePacing::PacerSleep`.
- New behavior:
  - At the next `Perf::beginFrame`, before `perfResetScopes()`, finalize the
    completed frame from `gPerfScopes` and the updated
    `gFramePacer.frameTimeMs()`.
  - `hostGapMs = max(0, cycleMs - lastFrameTimeMs)`, naming all work between
    game frames.
  - `rootAccountedMs` sums non-crossed `EngineTick::*` root scopes.
  - `unaccountedMs = max(0, frameMs - rootAccountedMs)`.
  - Top 16 scopes by self time are copied, excluding `ProfilerOverhead`.
  - Emits a rate-limited 1 Hz `performance.frame-accounting` event while
    `perf_report`/deep profiling is on, with cycle/host/frame/root/unaccounted/
    swap/sleep and the top 8 scopes.
  - Maps the real swap/pacer-sleep timer names.
- Reason: the old number compared two different frames and two different timer
  systems. Build is not behavior; this code only makes the next runtime run
  measurable.

## File: `src/perf/perf-overlay.cpp`

- Lines/functions: `Perf::renderOverlay` perf-report block, frame-scope list,
  timer list, NPC/memory panel offsets.
- Old behavior: `CPU` = partial legacy timer sum; `UNACCOUNTED` =
  previous cycle minus that partial sum; `[Subsystem Timing]` showed only the
  legacy timer subset; NPC/memory panels used hard-coded 21/12 line offsets.
- New behavior:
  - `HOST GAP`, `GAME`, `SWAP`, `SLEEP`, `UNACCOUNTED` with percent of cycle.
  - `[Frame Scopes]` shows the real completed-frame scope tree by self time.
  - `[Timers]` keeps Audio/PostFX/Networking etc. with percent of cycle.
  - NPC/memory panels follow the actual last drawn Y, removing the stale
    fixed offsets.
- Reason: make Audio (0.06 ms) and PostFX (0.34 ms) read as sub-1% costs and
  expose the real missing-time owner.

## File: `src/engine/whole-game-entry.cpp`

- Lines/functions: `tickWholeGame`.
- Old behavior: `executePendingStartupCommands()` ran between
  `EngineTick::Simulate` and `EngineTick::Render` outside both roots.
- New behavior: scoped as `EngineTick::StartupCommands`.
- Reason: include that in-frame time in `rootAccountedMs`.

## File: `src/host/host-runtime.h` and `src/host/host-runtime.cpp`

- Lines/functions: constructor/destructor, `startReadyWatcher`,
  `stopReadyWatcher`, `startFromEnvironment`, `shutdown`, `poll`.
- Old behavior: `HostRuntime::poll()` ran `std::ifstream`, JSON parse,
  `std::filesystem::absolute/equivalent/exists`, and activation checks on the
  main thread every 250 ms.
- New behavior:
  - A background watcher reads/parses `readyPath_` every 250 ms and stores a
    `ReadySnapshot` under a mutex; it never touches game state.
  - `poll()` uses `try_lock`; if the watcher is publishing, the main thread
    skips that frame instead of waiting.
  - Only candidate activation (path checks, barrier quiesce, DLL load) remains
    on the main thread, and only when a candidate is pending.
  - Watcher is stopped and joined in `shutdown()` and in the destructor.
- Reason: satisfy the hot-client Phase 1 rule and remove repeated filesystem
  work from the hot thread.
- Cold boundary: `src/host/host-runtime.cpp/.h` are `resident` ownership, so
  this needs the EXE build. The file-watch mechanism remains cold; the
  decision/activation stays on the main thread.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `src/perf/perf-accounting.h` | absent | new accounting record | completed-frame buckets | delete file |
| `src/perf/perf.cpp` | partial-frame subtraction | finalize previous frame + event | honest unaccounted | `git checkout -- src/perf/perf.cpp` |
| `src/perf/perf-overlay.cpp` | legacy timer list | host/game/swap/sleep + real scopes | show real owner | `git checkout -- src/perf/perf-overlay.cpp` |
| `src/engine/whole-game-entry.cpp` | startup commands unscoped | `EngineTick::StartupCommands` | close in-frame gap | `git checkout -- src/engine/whole-game-entry.cpp` |
| `src/host/host-runtime.h` | inline poll members | watcher members | off-thread file I/O | `git checkout -- src/host/host-runtime.h` |
| `src/host/host-runtime.cpp` | main-thread file read | watcher + try_lock consume | no main-thread wait | `git checkout -- src/host/host-runtime.cpp` |
| `devscripts/blender-ai` | pre-existing submodule dirt | untouched | not this session | leave alone |
| `docs/changelog/.../20261116_192902...md` | absent | this record | session history | delete file |

# Contract / ABI changes

- Contract: none. No module ABI, packet, or state schema changed.
- Version bump: none.
- Compatibility: existing game DLL ABI unchanged.
- Reverse: revert the source files.

# State / schema changes

- Domain: none.
- Old schema: n/a.
- New schema: n/a.
- Migration: n/a.
- Reverse: n/a.

# Deleted paths

- None.

# Diagnostics

- Owner/category: `Perf::finalizeFrameAccounting`, category `Performance`.
- Input: completed `gPerfScopes`, `gFramePacer.frameTimeMs()`,
  `PerfState.lastFrameTimeMs`.
- Decision: split cycle into host gap + game roots + unaccounted.
- Output: `performance.frame-accounting` with `cycle_ms`, `host_gap_ms`,
  `frame_ms`, `root_accounted_ms`, `unaccounted_ms`, `swap_ms`, `sleep_ms`,
  `scopes`.
- Failure or rejection reason: none.
- Rate limiting: one record per second, only while `perf_report`/deep profiling
  is on.

# Validation

- Focused skill paths and results:
  - `docs/skills/efficiency-checker-v1.md`: no P0/P1 finding in the new code.
    The scope scan is once per frame, bounded by `MAX_SCOPES_PER_FRAME`, and
    uses stack storage. The old main-thread ready-file read was the hot-path
    finding and is now on the watcher thread.
  - `docs/skills/logging-checker-v1.md`: the new event is owned by
    `Perf::finalizeFrameAccounting`, category `Performance`, one record per
    second, and includes the decision inputs and all accounting outputs.
  - Note: `PerfTimes` and `gPerfScopes` remain two timer systems. This change
    reads roots from `gPerfScopes` and legacy timers from `PerfTimes`; merging
    them is follow-up work, not part of this patch.
- Tests and exact commands: none run.
- Build status:
  - EXE build succeeded after the follow-up fix (`last_build_result=success` in
    `.dev/state.json`).
  - Game DLL build compiled `src/perf/perf.cpp` successfully (57.65 s in
    `build/game/progress.log`).
  - `src/perf/perf-overlay.cpp` and `src/engine/whole-game-entry.cpp` were
    cache-skipped from a previous successful compile; no errors.
  - The game DLL build as a whole failed on an unrelated concurrent file:
    `src/physics/movement/actor-contact-policy-module.cpp:13: fatal error:
    module-identity.h: No such file or directory`.
  - No new game generation was published after this session's edits; the
    active generation remains 277.
- Runtime or hot-reload evidence: none. Same-PID activation of the accounting
  overlay was not observed.
- Output files: none.

# Measured evidence

- Before values: user screenshot: `FRAME: 62.39ms`, `CPU: 1.49ms`,
  `UNACCOUNTED: 60.91ms`, `Audio: 0.06ms`, `PostFX: 0.34ms`.
- After values: not measured in runtime.
- Timestamps: not measured.
- Tick/frame/network measurements: not measured.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no confirmed behavior break;
  the old value was a measurement-window bug, and runtime proof is pending.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified:
  - `UNACCOUNTED` near zero in a live run.
  - `HOST GAP + GAME + UNACCOUNTED = CYCLE`.
  - `Swap`/`Sleep` populate from the real scopes.
  - The host watcher actually removes main-thread filesystem stalls.
  - The unrelated `module-identity.h` build blocker is resolved.

# Agent handoff context

- Owner: opencode
- Focus: performance
- Files to check: `src/perf/perf.cpp`, `src/perf/perf-overlay.cpp`,
  `src/perf/perf-accounting.h`, `src/engine/whole-game-entry.cpp`,
  `src/host/host-runtime.cpp`, `src/host/host-runtime.h`.
- What another agent should not redo: do not rebuild the accounting from
  `PerfTimes.total`; do not move the ready-file read back onto the main thread.
- Next exact step: resolve the unrelated
  `src/physics/movement/actor-contact-policy-module.cpp` missing
  `module-identity.h` failure, publish a game generation, then run with
  `perf_report 1` and inspect `events.jsonl` for
  `performance.frame-accounting`.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 7efe9c172acb80bdb834a87d6550a5384bd71358 / 7efe9c172acb80bdb834a87d6550a5384bd71358
- Git status: captured below
- Diff stat: captured below
- Build identity: `.dev/state.json` and `build/hot-reload/status.json` were read.
- Generation: 277 active; no new generation published
- Artifact: build\game\generation-277\game-g277.dll
- Journal path: C:\mimita-v9\logs\2026-11-16\20261116_180054\client-01\events.jsonl
- Session ID: session-20261116-192845

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: none.

## Captured git state

```text
 m devscripts/blender-ai
 M src/host/host-runtime.cpp
?? docs/changelog/2026-11-16/20261116_192902-honest-frame-accounting-and-host-poll-watcher.md
```

## Captured diff stat

```text
 devscripts/blender-ai     | 0
 src/host/host-runtime.cpp | 4 ++--
 2 files changed, 2 insertions(+), 2 deletions(-)
```

## Captured changed paths

```text
M	devscripts/blender-ai
M	src/host/host-runtime.cpp
```

## Captured build identity

```json
{
  "exe_build": {
    "last_result": "success",
    "active_build_phase": "build_complete"
  },
  "game_build": {
    "status": "failed",
    "detail": "game build failed with exit code 1",
    "failure": "src/physics/movement/actor-contact-policy-module.cpp:13:10: fatal error: module-identity.h: No such file or directory"
  },
  "hot_reload": {
    "generation": 277,
    "status": "ok",
    "progress": "failed/failed"
  }
}
```
