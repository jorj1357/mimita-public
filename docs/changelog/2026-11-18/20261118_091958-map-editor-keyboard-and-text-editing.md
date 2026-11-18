// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Map editor keyboard and text editing

# Task

- Task ID: session-20261118-091955
- Session ID: session-20261118-091955
- Owner: Codex
- Focus: Map editor keyboard input
- Summary: Route hot-client key events into Map Editor shortcuts, add bounded text-field undo/redo, clarify field versus map shortcut priority, and record the reported regression; runtime verification remains pending.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T09:19:57+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The shared checkout contained unrelated dirty config, gameplay, dev-loop, map, and regression files before this work. They were preserved.
- `src/mapeditor/map-editor.cpp` already had this thread's earlier RMB pointer-lock edit; this task adds text-field history resets and focused-field Ctrl+S routing there. No pre-existing edits in the other source files touched here were overwritten.
- The generator captured the full checkout status below, which includes prior work. The exact edits owned by this task are listed under “Exact implementation changes”.

# Requested behavior

Make the Map Editor keyboard work in the hot client: deliver keyboard events to editor shortcuts and focused text boxes; support typing, Ctrl+X/C/V/A, Ctrl+Z/Y and Ctrl+Shift+Z in text fields; retain map-object shortcuts when no field is focused; and add the issue/fix to the regressions documentation.

# Specification alignment

- Current specification: `docs/specs/entity-editor/entity-editor.md`, “Map Editor v1 viewport mode”.
- Exact requirements: “Ctrl+C/V/X copy / paste / cut”, “Ctrl+Z undo”, “Ctrl+Shift+Z/Y redo”, and “Enter apply the focused position field”.
- Source trace: the resident `InputCommandSystem::keyCallback` routes `MapEditor::handleTextKey` and `handleKey`; the hot-client `mimita::game::handleGameInputKey` did not. Shared text handling implemented Ctrl+A/C/X/V but not local undo/redo.
- Alignment: host-dispatched key events now use the same Map Editor key owners. Text-local shortcuts take precedence over object shortcuts when a field is focused; Ctrl+S remains a map save. The spec now states those priorities.
- No unresolved design choice. Runtime behavior remains unverified because the current dev-loop status showed a build in progress and no running client.

# Exact implementation changes

## Files changed

- `src/game/game-input-module.cpp`, `handleGameInputKey`: terminal retains first priority; after chat, host-dispatched input now gives focused Map Editor fields first chance, then dispatches remaining keys to Map Editor shortcuts.
- `src/gui/ui-text-input.h` and `src/gui/ui-text-input.cpp`, `UITextInputState` / `uiTextInputHandleKey`: adds bounded 64-edit field-local snapshots. Typing, cut, paste, backspace, and delete enter undo history. Ctrl+Z restores; Ctrl+Y and Ctrl+Shift+Z redo.
- `src/mapeditor/map-editor.cpp`, `handleTextKey` and property synchronization: Ctrl+S passes to map save; programmatic property updates and selection changes reset field history to prevent undo from restoring text belonging to a prior object.
- `docs/specs/entity-editor/entity-editor.md`, viewport controls: clarifies focused-field versus map-object Ctrl shortcuts.
- `docs/regressions/gameplay/README.md` Entry 43 and `docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md`: records report, source cause/fix, and pending runtime acceptance.
- Old behavior: hot key events bypassed Map Editor key handling; focused-field handlers consumed unimplemented text undo/redo shortcuts.
- Why unrelated behavior is preserved: terminal and chat still receive keys first; text fields own their clipboard and text history; map-object operations remain active when no field has focus.

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
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/regressions/collisions/README.md` |  |  |  | git checkout -- docs/regressions/collisions/README.md |
| `docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md` |  |  |  | git checkout -- docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md |
| `docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md` |  |  |  | git checkout -- docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/gameplay/README.md` |  |  |  | git checkout -- docs/regressions/gameplay/README.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` |  |  |  | git checkout -- docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/dev-loop.md` |  |  |  | git checkout -- docs/specs/dev-loop/dev-loop.md |
| `docs/specs/entity-editor/entity-editor.md` |  |  |  | git checkout -- docs/specs/entity-editor/entity-editor.md |
| `docs/specs/gamemodes/funcounterstrike.md` |  |  |  | git checkout -- docs/specs/gamemodes/funcounterstrike.md |
| `docs/specs/open-gameplay-issues.md` |  |  |  | git checkout -- docs/specs/open-gameplay-issues.md |
| `docs/specs/performance/performance.md` |  |  |  | git checkout -- docs/specs/performance/performance.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/explosion-fx.h` |  |  |  | git checkout -- src/combat/explosion-fx.h |
| `src/combat/weapon-rocket-launcher.cpp` |  |  |  | git checkout -- src/combat/weapon-rocket-launcher.cpp |
| `src/effects/effect-part-render.cpp` |  |  |  | git checkout -- src/effects/effect-part-render.cpp |
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/game/game-input-module.cpp` |  |  |  | git checkout -- src/game/game-input-module.cpp |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/gui/ui-text-input.cpp` |  |  |  | git checkout -- src/gui/ui-text-input.cpp |
| `src/gui/ui-text-input.h` |  |  |  | git checkout -- src/gui/ui-text-input.h |
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
| `docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md' |

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

