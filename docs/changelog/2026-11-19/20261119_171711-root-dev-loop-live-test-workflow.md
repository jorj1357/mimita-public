// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Root dev-loop live test workflow

# Task

- Task ID: session-20261119-171710
- Session ID: session-20261119-171710
- Owner: Codex
- Focus: agent live runtime validation and exact executable journals
- Summary: Align runtime testing instructions with the shared root dev-loop, require current artifact identity, and use the launched run's per-instance events.jsonl as gameplay evidence.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T17:17:11+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

The checkout already contained numerous unrelated dirty changes before this
session. They are retained in the captured git status below and were not
modified or reverted here. This session changed only the three workflow files
listed in Exact implementation changes and this changelog.

# Requested behavior

Specify that agents use the newly published executable from the root dev-loop,
launch the requested live mode, and prove behavior from that run's exact
per-process `events.jsonl` journals. Never accept evidence from a stale build.

# Specification alignment

 - Current specification paths: `docs/specs/dev-loop/agent-lanes-build-authority.md`,
   `docs/specs/dev-loop/dev-loop.md`, `docs/workflows/runtime-scenario-validation.md`.
 - Exact requirements: saved compiler inputs are the normal build trigger;
   agents do not request or wait for builds; runtime evidence identifies the
   exact executable and its own journal.
 - Why the change follows the specification: replaced stale mode-2 lane
   directions with the shared-checkout/root-dev-loop workflow and a freshness
   gate before any gameplay evidence is counted.
 - Conflicts or decisions: no behavior decision changed. Module-only work still
   requires its own activation-generation evidence.

# Exact implementation changes

## File: `docs/workflows/runtime-scenario-validation.md`

- Lines/functions/headings: Required loop steps 4-8 and Evidence report.
- Old content or behavior: `Work in your own mode 2 lane`; `Launch the lane's own client/server pair`.
- New content or behavior: use the root dev-loop; verify artifact/source
  generation freshness; launch TDM mode 41 or another requested mode; record
  artifact hash, process identity and exact journals; compare client/server
  outcomes. Stale/failed artifact means `NOT_TESTED`.
- Reason: ensure live tests exercise the source edit and provide direct runtime
  evidence rather than an older executable's result.
- Why unrelated behavior is preserved: scenario, collision replay, and human
  acceptance requirements remain in place.

## File: `docs/workflows/multi-client-dev-testing.md`

- Lines/functions/headings: Workflow introduction and Start one server and two clients.
- Old content or behavior: agents run in mode 2 lanes with lane-local build/run.
- New content or behavior: agents use the shared root dev-loop and verify the
  current artifact plus matching server/client build identities.
- Reason: align multi-client instructions with the active checkout policy.
- Why unrelated behavior is preserved: existing launch commands and per-instance
  journal model are retained.

## File: `docs/operations/task-completion/task-completion.md`

- Lines/functions/headings: Completion flow steps 8-9.
- Old content or behavior: use a repository-local lane dev-loop and report its
  lane ID/artifact evidence.
- New content or behavior: use root dev-loop auto-publication; require a current
  artifact identity and exact client/server journal paths, otherwise mark
  runtime validation `NOT_TESTED`.
- Reason: make the general agent completion checklist enforce the live evidence
  gate for every code edit.
- Why unrelated behavior is preserved: build and runtime evidence remain
  separate from human acceptance.

# Captured dirty-tree paths and reverse steps

