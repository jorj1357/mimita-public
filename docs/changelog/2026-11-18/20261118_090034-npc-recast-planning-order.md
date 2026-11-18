// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# NPC Recast planning order

# Task

- Task ID: session-20261118-090031
- Session ID: session-20261118-090031
- Owner: Codex
- Focus: Performance and NPC navigation
- Summary: Used the live Payload journal to identify duplicated local and Recast route planning; changed Recast mode to try its authoritative query before the custom fallback.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T09:00:33+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The shared checkout already had broad unrelated changes across configs,
  dev-loop sources/docs, regressions, NPC loadout code, physics, and UI. The
  exact initial `git status --short --branch` is represented by the captured
  repository state at the end of this file. Those edits were preserved.
- Files created/modified by this session: only
  `src/npc/npc-navigator.cpp` and this changelog.

# Requested behavior

- Diagnose the low authoritative Payload server tick rate from its live
  `events.jsonl`, prioritize server performance, and fix an obvious measured
  bottleneck. NPC aggression changes were secondary and were not attempted
  until the server budget is understood.

# Specification alignment

- Current specification paths:
- `docs/specs/performance/performance.md` and
  `docs/specs/performance/perf-report-overlay.md`.
- Exact requirements: fixed server simulation remains 60 Hz; at least 99.99%
  of ticks must be at or below 16.667 ms, with zero hard timeouts and zero
  `performance.server-tick-stall` events over 100 ms. Attribute the first
  over-budget stage before changing it.
- The live run identified NPC navigation as the dominant stage. In the
  configured Recast backend, the custom local planner was run before Recast,
  despite Recast being authoritative and the custom route serving only as its
  fallback. Reordering removes that work when Recast succeeds and retains it
  when Recast fails.
- No spec conflict. Reaching 60 Hz and increasing NPC aggression remain
  unproven/open.

# Exact implementation changes

## File: `src/npc/npc-navigator.cpp`

- Lines/functions/headings: `NpcNavigator::update`, planning block around
  `tryLocalFallback` and the Recast query (current lines 1234–1315).
- Old content or behavior: route planning ran `planLocalPath` before invoking
  Recast for every backend; Recast mode then replaced the local result on
  success, discarding the work.
- New content or behavior: Recast mode queries first, and calls the same local
  planner only if Recast did not produce a route. Custom and compare modes
  still produce their custom route first; compare remains observational.
- Reason: remove redundant navigation work from NPC fixed-tick updates while
  preserving route fallback and mode behavior.
- Why unrelated behavior is preserved: no route conditions, fallback code,
  combat decisions, collision, damage, or fixed-tick cadence changed.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/camconfig.json` |  |  |  | git checkout -- config/camconfig.json |
| `config/gamemodes/funcounterstrike.json` |  |  |  | git checkout -- config/gamemodes/funcounterstrike.json |
| `config/healthbar.json` |  |  |  | git checkout -- config/healthbar.json |
| `config/movement.json` |  |  |  | git checkout -- config/movement.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `config/weaponcollisions.json` |  |  |  | git checkout -- config/weaponcollisions.json |
| `config/weapons/rocket_launcher/config.json` |  |  |  | git checkout -- config/weapons/rocket_launcher/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev-profiles/npc-navigation.json` |  |  |  | git checkout -- devscripts/dev-profiles/npc-navigation.json |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/regressions/collisions/README.md` |  |  |  | git checkout -- docs/regressions/collisions/README.md |
| `docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md` |  |  |  | git checkout -- docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md |
| `docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md` |  |  |  | git checkout -- docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/gameplay/README.md` |  |  |  | git checkout -- docs/regressions/gameplay/README.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` |  |  |  | git checkout -- docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/gamemodes/funcounterstrike.md` |  |  |  | git checkout -- docs/specs/gamemodes/funcounterstrike.md |
| `docs/specs/open-gameplay-issues.md` |  |  |  | git checkout -- docs/specs/open-gameplay-issues.md |
| `docs/specs/performance/performance.md` |  |  |  | git checkout -- docs/specs/performance/performance.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/explosion-fx.h` |  |  |  | git checkout -- src/combat/explosion-fx.h |
| `src/combat/weapon-rocket-launcher.cpp` |  |  |  | git checkout -- src/combat/weapon-rocket-launcher.cpp |
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/network/multiplayer-context.h` |  |  |  | git checkout -- src/network/multiplayer-context.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/npc/npc-navigator.cpp` |  |  |  | git checkout -- src/npc/npc-navigator.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `src/physics/movement/physics-collision-body.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-body.cpp |
| `config/movement/movement-deliberate.json` |  |  |  | Remove-Item -LiteralPath 'config/movement/movement-deliberate.json' |
| `docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md' |
| `docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md' |
| `docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md' |
| `docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md' |
| `docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md' |
| `docs/changelog/2026-11-18/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-18/' |
| `docs/handoffs/collision-melee-fcs-trace-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/collision-melee-fcs-trace-handoff.md' |
| `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md' |
| `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md' |
| `docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md' |

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

