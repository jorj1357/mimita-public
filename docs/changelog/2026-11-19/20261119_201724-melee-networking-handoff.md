// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Melee Networking Handoff

# Task

- Task ID: session-20261119-201722
- Session ID: session-20261119-201722
- Owner: Codex
- Focus: docs/handoffs/20261119_201249-melee-networking-reliability.md
- Summary: Created a self-contained handoff for the predicted melee, snapshot reconnect, and server tick investigations. No gameplay code was changed in this session.
- Status: NOT_TESTED
- Date, time, timezone: 2026-11-19T20:17:24+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

- The shared checkout was already heavily modified by ongoing work. The
  generated diff summary captured 86 tracked paths plus untracked work. None
  of those changes is attributed to this handoff session.
- This session created exactly two files: the new handoff below and this
  changelog. No source/config files were edited here. Preserve all other
  working-tree state.
- Branch/commit and UTC timestamp are in the task metadata above. Human display
  time: 2026-11-19 15:17 EST.

# Requested behavior

Create a self-contained continuation brief for the next agent working on
Mode 1 Chainsaw, Mace, and SwordSword prediction/contact parity, plus the
reproduced snapshot-starvation/reconnect failure and server tick overruns. The
user wants the next agent to pick up from the evidence and continue toward a
current-build, live `events.jsonl` proof.

# Specification alignment

- `docs/specs/networking/contact-authority.md` describes immediate client
  prediction, server-referee validation, shared authoritative damage, and the
  99.99% eligible-contact agreement target.
- `docs/specs/weapons/melee/melee-weapons.md` scopes this phase to Mode 1 for
  Chainsaw, Mace, and SwordSword; Mode 2 remains spec-only and SpyKnife/GodBall
  are deferred.
- `docs/specs/performance/performance.md` requires the fixed 60 Hz simulation
  and measures the 16.667 ms tick budget. A short run does not establish its
  99.99% acceptance target.
- The handoff follows `AGENTS.md`: it records the root dev-loop build authority,
  does not direct an agent to build or wait, and separates build evidence from
  runtime evidence.

# Exact implementation changes

## File: `docs/handoffs/20261119_201249-melee-networking-reliability.md`

- Before: no file existed.
- After: added a 245-line self-contained investigation handoff covering user
  scope, active build/run identity, the exact earlier run and journal paths,
  snapshot/reconnect and melee evidence, code/spec owners, and the next
  evidence-first steps.
- Reason: let the next agent resume without rerunning the same failed baseline
  or mistaking a stale executable, compile result, or reconnect accept for
  gameplay success.
- Reverse: remove only this new handoff file if explicitly requested.

## File: `docs/changelog/2026-11-19/20261119_201724-melee-networking-handoff.md`

- Before: no file existed.
- After: records this doc-only session, evidence boundary, pre-existing tree
  state, review references, and validation.
- Reverse: remove only this session changelog if explicitly requested.

# Contract / ABI changes

None. This session did not change code, packet schemas, or runtime contracts.

# State / schema changes

None.

# Deleted paths

None.

# Diagnostics

No diagnostics were added in this session. The handoff documents the prior
`peer_sends` instrumentation in `src/network/server-packets.cpp`; its note ID,
DLL attempt, and unproven live status are distinguished from this document
work.

# Validation

- Focused documentation review: `docs/skills/documentation-checker-v1.md`.
  Purpose/audience and a DOES NOT section are present; all 22 relative Markdown
  links resolved to existing files.
- Relevant routing and operating documents reviewed: `docs/ROUTER.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/workflows/multi-client-dev-testing.md`, networking/contact authority,
  melee, performance, logging, hot reload, regression, and task-completion
  references listed in the handoff.
- Current dev-loop status was read once. It reported the primary run not
  running, the published EXE stale, and a newer game-DLL attempt active at
  `manifest_snapshot`; no build was requested or awaited.
- Prior runtime JSONL was inspected and summarized in the handoff. No new
  gameplay run, source build, or human gameplay review occurred in this
  session. This handoff does not claim the feature is fixed.

# Measured evidence

The handoff links the old test's per-process journals and records its exact
reconnect failure, NPC tick overruns, partial Mace acceptance/rejection counts,
and `claim_only` client feedback. Those are baseline observations from the
pinned EXE/DLL identity recorded in the handoff, not results from this
session's docs or from a current build.

# Regression review