The generator captured all pre-existing dirty paths so they are visible to the
next session. Only the three workflow files above were edited by this session;
do not use the generated whole-tree list below to attribute work to this edit.

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gui/main-menu.json` |  |  |  | git checkout -- config/gui/main-menu.json |
| `config/gui/map-editor.json` |  |  |  | git checkout -- config/gui/map-editor.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/weapons/chainsaw/config.json` |  |  |  | git checkout -- config/weapons/chainsaw/config.json |
| `config/weapons/mace/config.json` |  |  |  | git checkout -- config/weapons/mace/config.json |
| `config/weapons/swordsword/config.json` |  |  |  | git checkout -- config/weapons/swordsword/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/source_manifest.py` |  |  |  | git checkout -- devscripts/source_manifest.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/architecture/live-development/incremental-build-and-header-fanout.md` |  |  |  | git checkout -- docs/architecture/live-development/incremental-build-and-header-fanout.md |
| `docs/architecture/live-development/module-migration-plan.md` |  |  |  | git checkout -- docs/architecture/live-development/module-migration-plan.md |
| `docs/operations/task-completion/task-completion.md` |  |  |  | git checkout -- docs/operations/task-completion/task-completion.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/hotreload/README.md` |  |  |  | git checkout -- docs/regressions/hotreload/README.md |
| `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md` |  |  |  | git checkout -- docs/regressions/performance/server-tick-budget-REG-2026-11-03.md |
| `docs/specs/animations/animations.md` |  |  |  | git checkout -- docs/specs/animations/animations.md |
| `docs/specs/camera/camera.md` |  |  |  | git checkout -- docs/specs/camera/camera.md |
| `docs/specs/code-wrapper/code-wrapper.md` |  |  |  | git checkout -- docs/specs/code-wrapper/code-wrapper.md |
| `docs/specs/creators/monster-authoring.md` |  |  |  | git checkout -- docs/specs/creators/monster-authoring.md |
| `docs/specs/debug-logging/canonical-jsonl.md` |  |  |  | git checkout -- docs/specs/debug-logging/canonical-jsonl.md |
| `docs/specs/debug-logging/debug-logging.md` |  |  |  | git checkout -- docs/specs/debug-logging/debug-logging.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/gamemodes/zombie-attack.md` |  |  |  | git checkout -- docs/specs/gamemodes/zombie-attack.md |
| `docs/specs/hotreload/hotreload.md` |  |  |  | git checkout -- docs/specs/hotreload/hotreload.md |
| `docs/specs/movement/movement.md` |  |  |  | git checkout -- docs/specs/movement/movement.md |
| `docs/specs/networking/contact-authority.md` |  |  |  | git checkout -- docs/specs/networking/contact-authority.md |
| `docs/specs/networking/networking.md` |  |  |  | git checkout -- docs/specs/networking/networking.md |
| `docs/specs/weapons/melee/melee-weapons.md` |  |  |  | git checkout -- docs/specs/weapons/melee/melee-weapons.md |
| `docs/specs/weapons/weapons.md` |  |  |  | git checkout -- docs/specs/weapons/weapons.md |
| `docs/workflows/multi-client-dev-testing.md` |  |  |  | git checkout -- docs/workflows/multi-client-dev-testing.md |
| `docs/workflows/runtime-scenario-validation.md` |  |  |  | git checkout -- docs/workflows/runtime-scenario-validation.md |
| `src/combat/melee-client.cpp` |  |  |  | git checkout -- src/combat/melee-client.cpp |
| `src/engine/engine-tick-camera.cpp` |  |  |  | git checkout -- src/engine/engine-tick-camera.cpp |
| `src/engine/engine-tick-state.cpp` |  |  |  | git checkout -- src/engine/engine-tick-state.cpp |
| `src/entities/player-animation.cpp` |  |  |  | git checkout -- src/entities/player-animation.cpp |
| `src/game/server-command-dispatch-module.cpp` |  |  |  | git checkout -- src/game/server-command-dispatch-module.cpp |
| `src/game/server-runtime-module.cpp` |  |  |  | git checkout -- src/game/server-runtime-module.cpp |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `src/gui/gui-main.h` |  |  |  | git checkout -- src/gui/gui-main.h |
| `src/gui/menus/main-menu.cpp` |  |  |  | git checkout -- src/gui/menus/main-menu.cpp |
| `src/gui/menus/main-menu.h` |  |  |  | git checkout -- src/gui/menus/main-menu.h |
| `src/input/input-poll.cpp` |  |  |  | git checkout -- src/input/input-poll.cpp |
| `src/main-systems.cpp` |  |  |  | git checkout -- src/main-systems.cpp |
| `src/mapeditor/map-editor-commands.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-commands.cpp |
| `src/mapeditor/map-editor-projects.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-projects.cpp |
| `src/mapeditor/map-editor-projects.h` |  |  |  | git checkout -- src/mapeditor/map-editor-projects.h |
| `src/mapeditor/map-editor.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor.cpp |
| `src/mapeditor/map-editor.h` |  |  |  | git checkout -- src/mapeditor/map-editor.h |
| `src/network/server-gamemode.cpp` |  |  |  | git checkout -- src/network/server-gamemode.cpp |
| `src/network/server-gamemode.h` |  |  |  | git checkout -- src/network/server-gamemode.h |
| `src/network/server-packet-handlers.cpp` |  |  |  | git checkout -- src/network/server-packet-handlers.cpp |
| `src/network/server-physical-contact-actors.cpp` |  |  |  | git checkout -- src/network/server-physical-contact-actors.cpp |
| `src/npc/npc-nav-graph.cpp` |  |  |  | git checkout -- src/npc/npc-nav-graph.cpp |
| `src/npc/npc-nav-graph.h` |  |  |  | git checkout -- src/npc/npc-nav-graph.h |
| `src/npc/npc-navigator.cpp` |  |  |  | git checkout -- src/npc/npc-navigator.cpp |
| `src/physics/movement/physics-collision-body.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-body.cpp |
| `src/physics/movement/physics-collision-glb-main.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-glb-main.cpp |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-glb-sweep-slide.cpp |
| `src/physics/movement/physics-collision-shared.h` |  |  |  | git checkout -- src/physics/movement/physics-collision-shared.h |
| `src/physics/movement/physics-scratch.h` |  |  |  | git checkout -- src/physics/movement/physics-scratch.h |
| `src/world/world-gltf-loader.cpp` |  |  |  | git checkout -- src/world/world-gltf-loader.cpp |
| `src/world/world-gltf-loader.h` |  |  |  | git checkout -- src/world/world-gltf-loader.h |
| `config/cache/avatars/4bdcbb812c31b5a7e61d328c3be9185ba48e5a315e7c1e281db2e87da10a0771.asset` |  |  |  | Remove-Item -LiteralPath 'config/cache/avatars/4bdcbb812c31b5a7e61d328c3be9185ba48e5a315e7c1e281db2e87da10a0771.asset' |
| `config/gui/create-menu.json` |  |  |  | Remove-Item -LiteralPath 'config/gui/create-menu.json' |
| `config/gui/my-creations-menu.json` |  |  |  | Remove-Item -LiteralPath 'config/gui/my-creations-menu.json' |
| `devscripts/scenarios/creator_map_loop_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/creator_map_loop_v1.json' |
| `devscripts/scenarios/creator_my_creations_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/creator_my_creations_v1.json' |
| `docs/changelog/2026-11-19/20261119_014413-full-creator-implementation-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_014413-full-creator-implementation-handoff.md' |
| `docs/changelog/2026-11-19/20261119_014847-arrow-camera-controls.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_014847-arrow-camera-controls.md' |
| `docs/changelog/2026-11-19/20261119_015547-dll-build-latency-handoff-and-timing-diagnosis.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_015547-dll-build-latency-handoff-and-timing-diagnosis.md' |
| `docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md' |
| `docs/changelog/2026-11-19/20261119_015722-dll-build-notes-and-result-journaling.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_015722-dll-build-notes-and-result-journaling.md' |
| `docs/changelog/2026-11-19/20261119_022044-juggernaut-collision-hot-path-followup.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_022044-juggernaut-collision-hot-path-followup.md' |
| `docs/changelog/2026-11-19/20261119_025627-juggernaut-collision-scratch-reuse.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_025627-juggernaut-collision-scratch-reuse.md' |
| `docs/changelog/2026-11-19/20261119_040731-dll-build-fast-path-and-attribution.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_040731-dll-build-fast-path-and-attribution.md' |
| `docs/changelog/2026-11-19/20261119_101204-death-animation-idle-reset.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_101204-death-animation-idle-reset.md' |
| `docs/changelog/2026-11-19/20261119_101520-mode-spawns-after-dll-reload.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_101520-mode-spawns-after-dll-reload.md' |
| `docs/changelog/2026-11-19/20261119_102415-mode-spawn-regression-record.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_102415-mode-spawn-regression-record.md' |
| `docs/changelog/2026-11-19/20261119_144558-juggernaut-performance-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_144558-juggernaut-performance-handoff.md' |
| `docs/changelog/2026-11-19/20261119_152943-universal-weapon-prediction-contract.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_152943-universal-weapon-prediction-contract.md' |
| `docs/changelog/2026-11-19/20261119_154403-browser-join-freeze-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_154403-browser-join-freeze-investigation.md' |
| `docs/changelog/2026-11-19/20261119_160000-universal-live-creation-ideal.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_160000-universal-live-creation-ideal.md' |
| `docs/changelog/2026-11-19/20261119_161553-creator-slice-1-create-map-editor-loop.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_161553-creator-slice-1-create-map-editor-loop.md' |
| `docs/changelog/2026-11-19/20261119_162600-command-driven-gui-testing-and-browser-join-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_162600-command-driven-gui-testing-and-browser-join-investigation.md' |
| `docs/changelog/2026-11-19/20261119_170056-mode-1-melee-client-prediction.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-19/20261119_170056-mode-1-melee-client-prediction.md' |
| `docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md' |
| `docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md' |
| `docs/handoffs/dll-build-one-second-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/dll-build-one-second-investigation.md' |
| `docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md' |
| `docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md' |
| `docs/specs/hotreload/hot-layer-latency-and-language.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/hotreload/hot-layer-latency-and-language.md' |

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

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused `git diff --check` on the three changed workflow files: passed.
- Documentation review: confirmed the obsolete mode-2 workflow instructions
  were removed from the three edited files and the referenced TDM/multi-instance
  command forms exist in current repo documentation.
- No builds, tests, launches, or gameplay runs were started for this docs-only
  change. The melee code's live acceptance remains pending a matching runtime.

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: Codex
- Focus: agent live runtime validation and exact executable journals
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 214849f07f9a4bc045d1461b1dbaf463a2084a85 / 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T15:17:05.094869Z", "latest_successful_build_id": "2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92", "latest_successful_source_generation": 33}, "game_current": {"build_mode": "debug", "code_hash": "15ab034481a080311cf4649f51c5d9165251689541786a96f1442bb8662bcf52", "dll": "build\\game\\generation-367\\game-g367.dll", "generation": 367, "revision": "949522a4338c6f6a25cc99800e56f4558b1e71f2672bb7de713e6699786f1de8", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-367\\game-g367.dll", "generation": 367, "published_utc": "2026-11-19T17:16:03.799Z", "revision": "949522a4338c6f6a25cc99800e56f4558b1e71f2672bb7de713e6699786f1de8", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 31.758, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-19T15:17:02.801Z"}}
- Generation: 367
- Artifact: build\game\generation-367\game-g367.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_152651\client-01\events.jsonl
- Session ID: session-20261119-171710

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
 M config/accounts/default.json
 M config/analytics.json
 M config/gui/main-menu.json
 M config/gui/map-editor.json
 M config/hot-reload/packet-ledger.json
 M config/weapons/chainsaw/config.json
 M config/weapons/mace/config.json
 M config/weapons/swordsword/config.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
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
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/dev-loop/dev-loop.md
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
 M src/gui/gui-main.cpp
 M src/gui/gui-main.h
 M src/gui/menus/main-menu.cpp
 M src/gui/menus/main-menu.h
 M src/input/input-poll.cpp
 M src/main-systems.cpp
 M src/mapeditor/map-editor-commands.cpp
 M src/mapeditor/map-editor-projects.cpp
 M src/mapeditor/map-editor-projects.h
 M src/mapeditor/map-editor.cpp
 M src/mapeditor/map-editor.h
 M src/network/server-gamemode.cpp
 M src/network/server-gamemode.h
 M src/network/server-packet-handlers.cpp
 M src/network/server-physical-contact-actors.cpp
 M src/npc/npc-nav-graph.cpp
 M src/npc/npc-nav-graph.h
 M src/npc/npc-navigator.cpp
 M src/physics/movement/physics-collision-body.cpp
 M src/physics/movement/physics-collision-glb-main.cpp
 M src/physics/movement/physics-collision-glb-sweep-slide.cpp
 M src/physics/movement/physics-collision-shared.h
 M src/physics/movement/physics-scratch.h
 M src/world/world-gltf-loader.cpp
 M src/world/world-gltf-loader.h
?? config/cache/avatars/4bdcbb812c31b5a7e61d328c3be9185ba48e5a315e7c1e281db2e87da10a0771.asset
?? config/gui/create-menu.json
?? config/gui/my-creations-menu.json
?? devscripts/scenarios/creator_map_loop_v1.json
?? devscripts/scenarios/creator_my_creations_v1.json
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
?? docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md
?? docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md
?? docs/handoffs/dll-build-one-second-investigation.md
?? docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md
?? docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md
?? docs/specs/hotreload/hot-layer-latency-and-language.md
```

