// 2026-11-17 10:45 EST
/* purpose
* record the performance report modes, live server Hz, keep-open dev loop,
* and the first measured client/server hot-path fixes
* separate source, build, runtime, and human acceptance claims
* this file DOES NOT claim the 240 FPS or 60 Hz targets are met
*/

# Perf report modes and frame owner fixes

# Task

- Task ID: session-20261117-154358
- Session ID: session-20261117-154358
- Owner: opencode
- Focus: performance
- Summary: Add sampled performance report modes, server RX Hz, 1 Hz report records, keep-open dev-loop builds, and target measured client/server hot paths.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-17 10:45:05 EST (2026-11-17T15:45:05Z)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7ff11a91b7775d3dcdefdcdf487dd44983a53097 (plus concurrent work)
- Final commit: working tree at time of writing

# Pre-existing changes

The shared checkout contains concurrent edits outside this session, including
`build.py`, `devscripts/dev_loop/game_build.py`, `devscripts/dev_loop/gui.py`,
`config/analytics.json`, documentation moves, and other changelogs. This session
preserved them and did not claim them.

# Requested behavior

1. Make mode 2 the default compact report, add mode 3 for full detail sampled at
   1000 ms, and keep mode 1 live/full.
2. Put server RX Hz and local actual server Hz in the report.
3. Log performance results at 1 Hz so AI can see live `events.jsonl` changes
   across code edits and game-DLL generations.
4. Keep the EXE open during builds; never auto-replace a live process after a
   successful build. `--auto-restart` is dead-client recovery only.
5. Run the real game, inspect `events.jsonl`, and fix measured hot paths until
   240 FPS / 4.17 ms 99.99% and 60 Hz server are reached.

# Specification alignment

- `docs/specs/performance/performance.md`: 60 Hz fixed simulation, 240 FPS /
  4.17 ms client target, 1 Hz logging, measure before optimizing.
- `docs/specs/performance/perf-report-overlay.md` (new): modes, server Hz,
  overlay cost, 1 Hz journal contract.
- `docs/specs/dev-loop/dev-loop.md`: a successful build publishes but does not
  replace a live process; explicit launch is required.
- `docs/specs/dev-loop/agent-lanes-build-authority.md`: root dev-loop is the
  only build authority; agents edit and observe.

# Exact implementation changes

## New: `docs/specs/performance/perf-report-overlay.md`

Defines modes 0-3; mode 2 default; mode 3 full detail sampled at 1000 ms;
server RX/actual Hz; 1 Hz `performance.report`; overlay self-cost; client and
server targets.

## New: `src/perf/perf-report-state.h`

Game-only report mode state and `perf_sample_ms` interval API.

## `src/perf/perf.cpp`

- Added report mode state and `Perf::setReportMode`, `reportMode`,
  `setReportSampleMs`, `reportSampleMs`.
- Extended `PerfFrameAccounting` writes with server RX Hz, server tick, stale
  state, overlay cost, and published generation.
- Samples `latestServerTick` once per second; actual server Hz stays invalid
  unless a local measurement exists.
- Renamed the 1 Hz accounting record to `performance.report` and added mode,
  overlay, server, and generation fields.
- Published generation is read from the existing hot-status snapshot, not a new
  main-thread file read.

## `src/perf/perf-overlay.cpp`

- Added `Perf::Overlay` scope.
- Mode 1: existing live/full display.
- Mode 2: 1000 ms sampled compact display (summary + top 5 scopes + top 5
  timers) with cached lines.
- Mode 3: 1000 ms sampled full display with cached lines redrawn every frame.
- Added `OVERLAY`, `SERVER RX`, `SERVER ACTUAL`, `SERVER TICK`, and `GEN`.
- Only formats the sampled panel once per interval.

## `src/perf/perf-commands.cpp`

- `perf_report [0|1|2|3]` and `perf [0|1|2|3]`.
- Default enable is mode 2.
- Added `perf_sample_ms [100..10000]`.

## `src/perf/perf-frame.cpp`

- `performance.frame-budget` is now rate-limited to one record per second.

## `src/game/hot-status-tips.*`

- Cached published generation is exposed for the 1 Hz performance event.

## `src/physics/movement/actor-collision-mesh.cpp`

- The weapon collider is included only when its world transform is within 5 m of
  the actor. A stale/uninitialized weapon transform at the origin previously
  unioned the weapon mesh at world origin with the actor at z ~454, producing a
  ~452 m actor AABB and thousands of candidates. Measured after the fix:
  - before: query box about `(115.8, 138.1, 451.6) m`, `2242` candidates,
    `solve_ms` about `25.5 ms`;
  - after: query box about `(1.2, 1.8, 3.7) m`, `2` candidates,
    `solve_ms` about `0.32 ms`.

## `src/network/server-npcs.cpp`

- NPC sense/target/debounce navigation now uses the stagger path for every mode,
  not only Fun Counter Strike. Each NPC thinks every third tick; physics still
  runs every tick.
- Observed before: `updates=907/s`, `sense_runs=907`, `navigation_runs=907`,
  `sense_skipped=0`.
