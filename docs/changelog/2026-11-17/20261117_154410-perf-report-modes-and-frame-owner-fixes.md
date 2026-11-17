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
| `assets/avatars/{abusivegirl3,abusivegirlheadless4,girllargev2,girllargev3,jason,jorj1357large}/avatar.json` | no animation profile | `general_set_1` layered profile | restore local keyframed animations | remove `animation_profile` block |
| `assets/avatars/remote_*/avatar.json` | no animation profile | `general_set_1` layered profile | remote human avatars | remove `animation_profile` block |
| `src/npc/npc-navigator.cpp` | unbounded local A* | 6 ms / 12k expansion budget + event | prevent navigator stalls | `git checkout -- <file>` |
| `docs/features/performance/fps-improvement-ledger.md` | absent | new ledger | track FPS improvements | delete file |
| `docs/regressions/performance/nonragdoll-animation-stall-REG-2026-11-17.md` | absent | new regression | ragdoll/non-ragdoll evidence | delete file |
| `docs/regressions/collisions/world-triangle-candidate-scan-REG-2026-11-17.md` | absent | new regression | triangle scan tracking | delete file |
| `docs/regressions/performance/README.md` | 13 entries | 14 entries | index | `git checkout -- <file>` |
| `docs/regressions/collisions/README.md` | 20 entries | 21 entries | index | `git checkout -- <file>` |
| `docs/specs/animations/animations.md` | no human default profile rule | default profile requirement | prevent old animation fallback | `git checkout -- <file>` |
| `docs/specs/dev-loop/dev-loop.md` | no build/command event contract | build completion + delivery events | event-driven AI loop | `git checkout -- <file>` |
| `docs/gold/2026-11-16-honest-frame-accounting-and-perf-overlay.md` | absent | new gold | method record | delete file |
| `docs/regressions/performance/unaccounted-frame-time-REG-2026-11-16.md` | absent | new regression | old wrong number | delete file |
| `docs/regressions/performance/README.md` | 12 entries | 13 entries | index | `git checkout -- <file>` |
| `docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md` | absent | this record | history | delete file |

## Continuation — animation profiles, navigator budget, tracking

### Human avatar animation profiles

Added `animation_profile` with `"set": "general_set_1"` and
`"poseComposition": "layered"` to human/local avatars that had none:

- `abusivegirl3`, `abusivegirlheadless4`, `girllargev2`, `girllargev3`,
  `jason`, `jorj1357large`
- generated `remote_*` human avatars in `assets/avatars/`

The local player previously had no profile and fell back to the old global
procedural animation. Monster avatars were left alone; their
`authored_only` profiles are separate.

Validation: every edited `avatar.json` parsed as valid JSON and resolved
`animation_profile.set = general_set_1`. Runtime animation binding still needs
a visible client check (`anim.set.bound`, `anim.melee.pose_probe`).

### NPC navigator planning budget

`src/npc/npc-navigator.cpp` now bounds local A* planning to 6 ms and 12,000
expansions. On exhaustion it returns no new route and emits
`npc.nav.budget-exhausted` at most once per second with actor, expansions,
nodes, candidates, and elapsed ms. This prevents a single navigator update from
consuming hundreds of milliseconds. Movement/physics still run on every 60 Hz
tick; only route planning is bounded.

### Tracking documents

- `docs/features/performance/fps-improvement-ledger.md`: one row per verified
  FPS improvement with before/after evidence.
- `docs/regressions/performance/nonragdoll-animation-stall-REG-2026-11-17.md`:
  ragdoll bypasses the costly non-ragdoll animation/final-pose path.
- `docs/regressions/collisions/world-triangle-candidate-scan-REG-2026-11-17.md`:
  recurring world-triangle candidate/fallback-scan risk.
- Performance/collision README entries updated.

### Build and command events

`devscripts/dev-loop.py` now emits:

- `dev-loop.build.completed` / `dev-loop.build.failed`
- `dev-loop.game-generation.ready` / `dev-loop.game-build.failed`
- `dev-loop.build.published_live_process_unchanged`