- Focused skill: `docs/skills/spec-behavior-review-v1.md` — PASS for identifying the spec/code disagreement and recording runtime acceptance as pending.
- Validation command: `git diff --check -- src/game/game-input-module.cpp src/gui/ui-text-input.h src/gui/ui-text-input.cpp src/mapeditor/map-editor.cpp docs/specs/entity-editor/entity-editor.md docs/regressions/gameplay/README.md docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md` — passed.
- Tests: none run. No build was requested by this agent.
- Last permitted dev-loop status observation: `lifecycle=source_changed`; `game-build status=building generation=338`; `run=NOT RUNNING`. No build wait or repeat poll was performed.
- Runtime/hot reload: pending. The previously active generation 338 was built before these keyboard edits and is not evidence for them. No runtime behavior claim is made.
- Output: source/spec/regression edits and this changelog.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes — Entry 43 in `docs/regressions/gameplay/README.md`.
- The hot-client path and shared text-widget gaps are confirmed from source. The user-reported symptom has not yet been reproduced in this generation; regression status remains source-fix saved with runtime validation pending.
- Related regression: `docs/regressions/gameplay/input-hot-client-REG-2026-11-15.md`.

# Human acceptance

- Visual review: not performed for this change.
- Gameplay review: no client was running after these edits; text-field and map-shortcut interaction remain unverified.
- Multiplayer review:
- Still unverified: successful game-DLL build, same-PID activation, live input routing, text undo/redo, map shortcuts, and visible field contents after each operation.

# Agent handoff context

- Owner: Codex
- Focus: Map editor keyboard input
- Files to check: the five source/spec files and the Entry 43 regression record listed above.
- What another agent should not redo: the hot-client key-route trace or the field-local undo/redo implementation.
- Next exact step: after the root dev-loop publishes the saved inputs, run the target editor client, verify activation and keyboard interaction in its journal/visible UI, and update the regression status with that evidence.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "2c70529f5fa72185c27cae6eac4c4268b204e50ecd38d19e40c8cb7876149360", "dll": "build\\game\\generation-338\\game-g338.dll", "generation": 338, "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-338\\game-g338.dll", "generation": 338, "published_utc": "2026-11-18T09:07:45.314Z", "revision": "aab134f7e40ba432b7a2d340a08a8b673832cc3e88c054ac6408ded540d1a604", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 338
- Artifact: build\game\generation-338\game-g338.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_091931\server-01\events.jsonl
- Session ID: session-20261118-091955

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
 M devscripts/game_build_worker.py
 M docs/regressions/collisions/README.md
 M docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
 M docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/entity-editor/entity-editor.md
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/open-gameplay-issues.md
 M docs/specs/performance/performance.md
 M src/combat/explosion-fx.cpp
 M src/combat/explosion-fx.h
 M src/combat/weapon-rocket-launcher.cpp
 M src/effects/effect-part-render.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/game/game-input-module.cpp
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/gui/ui-text-input.cpp
 M src/gui/ui-text-input.h
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
?? docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md
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
 devscripts/game_build_worker.py                    |  47 +++++--
 docs/regressions/collisions/README.md              |   4 +-
 .../physical-objects-collision-REG-2026-10-01.md   |  69 +++++++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |   9 ++
 docs/regressions/dev-loop/README.md                |  16 ++-
 docs/regressions/gameplay/README.md                |  16 ++-
 docs/regressions/networking/README.md              |   2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    |  30 +++--
 docs/specs/dev-loop/dashboard.md                   |  16 ++-
 docs/specs/dev-loop/dev-loop.md                    |  40 ++++--
 docs/specs/entity-editor/entity-editor.md          |  13 +-
 docs/specs/gamemodes/funcounterstrike.md           |   3 +
 docs/specs/open-gameplay-issues.md                 |  14 ++-
 docs/specs/performance/performance.md              |  33 +++++
 src/combat/explosion-fx.cpp                        |  30 +++--
 src/combat/explosion-fx.h                          |   8 +-
 src/combat/weapon-rocket-launcher.cpp              | 137 +++++++++++++++++++--
 src/effects/effect-part-render.cpp                 |  56 +++++++--
 src/effects/effect-part.cpp                        |   7 +-
 src/effects/effect-part.h                          |   4 +-
 src/game/game-input-module.cpp                     |  17 ++-
 src/gui/hud/player-nameplates.cpp                  |   8 ++
 src/gui/hud/player-nameplates.h                    |   3 +-
 src/gui/ui-text-input.cpp                          |  84 ++++++++++++-
 src/gui/ui-text-input.h                            |  11 ++
 src/mapeditor/map-editor.cpp                       |  61 +++++++--
 src/network/multiplayer-context.h                  |   4 +
 src/network/multiplayer-projectiles.cpp            |  19 +++
 src/npc/npc-nav-graph.cpp                          |  32 +++--
 src/npc/npc-navigator.cpp                          |  21 +++-
 src/npc/npc.cpp                                    |   8 +-
 48 files changed, 888 insertions(+), 134 deletions(-)
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
M	devscripts/game_build_worker.py
M	docs/regressions/collisions/README.md
M	docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
M	docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/entity-editor/entity-editor.md
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/open-gameplay-issues.md
M	docs/specs/performance/performance.md
M	src/combat/explosion-fx.cpp
M	src/combat/explosion-fx.h
M	src/combat/weapon-rocket-launcher.cpp
M	src/effects/effect-part-render.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/game/game-input-module.cpp
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/gui/ui-text-input.cpp
M	src/gui/ui-text-input.h
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
    "latest_successful_build": "2026-11-17T21:12:47.128057Z",
    "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac",
    "latest_successful_source_generation": 98
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
