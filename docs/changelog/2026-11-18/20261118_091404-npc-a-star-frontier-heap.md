// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Server tick and build visibility

# Task

- Task ID: session-20261118-091401
- Session ID: session-20261118-091401
- Owner: Codex
- Focus: Server tick performance, NPC navigation, and game DLL build visibility
- Summary: Inspected the Payload server journal, traced severe tick overruns to expensive NPC navigation, replaced its quadratic A* frontier with a deterministic heap, and added live game DLL phase/count visibility to the dashboard.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T09:14:03+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The shared checkout contained unrelated config, dev-loop, UI, map-editor,
  combat, effects, networking, physics, documentation, and regression edits.
  The full captured state is retained below. Those changes were preserved.
- The earlier session's `src/npc/npc-navigator.cpp` change remains separate and
  is recorded in `20261118_090034-npc-recast-planning-order.md`.
- This session changed `src/npc/npc-nav-graph.cpp`,
  `devscripts/game_build_worker.py`, `devscripts/dev_loop/gui.py`, and the
  dashboard specification, and updated this changelog. Existing user edits in
  the GUI and dashboard spec were preserved.

# Requested behavior

- Reinspect the Payload server journal and continue fixing the proven source
  of the far-below-60-Hz server rate. Also show game DLL build progress in the
  dashboard.

# Specification alignment

- Current specification paths:
- `docs/specs/performance/performance.md`,
  `docs/specs/npc-behavior/npc-behavior.md`, and
  `docs/architecture/player-npc-systems/npc-movement.md`.
- Exact requirements: the authoritative server simulation stays fixed at
  60 Hz; 99.99% of ticks should finish within 16.667 ms. Identify the first
  over-budget owner from live evidence before changing it, and keep physics,
  collision, and damage on the fixed tick.
- The active journal attributes multi-second ticks to NPC navigation. The
  custom graph's A* open list used a linear minimum search plus vector erase on
  each expansion, which scales quadratically with the frontier. A min-heap
  reduces selection/removal to logarithmic work without changing route policy.
- No spec conflict. The Recast outliers and final 60-Hz acceptance remain
  unresolved.

# Dashboard and worker changes

- The GUI now reads the root worker's `build/game/progress.json` and displays phase, elapsed seconds, compile completed/total and percentage, active source paths, source revision, matching changed paths, and published generation plus DLL SHA-256 when available.
- `[CXX]` means compiled and `[SKIP]` means cached object reuse. The UI says the natural-language prompt is not recorded, avoiding an invented explanation.
- The game worker emits a half-second elapsed-time heartbeat, tracks active translation units, and removes the separate probe export from the whole-game source total before progress initializes. This fixes the prior 642/643 denominator mismatch.
- Dashboard specification requires honest compile progress and named link/publication/runtime-activation phases.
# Exact implementation changes

## File: `src/npc/npc-nav-graph.cpp`

- Lines/functions/headings: `NpcNavGraph::findOneRoute`, A* open frontier
  (current lines 543–590).
- Old content or behavior: every expansion searched all open entries for the
  smallest `f`, then erased from the middle of the vector. With many nodes this
  made route search quadratic.
- New content or behavior: a min-priority queue selects/removes the next
  frontier entry in logarithmic time. An insertion-order tie breaker preserves
  deterministic ordering for equal `f`; stale improved entries are skipped.
- Reason: the live journal attributed 8.9–38.6 second delays to
  `global_graph_ms` for balanced NPCs.