No new regression entry was added. The handoff records a reproduced failure
and points the next agent to `docs/regressions/networking/README.md`; a
regression record should be appended only when its exact owner/cause/fix is
confirmed.

# Human acceptance

No visual, feel, or multiplayer acceptance was performed. Actual immediate
melee feedback, continuous NPC presentation, snapshot recovery, 60 Hz tick
compliance, and 99.99% contact agreement remain unverified.

# Agent handoff context

- Receiving agent: continue from
  `docs/handoffs/20261119_201249-melee-networking-reliability.md`.
- Do not redo the old `20261119_190051` baseline as proof of a new edit; its
  client/server pair is stopped and its build is stale.
- Next exact step: inspect the current root build and source ownership once,
  continue the receive/send/contact trace while the root authority acts, then
  launch a fresh isolated run only after its artifact is current.
- Do not revert unrelated shared-tree edits or stop the primary process if it
  is restarted by another owner.

# Reverse

If the handoff itself should be removed, delete only the two files named under
Exact implementation changes. Do not use the generated repository-wide
`git checkout` suggestions against pre-existing shared-tree modifications.

## Captured git state

```text
M AGENTS.md
 M config/accounts/default.json
 M config/analytics.json
 M config/gui/main-menu.json
 M config/gui/map-editor.json
 M config/hot-reload/packet-ledger.json
 M config/mapeditor.json
 M config/video-settings.json
 M config/weapons/chainsaw/config.json
 M config/weapons/mace/config.json
 M config/weapons/swordsword/config.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M devscripts/source_manifest.py
 M docs/ROUTER.md
 M docs/architecture/live-development/incremental-build-and-header-fanout.md
 M docs/architecture/live-development/module-migration-plan.md
 M docs/operations/task-completion/task-completion.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/hotreload/README.md
 M docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
 M docs/specs/animations/animations.md
 M docs/specs/camera/camera.md
 M docs/specs/code-wrapper/code-wrapper.md
 M docs/specs/creators/monster-authoring.md
 M docs/specs/debug-logging/canonical-jsonl.md
 M docs/specs/debug-logging/debug-logging.md
 M docs/specs/dev-loop/agent-lanes-build-authority.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/dev-loop/multi-instance.md
 M docs/specs/entity-editor/entity-editor.md
 M docs/specs/gamemodes/zombie-attack.md
 M docs/specs/hotreload/hotreload.md
 M docs/specs/movement/movement.md
 M docs/specs/networking/contact-authority.md
 M docs/specs/networking/networking.md
 M docs/specs/weapons/melee/melee-weapons.md
 M docs/specs/weapons/weapons.md
 M docs/workflows/multi-client-dev-testing.md
 M docs/workflows/runtime-scenario-validation.md
 M src/combat/melee-client.cpp
 M src/engine/engine-tick-camera.cpp
 M src/engine/engine-tick-state.cpp
 M src/entities/player-animation.cpp
 M src/game/server-command-dispatch-module.cpp
 M src/game/server-runtime-module.cpp
 M src/gamemode/map-config.cpp
 M src/gamemode/map-config.h
 M src/gui/gui-main.cpp
 M src/gui/gui-main.h
 M src/gui/menus/main-menu.cpp
 M src/gui/menus/main-menu.h
 M src/input/input-poll.cpp
 M src/main-systems.cpp
 M src/map/map-authored-geometry.cpp
 M src/map/map-loader-collision.cpp
 M src/map/map_common.h
 M src/mapeditor/map-editor-commands.cpp
 M src/mapeditor/map-editor-config.cpp
 M src/mapeditor/map-editor-config.h
 M src/mapeditor/map-editor-projects.cpp
 M src/mapeditor/map-editor-projects.h
 M src/mapeditor/map-editor.cpp
 M src/mapeditor/map-editor.h
 M src/network/server-gamemode.cpp
 M src/network/server-gamemode.h
 M src/network/server-packet-handlers.cpp
 M src/network/server-packets.cpp
 M src/network/server-physical-contact-actors.cpp
 M src/npc/npc-nav-graph.cpp
 M src/npc/npc-nav-graph.h
 M src/npc/npc-navigator.cpp
 M src/physics/movement/physics-collision-body.cpp
 M src/physics/movement/physics-collision-glb-main.cpp
 M src/physics/movement/physics-collision-glb-sweep-slide.cpp
 M src/physics/movement/physics-collision-shared.h
 M src/physics/movement/physics-scratch.h
 M src/procedural/infinite-world-runtime.cpp
 M src/world/world-gltf-loader.cpp
 M src/world/world-gltf-loader.h
?? config/cache/avatars/4bdcbb812c31b5a7e61d328c3be9185ba48e5a315e7c1e281db2e87da10a0771.asset
?? config/gui/create-menu.json
?? config/gui/my-creations-menu.json
?? devscripts/agent-runtime.py
?? devscripts/dev_loop/parallel_test_runs.py
?? devscripts/dev_loop/runtimes.py
?? devscripts/prune-builds.py
?? devscripts/scenarios/creator_map_loop_v1.json
?? devscripts/scenarios/creator_my_creations_v1.json
?? devscripts/scenarios/map_editor_chunk_integrity_v1.json
?? docs/changelog/2026-11-19/20261119_014413-full-creator-implementation-handoff.md
?? docs/changelog/2026-11-19/20261119_014847-arrow-camera-controls.md
?? docs/changelog/2026-11-19/20261119_015547-dll-build-latency-handoff-and-timing-diagnosis.md
?? docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md
?? docs/changelog/2026-11-19/20261119_015722-dll-build-notes-and-result-journaling.md
?? docs/changelog/2026-11-19/20261119_022044-juggernaut-collision-hot-path-followup.md
?? docs/changelog/2026-11-19/20261119_025627-juggernaut-collision-scratch-reuse.md
?? docs/changelog/2026-11-19/20261119_040731-dll-build-fast-path-and-attribution.md
?? docs/changelog/2026-11-19/20261119_101204-death-animation-idle-reset.md
?? docs/changelog/2026-11-19/20261119_101520-mode-spawns-after-dll-reload.md
?? docs/changelog/2026-11-19/20261119_102415-mode-spawn-regression-record.md
?? docs/changelog/2026-11-19/20261119_144558-juggernaut-performance-handoff.md
?? docs/changelog/2026-11-19/20261119_152943-universal-weapon-prediction-contract.md
?? docs/changelog/2026-11-19/20261119_154403-browser-join-freeze-investigation.md
?? docs/changelog/2026-11-19/20261119_160000-universal-live-creation-ideal.md
?? docs/changelog/2026-11-19/20261119_161553-creator-slice-1-create-map-editor-loop.md
?? docs/changelog/2026-11-19/20261119_162600-command-driven-gui-testing-and-browser-join-investigation.md
?? docs/changelog/2026-11-19/20261119_170056-mode-1-melee-client-prediction.md
?? docs/changelog/2026-11-19/20261119_171711-root-dev-loop-live-test-workflow.md
?? docs/changelog/2026-11-19/20261119_172001-juggernaut-nav-graph-tick-budget.md
?? docs/changelog/2026-11-19/20261119_175347-isolated-agent-test-runs.md
?? docs/changelog/2026-11-19/20261119_183301-dev-loop-snapshot-builds-and-supervision.md
?? docs/changelog/2026-11-19/20261119_192054-snapshot-peer-send-diagnostics.md
?? docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md
?? docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md
?? docs/handoffs/20261119_201249-melee-networking-reliability.md
?? docs/handoffs/dll-build-one-second-investigation.md
?? docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md
?? docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md
?? docs/specs/hotreload/hot-layer-latency-and-language.md
```