- Owner/category: `NpcNavigator::update`, `NPC_MOVEMENT`.
- Input: live `npc.nav.plan-cost` fields `local_plan_ms`, `recast_ms`, and
  `total_ms`, correlated with `performance.server-tick-window` and
  `performance.server-loop-window`.
- Decision: this change alters planning order only for the explicit Recast
  backend.
- Output: existing plan-cost and server tick performance events; no new event
  or logging volume was added.
- Failure or rejection reason: when Recast cannot make a route,
  `planLocalPath` still runs as fallback.
- Rate limiting: existing owner diagnostics remain unchanged.

# Validation

- Focused skill paths and results: `docs/skills/efficiency-checker-v1.md` —
  confirmed redundant local planning in Recast mode;
  `docs/skills/spec-behavior-review-v1.md` — aligned with performance and
  navigation ownership; `docs/specs/hotreload/hotreload.md` and
  `docs/handoffs/hot-reload-agent-focus.md` reviewed; source manifest marks
  `src/npc/npc-navigator.cpp` as game-owned.
- Tests and exact commands: no tests or synthetic gameplay checks run.
  `git diff --check -- src/npc/npc-navigator.cpp` passed.
- Build status: no build command or request issued. The one observed dev-loop
  status showed `game_building`; no resulting generation/activation was
  confirmed by this session.
- Runtime or hot-reload evidence: baseline runtime evidence is below. No
  before/after generation activation proof or sustained post-change 60 Hz
  runtime acceptance is claimed.
- Output files: `logs/2026-11-18/20261118_085322/server-01/events.jsonl`.

# Measured evidence

- Before values: run `20261118_085322`, server PID `19824`, instance
  `server-01`, FunWorld3/Payload. The journal contains 184 server tick windows
  and 322 `performance.server-tick-stall` events over 100 ms. A representative
  window at 08:56:21Z reported `actual_hz=17.62`, 20 ticks, 54.15 ms average
  tick time, 182.48 ms max, 49.17 ms average NPC time, and 42 NPCs. A later
  window at 08:56:30Z reported `actual_hz=4.57`, 5 ticks, 205.59 ms average
  tick time, and 191.36 ms average NPC time. The worst recorded tick was
  3057.678 ms with 3039.844 ms in NPC work.
- Before planner attribution: `npc.nav.plan-cost` reached
  `local_plan_ms=36.4436` plus `recast_ms=45.3489` (81.7926 ms total) for one
  actor. Another sample spent 29.5287 ms local plus 46.4547 ms in Recast and
  still failed to plan. The active `elite_squad` profile selects
  `navigation_backend=recast`.
- After values: no verified after-build/activation sample. A later journal
  sample at 09:00:59Z still reported `actual_hz=7.73`; its generation remained
  335 and therefore does not establish a sustained fix. Another plan-cost
  event at 09:01:00Z showed 0.3902 ms local and 20.3462 ms Recast, but without
  matching activation evidence it is not attributed to this source edit.
- Timestamps: journal timestamps are UTC (`Z`); run launched
  `2026-11-18T08:53:22.220Z`.
- Tick/frame/network measurements: server only; no client performance
  acceptance measured. The run identity advertised `npcs=19` in dev-loop
  status while server tick events reported 42 NPCs; reconcile this mismatch
  for the next controlled sample.

# Regression review

- Regression entry appended: yes/no
- no. This is a measured performance defect in an existing unresolved NPC
  tick-overload area, not a newly confirmed regression or a verified fix.
