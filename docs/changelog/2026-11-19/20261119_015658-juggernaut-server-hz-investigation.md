// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Juggernaut server Hz investigation

# Task

- Task ID: session-20261119-juggernaut-server-hz
- Session ID: session-20261119-juggernaut-server-hz
- Owner: Codex
- Focus: Juggernaut 60 Hz with all NPCs moving
- Summary: Traced severe 67-NPC server Hz collapse to repeated NPC collision/physics and navigation outliers; added bounded stage diagnostics and selected Recast for the remaining Juggernaut profile. Runtime target not yet met; waiting on the root dev-loop candidate to compile and load before iterating.
- Status: in-progress
- Date, time, timezone: 2026-11-19T01:56:58+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

- Exact status output: see captured git state below; it includes unrelated concurrent work.
- Files not created or modified by this session: `AGENTS.md`, `config/analytics.json`, `devscripts/blender-ai`, `devscripts/dev-loop.py`, `devscripts/dev_loop/game_build.py`, `devscripts/dev_loop/gui.py`, `devscripts/game_build_worker.py`, `docs/regressions/dev-loop/README.md`, `docs/specs/camera/camera.md`, `docs/specs/creators/monster-authoring.md`, `docs/specs/debug-logging/canonical-jsonl.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/gamemodes/zombie-attack.md`, `docs/specs/movement/movement.md`, `src/engine/engine-tick-camera.cpp`, `src/input/input-poll.cpp`, `docs/changelog/2026-11-19/20261119_014413-full-creator-implementation-handoff.md`, `docs/changelog/2026-11-19/20261119_014847-arrow-camera-controls.md`, `docs/changelog/2026-11-19/20261119_015547-dll-build-latency-handoff-and-timing-diagnosis.md`, `docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md`, `docs/handoffs/dll-build-one-second-investigation.md`, `docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md`.

# Requested behavior

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/agent-lanes-build-authority.md`.
- Exact requirements: Keep gameplay on fixed 60 Hz, bound NPC/navigation work to the tick budget, diagnose through the live dev loop and its `events.jsonl`, and distinguish loaded runtime evidence from build status.
- Why the change follows the specification: The runtime journal identifies a 363 ms synchronous global graph call and 93 ms physics/collision update; the patch adds bounded attribution and configures the remaining Juggernaut profile to use the existing Recast backend.
- Conflicts or decisions: Recast/local planning and collision still show large outliers. No behavior or Hz acceptance is claimed before the candidate is loaded and measured.

# Exact implementation changes

## File: `config/behavior-profiles.json`

- Lines/functions/headings: `juggernaut_limited` profile.
- Old content or behavior: The profile omitted `navigation_backend`, resolving to the synchronous custom graph by default.
- New content or behavior: Selects `recast`, matching `juggernaut_swarm`; navigator retains its local planner fallback when Recast cannot route.
- Reason: The run measured a 363.214 ms global graph planning call.
- Why unrelated behavior is preserved: This only changes navigation backend selection for this named profile; combat and other roles are untouched.

## File: `src/npc/npc-nav-graph.cpp`, `src/npc/npc-nav-graph.h`, `src/npc/npc-navigator.cpp`

- Lines/functions/headings: `NpcNavGraph::findRoutes`, its caller in `NpcNavigator::update`.
- Old content or behavior: A slow custom global-graph query had no internal stage attribution, and its event tick field used the actor ID.
- New content or behavior: Adds a rate-limited event separating geometry scan, chunk build/link, and route search; tags it with the NPC simulation tick.
- Reason: Attribute the exact synchronous stage if custom backend calls remain.
- Why unrelated behavior is preserved: Route selection and graph algorithm are unchanged.

## File: `src/physics/movement/physics-collision-glb-main.cpp`

- Lines/functions/headings: `doGLBTriangleCollisions` and `logNpcCollisionSubstageSample`.
- Old content or behavior: NPC slow-update rows exposed aggregate collision time but not collision sub-stages.
- New content or behavior: Emits a bounded event for NPC collisions >=10 ms, with per-phase timings and candidate counts; measures emergency-search time separately.
- Reason: Runtime rows reached 93.16 ms in physics/collision.
- Why unrelated behavior is preserved: Diagnostics do not change movement or collision decisions.

## File: `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`

- Lines/functions/headings: Append-only occurrence for run `20261119_014024`.
- Old content or behavior: The previous Juggernaut occurrence described a different run.
- New content or behavior: Records the repeat run's 60-second Hz sample and measured NPC stalls.
- Reason: Preserve the confirmed recurrence and avoid overstating the candidate fix.
- Why unrelated behavior is preserved: Earlier occurrences are unchanged.

## File: `docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md`

- Lines/functions/headings: This session record.
- Old content or behavior: Generated template.
- New content or behavior: Captures task scope, work, evidence, and exact next step.
- Reason: Repository requires one final changelog per session.
- Why unrelated behavior is preserved: The changed-files table below lists only this session's files.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/behavior-profiles.json` | Default custom backend | Recast for `juggernaut_limited` | Avoid synchronous graph stalls seen in run | `git checkout -- config/behavior-profiles.json` |
| `src/npc/npc-nav-graph.cpp` | No graph stage event | Rate-limited timings by stage | Diagnose graph outliers | `git checkout -- src/npc/npc-nav-graph.cpp` |
| `src/npc/npc-nav-graph.h` | No simulation tick argument | Optional tick argument | Correct diagnostic event tick | `git checkout -- src/npc/npc-nav-graph.h` |
| `src/npc/npc-navigator.cpp` | Calls graph without tick | Passes NPC simulation tick | Correct diagnostic event tick | `git checkout -- src/npc/npc-navigator.cpp` |
| `src/physics/movement/physics-collision-glb-main.cpp` | Aggregate collision timing | Bounded sub-stage event | Identify collision outlier owner | `git checkout -- src/physics/movement/physics-collision-glb-main.cpp` |
| `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md` | Previous occurrences only | Appended 014024 evidence | Track confirmed repeat | Restore only this append manually |
| `docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md` | Template | Filled session record | Repository requirement | Remove this file |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category: NPC movement and collision; `StructuredLogger`.
- Input: Global graph query stages; NPC collision totals >=10 ms.
- Decision: Emit the diagnostic only above the threshold and rate limit.
- Output: `npc.nav.graph-query-cost` and `npc.collision.substage-sample`.
- Failure or rejection reason: Neither event is present in run `20261119_014024` because the running server still uses generation 345.
- Rate limiting: One graph query event per thread per second; one collision event per server process every 250 ms.