## Captured diff stat

```text
AGENTS.md                                          |   15 +-
 config/accounts/default.json                       |    2 +-
 config/analytics.json                              |    4 +-
 config/gui/main-menu.json                          |   16 +
 config/gui/map-editor.json                         |   30 +
 config/hot-reload/packet-ledger.json               |   42 +-
 config/mapeditor.json                              |    6 +
 config/video-settings.json                         |    2 +-
 config/weapons/chainsaw/config.json                |    2 +-
 config/weapons/mace/config.json                    |   10 +-
 config/weapons/swordsword/config.json              |   14 +-
 devscripts/blender-ai                              |    0
 devscripts/dev-loop.py                             |  266 ++++-
 devscripts/dev_loop/cli.py                         |   23 +-
 devscripts/dev_loop/game_build.py                  |  460 +++++++-
 devscripts/dev_loop/gui.py                         |  356 ++++--
 devscripts/game_build_worker.py                    | 1228 ++++++++++++++++----
 devscripts/source_manifest.py                      |   86 +-
 docs/ROUTER.md                                     |    6 +-
 .../incremental-build-and-header-fanout.md         |   42 +
 .../live-development/module-migration-plan.md      |    7 +
 docs/operations/task-completion/task-completion.md |   25 +-
 docs/regressions/dev-loop/README.md                |   13 +-
 docs/regressions/hotreload/README.md               |   10 +-
 .../server-tick-budget-REG-2026-11-03.md           |   23 +
 docs/specs/animations/animations.md                |   14 +
 docs/specs/camera/camera.md                        |   27 +
 docs/specs/code-wrapper/code-wrapper.md            |   48 +-
 docs/specs/creators/monster-authoring.md           |    4 +-
 docs/specs/debug-logging/canonical-jsonl.md        |   12 +
 docs/specs/debug-logging/debug-logging.md          |   34 +
 docs/specs/dev-loop/agent-lanes-build-authority.md |   17 +-
 docs/specs/dev-loop/build-time-observability.md    |  205 +++-
 docs/specs/dev-loop/dashboard.md                   |   14 +
 docs/specs/dev-loop/dev-loop.md                    |   61 +-
 docs/specs/dev-loop/multi-instance.md              |   40 +-
 docs/specs/entity-editor/entity-editor.md          |  102 ++
 docs/specs/gamemodes/zombie-attack.md              |   30 +
 docs/specs/hotreload/hotreload.md                  |   29 +
 docs/specs/movement/movement.md                    |    2 +
 docs/specs/networking/contact-authority.md         |   78 +-
 docs/specs/networking/networking.md                |   91 +-
 docs/specs/weapons/melee/melee-weapons.md          |   75 +-
 docs/specs/weapons/weapons.md                      |   38 +-
 docs/workflows/multi-client-dev-testing.md         |   38 +-
 docs/workflows/runtime-scenario-validation.md      |   76 +-
 src/combat/melee-client.cpp                        |   33 +-
 src/engine/engine-tick-camera.cpp                  |   61 +
 src/engine/engine-tick-state.cpp                   |  115 ++
 src/entities/player-animation.cpp                  |   63 +-
 src/game/server-command-dispatch-module.cpp        |   39 +-
 src/game/server-runtime-module.cpp                 |   88 ++
 src/gamemode/map-config.cpp                        |   76 +-
 src/gamemode/map-config.h                          |   19 +
 src/gui/gui-main.cpp                               |  296 +++++
 src/gui/gui-main.h                                 |   23 +
 src/gui/menus/main-menu.cpp                        |    4 +-
 src/gui/menus/main-menu.h                          |    2 +
 src/input/input-poll.cpp                           |   22 +-
 src/main-systems.cpp                               |  241 ++++
 src/map/map-authored-geometry.cpp                  |   87 ++
 src/map/map-loader-collision.cpp                   |   15 +
 src/map/map_common.h                               |    2 +
 src/mapeditor/map-editor-commands.cpp              |  236 ++++
 src/mapeditor/map-editor-config.cpp                |   17 +-
 src/mapeditor/map-editor-config.h                  |    7 +
 src/mapeditor/map-editor-projects.cpp              |  253 ++++
 src/mapeditor/map-editor-projects.h                |   52 +
 src/mapeditor/map-editor.cpp                       | 1166 +++++++++++++++++--
 src/mapeditor/map-editor.h                         |   40 +
 src/network/server-gamemode.cpp                    |   42 +
 src/network/server-gamemode.h                      |    6 +
 src/network/server-packet-handlers.cpp             |    4 +-
 src/network/server-packets.cpp                     |   60 +-
 src/network/server-physical-contact-actors.cpp     |    6 +-
 src/npc/npc-nav-graph.cpp                          |  114 +-
 src/npc/npc-nav-graph.h                            |    5 +-
 src/npc/npc-navigator.cpp                          |    4 +-
 src/physics/movement/physics-collision-body.cpp    |    8 +-
 .../movement/physics-collision-glb-main.cpp        |   81 +-
 .../movement/physics-collision-glb-sweep-slide.cpp |   29 +-
 src/physics/movement/physics-collision-shared.h    |    1 +
 src/physics/movement/physics-scratch.h             |    5 +
 src/procedural/infinite-world-runtime.cpp          |   15 +-
 src/world/world-gltf-loader.cpp                    |    7 +-
 src/world/world-gltf-loader.h                      |    4 +-
 86 files changed, 6329 insertions(+), 742 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/gui/main-menu.json
M	config/gui/map-editor.json
M	config/hot-reload/packet-ledger.json
M	config/mapeditor.json
M	config/video-settings.json
M	config/weapons/chainsaw/config.json
M	config/weapons/mace/config.json
M	config/weapons/swordsword/config.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	devscripts/source_manifest.py
M	docs/ROUTER.md
M	docs/architecture/live-development/incremental-build-and-header-fanout.md
M	docs/architecture/live-development/module-migration-plan.md
M	docs/operations/task-completion/task-completion.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/hotreload/README.md
M	docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
M	docs/specs/animations/animations.md
M	docs/specs/camera/camera.md
M	docs/specs/code-wrapper/code-wrapper.md
M	docs/specs/creators/monster-authoring.md
M	docs/specs/debug-logging/canonical-jsonl.md
M	docs/specs/debug-logging/debug-logging.md
M	docs/specs/dev-loop/agent-lanes-build-authority.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/dev-loop/multi-instance.md
M	docs/specs/entity-editor/entity-editor.md
M	docs/specs/gamemodes/zombie-attack.md
M	docs/specs/hotreload/hotreload.md
M	docs/specs/movement/movement.md
M	docs/specs/networking/contact-authority.md
M	docs/specs/networking/networking.md
M	docs/specs/weapons/melee/melee-weapons.md
M	docs/specs/weapons/weapons.md
M	docs/workflows/multi-client-dev-testing.md
M	docs/workflows/runtime-scenario-validation.md
M	src/combat/melee-client.cpp
M	src/engine/engine-tick-camera.cpp
M	src/engine/engine-tick-state.cpp
M	src/entities/player-animation.cpp
M	src/game/server-command-dispatch-module.cpp
M	src/game/server-runtime-module.cpp
M	src/gamemode/map-config.cpp
M	src/gamemode/map-config.h
M	src/gui/gui-main.cpp
M	src/gui/gui-main.h
M	src/gui/menus/main-menu.cpp
M	src/gui/menus/main-menu.h
M	src/input/input-poll.cpp
M	src/main-systems.cpp
M	src/map/map-authored-geometry.cpp
M	src/map/map-loader-collision.cpp
M	src/map/map_common.h
M	src/mapeditor/map-editor-commands.cpp
M	src/mapeditor/map-editor-config.cpp
M	src/mapeditor/map-editor-config.h
M	src/mapeditor/map-editor-projects.cpp
M	src/mapeditor/map-editor-projects.h
M	src/mapeditor/map-editor.cpp
M	src/mapeditor/map-editor.h
M	src/network/server-gamemode.cpp
M	src/network/server-gamemode.h
M	src/network/server-packet-handlers.cpp
M	src/network/server-packets.cpp
M	src/network/server-physical-contact-actors.cpp
M	src/npc/npc-nav-graph.cpp
M	src/npc/npc-nav-graph.h
M	src/npc/npc-navigator.cpp
M	src/physics/movement/physics-collision-body.cpp
M	src/physics/movement/physics-collision-glb-main.cpp
M	src/physics/movement/physics-collision-glb-sweep-slide.cpp
M	src/physics/movement/physics-collision-shared.h
M	src/physics/movement/physics-scratch.h
M	src/procedural/infinite-world-runtime.cpp
M	src/world/world-gltf-loader.cpp
M	src/world/world-gltf-loader.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-19T17:45:16.114810Z",
    "latest_successful_build_id": "2026-11-19T17:45:16.114810Z:54879b91e6623e913d4781fe0824b9ad95aaef00449c20d413db48f24771c74a",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "b252d2131aeea66873b8f6da858f3337502f7a6370e21bbdd08a6968a27343a0",
    "dll": "build\\game\\generation-375\\game-g375.dll",
    "generation": 375,
    "revision": "3f23dc0dd2678ef509eebdabc968e17cef763673854809251ff613fcb2cabc5f",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-375\\game-g375.dll",
    "generation": 375,
    "published_utc": "2026-11-19T19:54:30.372Z",
    "revision": "3f23dc0dd2678ef509eebdabc968e17cef763673854809251ff613fcb2cabc5f",
    "status": "ready"
  },
  "progress": {
    "compiled": 2,
    "elapsed_s": 66.487,
    "phase": "build_complete",
    "result": "success",
    "skipped": 85,
    "updated_utc": "2026-11-19T17:44:11.768Z"
  }
}
```
