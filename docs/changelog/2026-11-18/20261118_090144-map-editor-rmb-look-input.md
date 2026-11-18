// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Map editor RMB look input

# Task

- Task ID: session-20261118-090142
- Session ID: session-20261118-090142
- Owner: Codex
- Focus: Map editor camera input
- Summary: Fix right-mouse look by engaging InputRouter pointer lock only while editor RMB look is active; document existing block creation capability and note runtime verification remains open.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T09:01:44+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The checkout already contained numerous unrelated config, gameplay, dev-loop, documentation, and changelog changes. They were preserved. `src/mapeditor/map-editor.cpp` was clean before this task; only this task's pointer-lock changes were added there.
- The changelog generator captured the full checkout status and build state in its automatic evidence section. No build request was made during this task.

# Requested behavior

Restore RMB-held mouse look in the Map Editor before expanding map authoring. Confirm existing block and wall creation paths and assess the requested random-texture capability.

# Specification alignment

- Current specification: `docs/specs/entity-editor/entity-editor.md`, “Map Editor v1 viewport mode”.
- Exact requirement: “RMB held mouse look”.
- `MapEditor::updateInput` selected `mLooking` and changed the host cursor mode, but never engaged `InputRouter` pointer lock. `InputRouter::ingestCursor` drops host look deltas while pointer lock is false; map editor intentionally unlocks it for free cursor interaction.
- `Authored blocks` documents map-level box blocks. `Chunk primitive authoring` documents `Add Block` plus the `wall` chunk-part kind. Existing `MapEditor::addBlock` creates a 2 m box in front of the camera; it can be resized using the shared size editing controls. No random-texture authoring control or authored-block random texture data exists in the inspected editor path. The random-texture request needs a defined texture source/selection behavior before extending the data model.

# Exact implementation changes

## File: `src/mapeditor/map-editor.cpp`

- Functions: `MapEditor::updateInput` inactive, blocked, RMB-press, and RMB-release branches.
- Old behavior: entering RMB look enabled host cursor mode 2, but left `InputRouter` pointer lock false, so cursor deltas were ignored by the camera look path. Interruptions also only restored the host cursor mode.
- New behavior: engage `InputRouter::setPointerLock(true)` when RMB look starts; release it on RMB release and when editor input is inactive or blocked.
- Reason: feed editor RMB deltas into the existing shared camera-look owner while retaining the editor's free cursor whenever RMB is not held.
- Unrelated behavior is preserved: right-click on an outliner row still opens its context menu; LMB selection/drag and freecam movement are untouched.

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
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/mapeditor/map-editor.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor.cpp |
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

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill: `docs/skills/spec-behavior-review-v1.md` — PASS for source/spec trace; runtime acceptance remains open.
- Static review: inspected `MapEditor::updateInput`, `InputRouter::ingestCursor`, and `InputRouter::applyPendingLook`; verified the new pointer lock gates the already-existing camera look delta path.
- Tests: none run. No build or build request was run; agents do not build under the shared-checkout authority.
- Runtime/hot reload: NOT VERIFIED. The existing game generation predates this edit; no claim that the new behavior is live is made.
- Output: source edit only plus this changelog.

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

- Visual review: not performed.
- Gameplay review: not performed; human should enter Map Editor, hold RMB and move the mouse, release RMB and confirm the cursor returns and LMB still selects/drags.
- Multiplayer review:
- Still unverified: compilation/publication and live RMB behavior. Random texture application is not implemented; map-level block creation and resizing already exist, and the chunk authoring path includes a wall primitive.

# Agent handoff context

- Owner: Codex
- Focus: Map editor camera input
- Files to check: `src/mapeditor/map-editor.cpp`, `src/input/input-router.cpp`, `docs/specs/entity-editor/entity-editor.md`.
- What another agent should not redo: pointer-lock source fix and input path trace.
- Next exact step: have the human validate RMB look through the canonical dev-loop, then define the random texture source and expected assignment behavior before implementing it.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "663409204f6bea889d3791b620bc58297c98ba3d756bd4848ec5198be08beb75", "dll": "build\\game\\generation-335\\game-g335.dll", "generation": 335, "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-335\\game-g335.dll", "generation": 335, "published_utc": "2026-11-18T07:29:35.088Z", "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 335
- Artifact: build\game\generation-335\game-g335.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_085322\server-01\events.jsonl
- Session ID: session-20261118-090142

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
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/mapeditor/map-editor.cpp
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
 config/maps/funworld3.json                         | 45 +++++++++++++-
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
 src/mapeditor/map-editor.cpp                       |  4 ++
 src/network/multiplayer-context.h                  |  4 ++
 src/network/multiplayer-projectiles.cpp            | 19 ++++++
 src/npc/npc-navigator.cpp                          | 21 +++++--
 src/npc/npc.cpp                                    |  8 ++-
 41 files changed, 487 insertions(+), 82 deletions(-)
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
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/mapeditor/map-editor.cpp
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