- Observed after: `updates=227/s`, `sense_runs=77`, `navigation_skipped=150`.
  This proves staggering activated, but the server remained far below 60 Hz in
  the pathological test state.

## `src/sim/simulate-tick.cpp` and `src/physics/physics-mini.cpp`

- Added owner scopes:
  `ActorPair::Build`, `ActorPair::Resolve`, `ActorPair::FinalPose`,
  `Simulation::FinalPose`, `Physics::Config`, `Physics::MovePre`,
  `Physics::CollisionSteps`, `Physics::MovePost`, `Physics::VisualFacing`,
  `Physics::Animation`.
- These exposed the remaining costs instead of leaving them in lifecycle self
  time.

## `devscripts/dev-loop.py`

- Removed the `auto_replace` path that relaunched a live client after every
  successful build.
- Only an explicit pending launch request relaunches.
- `--auto-restart` remains dead-client recovery only.
- Added `_should_relaunch_after_build` and test coverage.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/specs/performance/perf-report-overlay.md` | absent | new spec | mode/Hz contract | delete file |
| `docs/specs/performance/performance.md` | no overlay link | link + NPC stagger rule | alignment | `git checkout -- <file>` |
| `docs/specs/debug-logging/debug-logging.md` | no report event | `performance.report` row | event catalog | `git checkout -- <file>` |
| `docs/ROUTER.md` | no overlay spec | performance route | discoverability | `git checkout -- <file>` |
| `src/perf/perf-report-state.h` | absent | mode API | hot setting | delete file |
| `src/perf/perf.cpp` | no server/overlay fields | 1 Hz report + sampler | live evidence | `git checkout -- <file>` |
| `src/perf/perf-overlay.cpp` | single live display | modes 1/2/3 | bounded observer cost | `git checkout -- <file>` |
| `src/perf/perf-commands.cpp` | `0|1` | `0|1|2|3` + sample interval | controls | `git checkout -- <file>` |
| `src/perf/perf-frame.cpp` | per-frame budget event | 1/s rate limit | no spam | `git checkout -- <file>` |
| `src/game/hot-status-tips.*` | no generation getter | cached generation | event correlation | `git checkout -- <file>` |
| `src/physics/movement/actor-collision-mesh.cpp` | stale weapon mesh | distance guard | remove huge AABB | `git checkout -- <file>` |
| `src/network/server-npcs.cpp` | stagger only FCS | stagger all modes | spread NPC AI | `git checkout -- <file>` |
| `src/sim/simulate-tick.cpp` | coarse lifecycle | owner scopes | expose cost | `git checkout -- <file>` |
| `src/physics/physics-mini.cpp` | coarse physics | owner scopes | expose cost | `git checkout -- <file>` |
| `devscripts/dev-loop.py` | auto-replace live client | publish only | keep EXE open | `git checkout -- <file>` |
| `devscripts/quality/test_manual_build_guard.py` | 3 tests | 4 tests | keep-open policy | `git checkout -- <file>` |
| `docs/gold/2026-11-16-honest-frame-accounting-and-perf-overlay.md` | absent | new gold | method record | delete file |
| `docs/regressions/performance/unaccounted-frame-time-REG-2026-11-16.md` | absent | new regression | old wrong number | delete file |
| `docs/regressions/performance/README.md` | 12 entries | 13 entries | index | `git checkout -- <file>` |
| `docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md` | absent | this record | history | delete file |

# Contract / ABI changes

- Contract: none. No game/host ABI, packet, or state schema changed.
- Version bump: none.
- Compatibility: existing game DLL ABI.
- Reverse: revert source changes.

# State / schema changes

- Domain: none.
- Old schema: n/a.
- New schema: n/a.
- Migration: n/a.
- Reverse: n/a.

# Deleted paths

- None by this session.

# Diagnostics

- `performance.report`: 1 Hz while report mode is enabled; mode, host/game/
  swap/sleep/unaccounted, overlay ms, server RX/actual/tick/stale, generation,
  top scopes.
- `performance.frame-budget`: capped at 1/s.
- `npc.phase-window`, `npc.slow-update`, `npc.pair-cost`,
  `performance.server-loop-window`, `collision.solve.summary`,
  `actor.collision.profile` used for the measured loop.
- All new diagnostics are owner-level and bounded.

# Validation

- Focused skills:
  - `docs/skills/efficiency-checker-v1.md`: no new unbounded hot-loop
    allocation; sampled text is cached; mode 2 cost measured.
  - `docs/skills/logging-checker-v1.md`: 1 Hz report, 1/s budget log, reasons
    included, no per-frame journal writes.
- Tests:
  - `python -m py_compile devscripts/dev-loop.py devscripts/dev_loop/cli.py devscripts/quality/test_manual_build_guard.py` -> PASS.
  - `python -m unittest devscripts.quality.test_manual_build_guard -v` -> 4 tests PASS.
- Build:
  - Game generations 298, 300, 303, and 314 published during the loop; client
    activated 298, 300, 303, and 314 in the same PID.
  - No EXE rebuild was needed for the perf/gameplay fixes.
- Runtime:
  - `performance.report` observed with `mode=2`, `overlay_ms` about
    `0.05–0.12 ms`, and `unaccounted_ms` about `0.03–0.29 ms`.
  - Client FPS remained about 3–6 FPS in the pathological disconnected test
    state, not 240 FPS.
  - Server actual Hz remained about 3.9–4.6 Hz, not 60 Hz.

# Measured evidence

- Client after weapon-AABB fix: collision query box dropped from about
  `115.8 x 138.1 x 451.6 m` to about `1.2 x 1.8 x 3.7 m`; candidates from
  `2242` to `2`; solve from about `25.5 ms` to `0.32 ms`.
- Client remaining owners in a late window:
  `PlayerAnimation` up to ~169 ms, `ActorPairCollision` ~48–90 ms,
  `Simulation::Lifecycle` self ~46–93 ms, `Combat::PostWeaponContainment`
  ~42 ms, `Physics::CollisionSteps` ~29–32 ms, `Replay` ~88 ms, `Swap`
  ~23–26 ms.
- Server after stagger: `updates/s` fell from 907 to 227 and
  `navigation_skipped` became nonzero (150), proving the stagger path.
  `actual_hz` still about 4 Hz because one NPC navigator update reached
  ~238 ms and the server is in a pathological map/route state.
- Overlay cost in mode 2: about `0.05–0.12 ms` in normal windows; one
  `3.0 ms` sample occurred during a stall, so overlay cost is reported rather
  than hidden.

# Regression review

- Regression entry appended: yes.
- `docs/regressions/performance/unaccounted-frame-time-REG-2026-11-16.md`.
- Why: the old unaccounted number mixed a completed cycle with a partial frame
  and the legacy timer set.
- Not a gameplay regression; the client/server targets remain open.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified:
  - 240 FPS at <=4.17 ms in 99.99% of frames.
  - Server 60 Hz (59.9–60.1) in 99.99% of windows.
  - Mode 2 overlay p95 <=0.20 ms across a clean steady window.
  - The pathological client/server state in run `20261117_135530` is not a
    valid final performance baseline.

# Agent handoff context

- Owner: opencode
- Focus: performance
- Files to check: `src/perf/*`, `src/physics/movement/actor-collision-mesh.cpp`,
  `src/network/server-npcs.cpp`, `src/sim/simulate-tick.cpp`,
  `src/physics/physics-mini.cpp`, `devscripts/dev-loop.py`.
- What another agent should not redo: do not rebuild the report from
  `PerfTimes.total`; do not reintroduce build-triggered live-client relaunch.
- Next exact step: start a clean connected client/server run with the current
  generation, then optimize the remaining owners in this order:
  1. server NPC navigator worst-case (238 ms single actor);
  2. client `PlayerAnimation` / `Simulation::Lifecycle` remaining self time;
  3. `ActorPair::Build` / `ActorPair::FinalPose`;
  4. `Combat::PostWeaponContainment`;
  5. `Swap`.
  Measure with mode 2 and the 1 Hz `performance.report`/`performance.fps`
  records.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 7ff11a91 / working tree
- Build identity: game generations 298, 300, 303, 314; client PID 21068 unchanged
- Journal: `C:\mimita-v9\logs\2026-11-17\20261117_135530\client-01\events.jsonl`
- Server journal: `C:\mimita-v9\logs\2026-11-17\20261117_135530\server-01\events.jsonl`
- Session ID: session-20261117-154358

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: n/a
- Reverse only files touched by this session. Preserve concurrent edits.

# Related feature record

- Feature path: none.

## Captured git state

```text
M devscripts/dev-loop.py
M devscripts/quality/test_manual_build_guard.py
M docs/ROUTER.md
M docs/regressions/performance/README.md
M docs/specs/debug-logging/debug-logging.md
M docs/specs/dev-loop/dev-loop.md
M docs/specs/performance/performance.md
M src/game/hot-status-tips.cpp
M src/game/hot-status-tips.h
M src/network/server-npcs.cpp
M src/perf/perf-accounting.h
M src/perf/perf-commands.cpp
M src/perf/perf-frame.cpp
M src/perf/perf-overlay.cpp
M src/perf/perf.cpp
M src/physics/movement/actor-collision-mesh.cpp
M src/physics/physics-mini.cpp
M src/sim/simulate-tick.cpp
?? src/perf/perf-report-state.h
?? docs/specs/performance/perf-report-overlay.md
?? docs/gold/2026-11-16-honest-frame-accounting-and-perf-overlay.md
?? docs/regressions/performance/unaccounted-frame-time-REG-2026-11-16.md
```

## Captured diff stat

```text
See `git diff --stat` and the listed files above.
```

## Captured changed paths

```text
See Captured git state.
```

## Captured build identity

```json
{
  "client_pid": 21068,
  "client_generations_activated": [298, 300, 303, 314],
  "build_authority": "root dev-loop",
  "exe_rebuild_required": false,
  "runtime_target_met": false
}
```