- Why unrelated behavior is preserved: graph construction, route costs,
  movement, collision, combat, and NPC tick cadence are unchanged.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/camconfig.json` |  |  |  | git checkout -- config/camconfig.json |
| `config/gamemodes/funcounterstrike.json` |  |  |  | git checkout -- config/gamemodes/funcounterstrike.json |
| `config/healthbar.json` |  |  |  | git checkout -- config/healthbar.json |
| `config/maps/funworld3.json` |  |  |  | git checkout -- config/maps/funworld3.json |
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
| `src/effects/effect-part-render.cpp` |  |  |  | git checkout -- src/effects/effect-part-render.cpp |
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/mapeditor/map-editor.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor.cpp |
| `src/network/multiplayer-context.h` |  |  |  | git checkout -- src/network/multiplayer-context.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/npc/npc-nav-graph.cpp` |  |  |  | git checkout -- src/npc/npc-nav-graph.cpp |
| `src/npc/npc-navigator.cpp` |  |  |  | git checkout -- src/npc/npc-navigator.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `src/physics/movement/physics-collision-body.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-body.cpp |
| `assets/textureshq/rocket-shockwave-v1.png` |  |  |  | Remove-Item -LiteralPath 'assets/textureshq/rocket-shockwave-v1.png' |
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

- Owner/category: `NpcNavGraph::findOneRoute`, surfaced by
  `npc.nav.plan-cost` in `NPC_MOVEMENT`.
- Input: `global_graph_ms`, `local_plan_ms`, `recast_ms`, `total_ms`, and the
  server tick-window fields.
- Decision: replace only the open-set data structure; keep existing route
  selection and failure behavior.
- Output: existing diagnostics only; no new logging was added.
- Failure or rejection reason: none introduced; no-route results still follow
  the existing retry path.
- Rate limiting: unchanged.

# Validation

- Focused review: `docs/skills/efficiency-checker-v1.md` and
  `docs/skills/spec-behavior-review-v1.md` — confirmed the quadratic frontier
  operation and checked preservation of route policy.
- Tests and exact commands: no tests run. Focused `git diff --check` for the
  changed files passed; `python devscripts/changelog.py --check --session-id
  session-20261118-091401` passed.
- Build status: no explicit build request issued. The root dev-loop completed
  and published game generation 341 (`build/game/generation-341/game-g341.dll`)
  with code SHA-256 `d0641d81e6036d8ac2a20e5913ae2828eba9a496f889416e1339c0b2cac05995`.
  Its source-manifest revision was `59c4f880aba9aeded7dab5db748ba39f19c192d235792249a7f96b8a92770171`;
  the manifest SHA-256 for `src/npc/npc-nav-graph.cpp` matches the edited
  worktree file. Compile/link completed in 50.8s/7.8s (33 compiled, 609 skipped).
  Text status still reports generation 338/building, so its snapshot is stale
  relative to `build/game/current.json` and `ready.json`.
- Runtime or hot-reload evidence: baseline below. Generation 341 has not been
  activated in a server process; current dev-loop status reports no server or
  client running.
- Output file: `logs/2026-11-18/20261118_085322/server-01/events.jsonl`.

# Measured evidence

- Before values, run `20261118_085322`, server PID `19824`, FunWorld3/Payload:
  42 NPCs were reported in server tick events while launch status advertised
  19. The latest windows ranged from 0.15 to 0.44 actual Hz, with 2.3–32.5 s
  average tick time. The 09:13:28Z window had 5/5 ticks over 100 ms and a
  3.45 s maximum. NPC time accounted for about 2.27 s average of that window's
  2.29 s average tick.
- Navigator attribution: `global_graph_ms` reached 38.557 s at 09:08:40Z,
  15.652 s at 09:12:47Z, and 8.987 s at 09:13:00Z. A separate Recast plan
  reached 12.254 s at 09:07:42Z. These are source-stage wall-time
  observations; they are not a post-change comparison.
- After values: none; generation 341 has not been activated. No 60-Hz claim.
- Generation context: the same PID had activated whole-game generation 338
  at 09:07:29Z, before this heap edit. The server journal continued through
  09:14:03Z and recorded a 2.646-second tick, with 2.635 seconds in NPC work.
  That live generation still showed the old multi-second graph costs; the new
  generation 341 has not yet been activated. Current dev-loop status reports
  the server stopped, while ready.json identifies generation 341 as published.
- Tick/frame/network measurements: server only; client freshness had become
  stale during the slow windows. The NPC-count mismatch should be resolved
  before final acceptance.

# Regression review

- Regression entry appended: yes/no
- no. This is an optimization for an existing unresolved performance issue;
  it is compiled in generation 341 but not yet runtime-validated.
- Related regression paths: `docs/regressions/performance/README.md`,
  `docs/regressions/performance/fcs-journal-rotation-and-npc-tick-REG-2026-11-08.md`.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: heap generation activation, stable server rate after
  activation, route correctness, Recast multi-second outliers, other NPC tick
  costs, and the 19-vs-42 NPC mismatch.

# Agent handoff context

- Owner: Codex
- Focus: Server tick performance and NPC navigation
- Files to check: `src/npc/npc-nav-graph.cpp`,
  `npc.nav.plan-cost`, `npc.phase-window`, `performance.server-tick-window`,
  and `performance.server-loop-window` in the same run.
- What another agent should not redo: finding the vector-based A* frontier;
  the active logs and exact owner are recorded above.
- Next exact step: allow the root dev-loop to publish this saved game source,
  confirm activation in the same server PID, then compare the same 42-NPC
  workload's global-graph cost, tick durations, and actual Hz. If graph time
  falls but server ticks remain over 16.667 ms, measure the Recast/hash stage
  separately and address that owner next.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "2c70529f5fa72185c27cae6eac4c4268b204e50ecd38d19e40c8cb7876149360", "dll": "build\\game\\generation-338\\game-g338.dll", "generation": 338, "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-338\\game-g338.dll", "generation": 338, "published_utc": "2026-11-18T09:07:45.314Z", "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 338
- Artifact: build\game\generation-338\game-g338.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_085322\client-01\events.jsonl
- Session ID: session-20261118-091401

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
 M config/maps/funworld3.json
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
 M src/effects/effect-part-render.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/mapeditor/map-editor.cpp
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/npc/npc-nav-graph.cpp
 M src/npc/npc-navigator.cpp
 M src/npc/npc.cpp
 M src/physics/movement/physics-collision-body.cpp
?? assets/textureshq/rocket-shockwave-v1.png
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
config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   2 +-
 config/camconfig.json                              |   4 +-
 config/gamemodes/funcounterstrike.json             |   6 +-
 config/healthbar.json                              |   2 -
 config/maps/funworld3.json                         |  85 ++++++++++++-
 config/movement.json                               |   1 +
 config/movement/movement-source.json               |  10 +-
 config/video-settings.json                         |   2 +-
 config/weaponcollisions.json                       |   2 +-
 config/weapons/rocket_launcher/config.json         |   9 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |   1 +
 devscripts/dev-loop.py                             |  22 ++++
 devscripts/dev-profiles/npc-navigation.json        |   2 +-
 devscripts/dev_loop/gui.py                         |  44 ++++++-
 docs/regressions/collisions/README.md              |   4 +-
 .../physical-objects-collision-REG-2026-10-01.md   |  69 +++++++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |   9 ++
 docs/regressions/dev-loop/README.md                |  16 ++-
 docs/regressions/gameplay/README.md                |   9 +-
 docs/regressions/networking/README.md              |   2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    |  30 +++--
 docs/specs/dev-loop/dashboard.md                   |  16 ++-
 docs/specs/dev-loop/dev-loop.md                    |  40 ++++--
 docs/specs/gamemodes/funcounterstrike.md           |   3 +
 docs/specs/open-gameplay-issues.md                 |  14 ++-
 docs/specs/performance/performance.md              |  33 +++++
 src/combat/explosion-fx.cpp                        |  30 +++--
 src/combat/explosion-fx.h                          |   8 +-
 src/combat/weapon-rocket-launcher.cpp              | 134 +++++++++++++++++++--
 src/effects/effect-part-render.cpp                 |  56 +++++++--
 src/effects/effect-part.cpp                        |   7 +-
 src/effects/effect-part.h                          |   4 +-
 src/gui/hud/player-nameplates.cpp                  |   8 ++
 src/gui/hud/player-nameplates.h                    |   3 +-
 src/mapeditor/map-editor.cpp                       |   4 +
 src/network/multiplayer-context.h                  |   4 +
 src/network/multiplayer-projectiles.cpp            |  19 +++
 src/npc/npc-nav-graph.cpp                          |  32 +++--
 src/npc/npc-navigator.cpp                          |  21 +++-
 src/npc/npc.cpp                                    |   8 +-
 43 files changed, 671 insertions(+), 112 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/camconfig.json
M	config/gamemodes/funcounterstrike.json
M	config/healthbar.json
M	config/maps/funworld3.json
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
M	src/effects/effect-part-render.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/mapeditor/map-editor.cpp
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/npc/npc-nav-graph.cpp
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
    "code_hash": "2c70529f5fa72185c27cae6eac4c4268b204e50ecd38d19e40c8cb7876149360",
    "dll": "build\\game\\generation-338\\game-g338.dll",
    "generation": 338,
    "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-338\\game-g338.dll",
    "generation": 338,
    "published_utc": "2026-11-18T09:07:45.314Z",
    "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604",
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