The dev-loop spec now defines command lifecycle events and a timeout event for
a queued command with no host-read. This makes build readiness and delivery
readable from the journal without polling.

## Continuation — clean connected baseline (2026-11-17 17:02Z run)

A clean server/client pair was launched through the dev loop on payload funworld3
with the profile's 19 NPCs.

- Client PID 8968, server PID 12272; build `2026-11-17T17:02:29.218920Z`.
- Command delivery worked in this clean run: `perf_report 2` was delivered and
  completed. The earlier failure was a stalled client host loop, not the queue
  format.
- Animation binding confirmed for `jason`: `anim.set.loaded` with
  `clip_count=45` and `set_valid=true`; `anim.melee.pose_probe` shows
  `authored_only=false`, `set_valid=true`.
- Client baseline: about 7–14 FPS, frame 115–216 ms.
- Server baseline: `actual_hz` about 10–13 Hz, `npc.phase-window` total about
  419–522 ms/s for 19 NPCs.
- New top client owners: `Simulation::FinalPose` ~26–47 ms,
  `ActorPair::FinalPose` ~26–51 ms, `Rendering::Actors::NetworkEntities`
  ~10–17 ms, `Physics::CollisionSteps` ~7–8 ms, `Audio::StartSound` ~15 ms,
  `Combat::PostWeaponContainment` ~3–12 ms.
- Navigator budget evidence: `npc.nav.budget-exhausted` fires at about
  6 ms per plan, but 16 plans/s still leave `navigator_ms` ~127–205 ms/s.
- Change made: `ActorPair::FinalPose` containment now runs only when the actor
  pair actually changed (`corrected`, `pushed`, or `supportKey`), avoiding a
  second full final-pose sample on unchanged ticks.
- The 240 FPS / 60 Hz targets are still not met.

### Generation 318 result after conditional final pose

- `ActorPair::FinalPose` was removed from unchanged ticks; it no longer appears
  in the top client scopes.
- Client improved from about 7–14 FPS to about 23 FPS in the clean run
  (`performance.fps`: 23.8, 22.9; p50 ~34–40 ms).
- Remaining client owner: `Simulation::FinalPose` about 3.5–38 ms, plus
  `Combat::PostWeaponContainment` ~3–12 ms and `Swap` ~2–3 ms.
- Server remains below 60 Hz: `actual_hz` about 6–7; `performance.server-loop-window`
  shows 24–27 active projectiles with `projectile_sim_ms` 328–411 per window,
  and `npc.phase-window` total 326–390 ms/s.

### Further pass — projectile catch-up, NPC cadence, final pose

- Server projectile simulation now caps catch-up to 2 steps per projectile and
  12 total projectile steps per tick; `server.projectile.tick-cost` includes
  `budget_skipped` and the caps.
- NPC global plan budget reduced from 8 to 4 plans/s; repath interval 1.5 s;
  blocked retry 2.0 s; NPC thinking cadence changed from every 3 ticks to every
  4 ticks. Movement and physics still run every tick.
- Client `Simulation::FinalPose` now runs every 2 ticks for a stable idle actor
  and every tick otherwise.
- A second client (client-02) on generation 320 measured ~96–106 FPS while
  unconnected/idle, p50 ~5.6–6.1 ms, `Swap` ~0.8 ms in normal frames. This is
  not connected gameplay evidence.
- The 42-NPC server still measured ~9 Hz with `npc.phase-window` total
  398–744 ms/s and `navigator_ms` 221–428 ms/s. The next owner is navigator
  stage tracing (global graph vs local A* vs Recast vs follow), not just the
  per-plan budget.

### Generation 321 — projectile cap and cadence result

- Server activated generation 321 in the same PID 12272.
- With zero active projectiles, `actual_hz` was ~5–7.7 and one 155-second outer
  loop stall occurred while a concurrent build/link was running. That is not a
  clean gameplay window.
- `npc.phase-window` total remained 419–576 ms/s for 42 NPCs:
  `physics_ms` 189–204, `navigator_ms` 39–161, `decision_ms` 112–260,
  `path_ms` 28–41.