- Related regression paths: `docs/regressions/performance/README.md`,
  `docs/regressions/performance/fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: candidate generation activation; sustained 60 Hz under
  the same 42-NPC load; route correctness when Recast fails; the 19-vs-42 NPC
  run identity mismatch; NPC aggression toward the player and between teams.

# Agent handoff context

- Owner: Codex
- Focus: Performance and NPC navigation
- Files to check: `src/npc/npc-navigator.cpp`, the same server journal, and
  its game-generation activation records.
- What another agent should not redo: the baseline journal extraction and the
  Recast-before-local-fallback reorder.
- Next exact step: let the existing root dev-loop finish its game-module
  publication, confirm same-PID activation, then compare plan-cost events,
  tick windows, actual Hz, and stalls under the same NPC workload. If Hz stays
  low, split Recast's full collision-content hash from Detour query timing;
  the hash currently walks every base collision triangle on each query.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "663409204f6bea889d3791b620bc58297c98ba3d756bd4848ec5198be08beb75", "dll": "build\\game\\generation-335\\game-g335.dll", "generation": 335, "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-335\\game-g335.dll", "generation": 335, "published_utc": "2026-11-18T07:29:35.088Z", "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 335
- Artifact: build\game\generation-335\game-g335.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_085322\client-01\events.jsonl
- Session ID: session-20261118-090031

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
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/camconfig.json
 M config/gamemodes/funcounterstrike.json
 M config/healthbar.json
 M config/movement.json
 M config/movement/movement-source.json
 M config/video-settings.json
 M config/weaponcollisions.json
 M config/weapons/rocket_launcher/config.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev-loop.py
 M devscripts/dev-profiles/npc-navigation.json
 M devscripts/dev_loop/gui.py
 M docs/regressions/collisions/README.md
 M docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
 M docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/open-gameplay-issues.md
 M docs/specs/performance/performance.md
 M src/combat/explosion-fx.cpp
 M src/combat/explosion-fx.h
 M src/combat/weapon-rocket-launcher.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/npc/npc-navigator.cpp
 M src/npc/npc.cpp
 M src/physics/movement/physics-collision-body.cpp
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md
?? docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md
?? docs/changelog/2026-11-18/
?? docs/handoffs/collision-melee-fcs-trace-handoff.md
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
?? docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  4 +-
 config/analytics.json                              |  4 +-
 config/audio/music-settings.json                   |  2 +-
 config/camconfig.json                              |  4 +-
 config/gamemodes/funcounterstrike.json             |  6 +-
 config/healthbar.json                              |  2 -
 config/movement.json                               |  1 +
 config/movement/movement-source.json               | 10 ++-
 config/video-settings.json                         |  2 +-
 config/weaponcollisions.json                       |  2 +-
 config/weapons/rocket_launcher/config.json         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 devscripts/dev-loop.py                             | 22 +++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 devscripts/dev_loop/gui.py                         | 44 ++++++++++++--
 docs/regressions/collisions/README.md              |  4 +-
 .../physical-objects-collision-REG-2026-10-01.md   | 69 +++++++++++++++++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |  9 +++
 docs/regressions/dev-loop/README.md                | 16 ++++-
 docs/regressions/gameplay/README.md                |  9 ++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 +++++----
 docs/specs/dev-loop/dashboard.md                   | 16 +++--
 docs/specs/dev-loop/dev-loop.md                    | 40 ++++++++----
 docs/specs/gamemodes/funcounterstrike.md           |  3 +
 docs/specs/open-gameplay-issues.md                 | 14 ++++-
 docs/specs/performance/performance.md              | 33 ++++++++++
 src/combat/explosion-fx.cpp                        | 21 ++++---
 src/combat/explosion-fx.h                          |  6 +-
 src/combat/weapon-rocket-launcher.cpp              | 71 ++++++++++++++++++++--
 src/effects/effect-part.cpp                        |  4 +-
 src/effects/effect-part.h                          |  2 +-
 src/gui/hud/player-nameplates.cpp                  |  8 +++
 src/gui/hud/player-nameplates.h                    |  3 +-
 src/network/multiplayer-context.h                  |  4 ++
 src/network/multiplayer-projectiles.cpp            | 19 ++++++
 src/npc/npc-navigator.cpp                          | 21 +++++--
 src/npc/npc.cpp                                    |  8 ++-
 39 files changed, 439 insertions(+), 81 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/camconfig.json
M	config/gamemodes/funcounterstrike.json
M	config/healthbar.json
M	config/movement.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	config/weaponcollisions.json
M	config/weapons/rocket_launcher/config.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev-loop.py
M	devscripts/dev-profiles/npc-navigation.json
M	devscripts/dev_loop/gui.py
M	docs/regressions/collisions/README.md
M	docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
M	docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/open-gameplay-issues.md
M	docs/specs/performance/performance.md
M	src/combat/explosion-fx.cpp
M	src/combat/explosion-fx.h
M	src/combat/weapon-rocket-launcher.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/npc/npc-navigator.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-18T05:35:43.892563Z",
    "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "663409204f6bea889d3791b620bc58297c98ba3d756bd4848ec5198be08beb75",
    "dll": "build\\game\\generation-335\\game-g335.dll",
    "generation": 335,
    "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-335\\game-g335.dll",
    "generation": 335,
    "published_utc": "2026-11-18T07:29:35.088Z",
    "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 54.844,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-18T05:35:41.192Z"
  }
}
```