# Validation

- Focused skill paths and results: None.
- Tests and exact commands: `git diff --check -- config/behavior-profiles.json src/npc/npc-nav-graph.cpp src/npc/npc-nav-graph.h src/npc/npc-navigator.cpp src/physics/movement/physics-collision-glb-main.cpp` (passed with a line-ending warning for `npc-navigator.cpp`).
- Build status: Root dev loop reported game DLL generation 350 `building` for ~364 seconds at the last check. No build command was invoked by this session.
- Runtime or hot-reload evidence: Run `20261119_014024` is live on module generation 345; the new diagnostics and profile setting have not been runtime-verified.
- Output files: `logs/2026-11-19/20261119_014024/server-01/events.jsonl`.

# Measured evidence

- Before values: In the final captured 60 seconds, 32/32 loop windows were below 59.9 Hz; median 18.674 Hz, min 5.972 Hz, max 39.722 Hz. Worst stall: 1605.535 ms total, 1486.612 ms NPC. A sampled NPC had 93.16 ms physics/collision; another had 82.03 ms navigator. Earlier in this run custom global graph reached 363.214 ms.
- After values: None. The game loop still has generation 350 building and live server generation 345.
- Timestamps: Run launched 2026-11-19 01:40:24Z; latest evidence/status read around 01:56:50Z.
- Tick/frame/network measurements: Last-minute measurement used 32 one-second loop-window events; actual Hz target not met. Server reports 67 NPCs. NPC decisions and movement/animation rows show movement, but do not prove all-NPC continuous movement acceptance.

# Regression review

- Regression entry appended: yes.
- Why this is or is not a confirmed regression: Reproduced severe Hz overload in a second matching Juggernaut run and appended separate occurrence evidence.
- Related regression paths: `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`.

# Human acceptance

- Visual review: Not performed.
- Gameplay review: NPC movement events are present, but no human acceptance.
- Multiplayer review: One client connected; no multiplayer acceptance.
- Still unverified: Candidate load, collision sub-stage diagnosis, sustained 59.9–60.1 Hz, 99.99% target, and continuous movement for every NPC.

# Agent handoff context