- `server.projectile.tick-cost` emitted no records with zero projectiles, so
  the projectile caps need a projectile-active window for proof.
- Client-02 on generation 320 measured ~96–106 FPS idle, p50 ~5.6–6.1 ms.
  Connected 42-NPC gameplay remains unmeasured after generation 321 because the
  server was building/stalled.

### Best-guess latency reduction without a build

- `src/engine/engine-tick-setup.cpp`: hot-reload/config polling interval raised
  from 167 ms to 500 ms. This cuts repeated `Setup::ConfigPolling::*` and
  `Setup::HotReloadDLL` work by roughly 3x while keeping live edits responsive.
- `docs/specs/open-gameplay-issues.md`: added OGI-034, event-driven AI/agent
  work instead of timer- or wait-based loops.
- No build was run for this change; it is source-only until the next normal
  dev-loop build.

### Generation 325 — idle FPS and navigation stage evidence

- Client-02 idle reached ~211–231 FPS on generation 322, p50 ~3.77–4.06 ms,
  with 40–47% of frames still over 4.17 ms. This is idle, not connected
  gameplay.
- Server generation 325 active in PID 12272. `npc.phase-window` total dropped
  to ~364 ms/s; `navigator_ms` 94–120, `physics_ms` 122–134,
  `decision_ms` 146–158, `path_ms` 29–45.
- No `npc.nav.plan-cost` records were emitted, meaning no single plan exceeded
  5 ms after the tighter budget. The remaining navigator time is distributed
  across many updates, not one large plan.
- Server per-tick stalls still reached `npc_ms` 192–452 ms with 42 NPCs and
  `actual_hz` ~3–8 Hz. The next measured owner is per-NPC physics, not the
  local planner.
- Source-only change after generation 325: NPC `physicsMainUpdate` substeps
  reduced from 2 to 1; not yet built or measured.

### Server NPC physics sub-stages and presentation budget

- `physics-mini` now records `config_ms`, `move_pre_ms`, `collision_ms`,
  `move_post_ms`, `facing_ms`, `animation_ms`, and total physics time. The
  values are exposed through `physicsStageCost()` and reported by
  `npc.slow-update` and `npc.phase-window`.
- `npc.slow-update` and `npc.phase-window` now also report `aim_ms` (NPC
  hybrid aim-body solve), `pose_containment_ms`, and skipped counts.
- Server idle NPCs animate procedural pose and aim at 30 Hz; server NPCs with
  a target keep 60 Hz animation. Movement, collision, and firing still run at
  60 Hz. Local/client NPC simulation is unchanged.
- A 4 ms per-tick animation/aim budget and a 10 ms server-tick presentation
  cutoff skip only optional presentation work for remaining idle NPCs. The
  movement/collision path is never skipped, so NPC speed and collision cadence
  are preserved.
- This is source-only until the normal dev-loop build; no compiler or build
  was run by the agent.

### Validation attempted

- Dev loop was already running and was confirmed with `--ensure-running`;
  server PID 12272 remained alive. The game build was still in progress and no
  client was online.
- `devscripts/quality/test_manual_build_guard.py`: 4 passed.
- `devscripts/quality/test_path_containment.py`: 6 passed.
- `devscripts/dev_loop/test_artifacts.py`: 4 passed.
- Weapon JSON sync: passed. Gamemode JSON sync: passed.
- `lint.py --gate` failed against its accepted baseline with pre-existing
  aggregate increases: damage-authority 93 > 81, weapon_id_branches 43 > 41,
  dead-code 9 > 8, duplicate windows 569 > 551. No new finding was attributed
  to the files changed here; the baseline was not updated.
- The optional C++ tools (`clang-format`, `clang-tidy`, `cppcheck`, `jscpd`)
  were not installed.
- `agent-gate.py` was not used because it stops the dev loop before running.

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
  - All edited avatar JSON files parsed with `animation_profile.set = general_set_1`.
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
  - Generation 317 activated in the same client PID 21068. A later
    `perf_report 2` command was queued but did not reach `host-read`; the client
    host loop was stalled. This confirms the command-delivery gap documented in
    the dev-loop spec.

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