## Captured diff stat

```text
AGENTS.md                                          |   10 +
 config/accounts/default.json                       |    2 +-
 config/analytics.json                              |    4 +-
 config/gui/main-menu.json                          |   16 +
 config/gui/map-editor.json                         |   20 +
 config/hot-reload/packet-ledger.json               |   42 +-
 config/weapons/chainsaw/config.json                |    2 +-
 config/weapons/mace/config.json                    |   10 +-
 config/weapons/swordsword/config.json              |   14 +-
 devscripts/blender-ai                              |    0
 devscripts/dev-loop.py                             |   68 +-
 devscripts/dev_loop/game_build.py                  |  204 +++-
 devscripts/dev_loop/gui.py                         |  207 +++-
 devscripts/game_build_worker.py                    | 1163 ++++++++++++++++----
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
 docs/specs/dev-loop/build-time-observability.md    |  205 +++-
 docs/specs/dev-loop/dev-loop.md                    |   23 +-
 docs/specs/gamemodes/zombie-attack.md              |   30 +
 docs/specs/hotreload/hotreload.md                  |   29 +
 docs/specs/movement/movement.md                    |    2 +
 docs/specs/networking/contact-authority.md         |   78 +-
 docs/specs/networking/networking.md                |   91 +-
 docs/specs/weapons/melee/melee-weapons.md          |   75 +-
 docs/specs/weapons/weapons.md                      |   38 +-
 docs/workflows/multi-client-dev-testing.md         |   18 +-
 docs/workflows/runtime-scenario-validation.md      |   64 +-
 src/combat/melee-client.cpp                        |   33 +-
 src/engine/engine-tick-camera.cpp                  |   61 +
 src/engine/engine-tick-state.cpp                   |  115 ++
 src/entities/player-animation.cpp                  |   63 +-
 src/game/server-command-dispatch-module.cpp        |   39 +-
 src/game/server-runtime-module.cpp                 |   88 ++
 src/gui/gui-main.cpp                               |  296 +++++
 src/gui/gui-main.h                                 |   23 +
 src/gui/menus/main-menu.cpp                        |    4 +-
 src/gui/menus/main-menu.h                          |    2 +
 src/input/input-poll.cpp                           |   22 +-
 src/main-systems.cpp                               |  241 ++++
 src/mapeditor/map-editor-commands.cpp              |  160 +++
 src/mapeditor/map-editor-projects.cpp              |  253 +++++
 src/mapeditor/map-editor-projects.h                |   52 +
 src/mapeditor/map-editor.cpp                       |  290 ++++-
 src/mapeditor/map-editor.h                         |   20 +
 src/network/server-gamemode.cpp                    |   42 +
 src/network/server-gamemode.h                      |    6 +
 src/network/server-packet-handlers.cpp             |    4 +-
 src/network/server-physical-contact-actors.cpp     |    6 +-
 src/npc/npc-nav-graph.cpp                          |  114 +-
 src/npc/npc-nav-graph.h                            |    5 +-
 src/npc/npc-navigator.cpp                          |    4 +-
 src/physics/movement/physics-collision-body.cpp    |    8 +-
 .../movement/physics-collision-glb-main.cpp        |   81 +-
 .../movement/physics-collision-glb-sweep-slide.cpp |   29 +-
 src/physics/movement/physics-collision-shared.h    |    1 +
 src/physics/movement/physics-scratch.h             |    5 +
 src/world/world-gltf-loader.cpp                    |    7 +-
 src/world/world-gltf-loader.h                      |    4 +-
 70 files changed, 4289 insertions(+), 555 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/gui/main-menu.json
M	config/gui/map-editor.json
M	config/hot-reload/packet-ledger.json
M	config/weapons/chainsaw/config.json
M	config/weapons/mace/config.json
M	config/weapons/swordsword/config.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
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
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/dev-loop/dev-loop.md
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
M	src/gui/gui-main.cpp
M	src/gui/gui-main.h
M	src/gui/menus/main-menu.cpp
M	src/gui/menus/main-menu.h
M	src/input/input-poll.cpp
M	src/main-systems.cpp
M	src/mapeditor/map-editor-commands.cpp
M	src/mapeditor/map-editor-projects.cpp
M	src/mapeditor/map-editor-projects.h
M	src/mapeditor/map-editor.cpp
M	src/mapeditor/map-editor.h
M	src/network/server-gamemode.cpp
M	src/network/server-gamemode.h
M	src/network/server-packet-handlers.cpp
M	src/network/server-physical-contact-actors.cpp
M	src/npc/npc-nav-graph.cpp
M	src/npc/npc-nav-graph.h
M	src/npc/npc-navigator.cpp
M	src/physics/movement/physics-collision-body.cpp
M	src/physics/movement/physics-collision-glb-main.cpp
M	src/physics/movement/physics-collision-glb-sweep-slide.cpp
M	src/physics/movement/physics-collision-shared.h
M	src/physics/movement/physics-scratch.h
M	src/world/world-gltf-loader.cpp
M	src/world/world-gltf-loader.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-19T15:17:05.094869Z",
    "latest_successful_build_id": "2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92",
    "latest_successful_source_generation": 33
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "15ab034481a080311cf4649f51c5d9165251689541786a96f1442bb8662bcf52",
    "dll": "build\\game\\generation-367\\game-g367.dll",
    "generation": 367,
    "revision": "949522a4338c6f6a25cc99800e56f4558b1e71f2672bb7de713e6699786f1de8",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-367\\game-g367.dll",
    "generation": 367,
    "published_utc": "2026-11-19T17:16:03.799Z",
    "revision": "949522a4338c6f6a25cc99800e56f4558b1e71f2672bb7de713e6699786f1de8",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 31.758,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-19T15:17:02.801Z"
  }
}
```