- Owner: Codex
- Focus: Juggernaut 60 Hz with all NPCs moving
- Files to check: `config/behavior-profiles.json`, `src/npc/npc-nav-graph.cpp`, `src/npc/npc-navigator.cpp`, `src/physics/movement/physics-collision-glb-main.cpp`, root dev-loop status and run `20261119_014024` journal.
- What another agent should not redo: The active baseline run and its 67-NPC/Hz analysis; do not revert unrelated concurrent workspace edits.
- Next exact step: Check `python devscripts/dev-loop.py --status` later for generation 350 activation. Once a fresh module is loaded, launch a new authorized Juggernaut run and use its journal to measure Recast selection and collision stages; continue optimizing based on observed owner-level costs.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 214849f07f9a4bc045d1461b1dbaf463a2084a85 / 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T00:53:35.016823Z", "latest_successful_build_id": "2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18", "dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-345\\game-g345.dll", "generation": 345, "published_utc": "2026-11-19T00:53:18.768Z", "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 9.709, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-19T00:53:32.510Z"}}
- Generation: 345
- Artifact: build\game\generation-345\game-g345.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_014024\client-01\events.jsonl
- Session ID: session-20261119-juggernaut-server-hz

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M AGENTS.md
 M config/analytics.json
 M config/behavior-profiles.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M docs/regressions/dev-loop/README.md
 M docs/specs/camera/camera.md
 M docs/specs/creators/monster-authoring.md
 M docs/specs/debug-logging/canonical-jsonl.md
 M docs/specs/debug-logging/debug-logging.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/gamemodes/zombie-attack.md
 M docs/specs/movement/movement.md
 M src/engine/engine-tick-camera.cpp
 M src/input/input-poll.cpp
 M src/npc/npc-nav-graph.cpp
 M src/npc/npc-nav-graph.h
 M src/npc/npc-navigator.cpp
 M src/physics/movement/physics-collision-glb-main.cpp
?? docs/changelog/2026-11-19/20261119_014413-full-creator-implementation-handoff.md
?? docs/changelog/2026-11-19/20261119_014847-arrow-camera-controls.md
?? docs/changelog/2026-11-19/20261119_015547-dll-build-latency-handoff-and-timing-diagnosis.md
?? docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md
?? docs/handoffs/dll-build-one-second-investigation.md
?? docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md
```

## Captured diff stat

```text
AGENTS.md                                          | 10 +++
 config/analytics.json                              |  2 +-
 config/behavior-profiles.json                      |  1 +
 devscripts/blender-ai                              |  0
 devscripts/dev-loop.py                             |  9 +++
 devscripts/dev_loop/game_build.py                  | 15 ++++
 devscripts/dev_loop/gui.py                         | 39 +++++++----
 devscripts/game_build_worker.py                    | 76 +++++++++++++++++++-
 docs/regressions/dev-loop/README.md                |  9 ++-
 docs/specs/camera/camera.md                        | 27 ++++++++
 docs/specs/creators/monster-authoring.md           |  4 +-
 docs/specs/debug-logging/canonical-jsonl.md        | 12 ++++
 docs/specs/debug-logging/debug-logging.md          |  1 +
 docs/specs/dev-loop/build-time-observability.md    | 81 ++++++++++++++++++++--
 docs/specs/dev-loop/dev-loop.md                    | 15 +++-
 docs/specs/gamemodes/zombie-attack.md              | 30 ++++++++
 docs/specs/movement/movement.md                    |  2 +
 src/engine/engine-tick-camera.cpp                  | 61 ++++++++++++++++
 src/input/input-poll.cpp                           | 22 ++++--
 src/npc/npc-nav-graph.cpp                          | 65 ++++++++++++++++-
 src/npc/npc-nav-graph.h                            |  3 +-
 src/npc/npc-navigator.cpp                          |  4 +-
 .../movement/physics-collision-glb-main.cpp        | 60 +++++++++++++++-
 23 files changed, 515 insertions(+), 33 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/analytics.json
M	config/behavior-profiles.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	docs/regressions/dev-loop/README.md
M	docs/specs/camera/camera.md
M	docs/specs/creators/monster-authoring.md
M	docs/specs/debug-logging/canonical-jsonl.md
M	docs/specs/debug-logging/debug-logging.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/zombie-attack.md
M	docs/specs/movement/movement.md
M	src/engine/engine-tick-camera.cpp
M	src/input/input-poll.cpp
M	src/npc/npc-nav-graph.cpp
M	src/npc/npc-nav-graph.h
M	src/npc/npc-navigator.cpp
M	src/physics/movement/physics-collision-glb-main.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-19T00:53:35.016823Z",
    "latest_successful_build_id": "2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "a124a1ee639728eb55c7c097d248fdf90e0fa548ffd237ac6049c8afe36b8f18",
    "dll": "build\\game\\generation-345\\game-g345.dll",
    "generation": 345,
    "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-345\\game-g345.dll",
    "generation": 345,
    "published_utc": "2026-11-19T00:53:18.768Z",
    "revision": "d73402202c551195ee6f2822fbbb2f70ae091ba3b33bdef02c6c2e81bb897914",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 9.709,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-19T00:53:32.510Z"
  }
}
```
