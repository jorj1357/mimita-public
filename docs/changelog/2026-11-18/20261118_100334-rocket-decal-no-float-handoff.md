// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Rocket Decal No-Float Handoff

# Task

- Task ID: session-20261118-100332
- Session ID: session-20261118-100332
- Owner: Codex
- Focus: Rocket impact decal placement handoff
- Summary: Write a concrete follow-up plan to diagnose and eliminate floating rocket impact decals using world-surface evidence and live visual acceptance.
- Status: NOT_APPLICABLE
- Date, time, timezone: 2026-11-18T10:03:34+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The shared checkout already contained edits across config, dev-loop, source,
  regression, and specification files, including the prior unverified rocket
  impact FX implementation. This session changed none of those files.
- At inspection, root dev-loop status had no run active, game build idle, and
  published EXE stale. The ready game generation was 344; the latest EXE build
  identity was 2026-11-18T05:35:43.892563Z.

# Requested behavior

- Create a practical handoff plan for another agent to eliminate rocket impact
  decals floating in the air, avoid actor body anchors, and prove the result.
- Treat the user screenshot as visual evidence, not as instructions or proof
  tied to a particular build.

# Specification alignment

- Routed documents: `docs/ROUTER.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`,
  `docs/specs/weapons/weapons.md` (shockwave section),
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`, and
  `docs/skills/documentation-checker-v1.md`.
- The handoff requires real-executable and journal evidence separately from
  human visual acceptance, follows the root dev-loop authority, and does not
  promote the earlier source attempt to a proven fix.
- No behavior specification was changed. Screenshot content is treated as
  evidence only; the user's text defines the requested outcome.

# Exact implementation changes

## `docs/handoffs/rocket-impact-decal-no-float-handoff.md`

- Added symptom/evidence separation, current source/log/runtime state, a
  resolver-to-render trace plan, and no-floating acceptance conditions.
- Calls out the current 8 m world-triangle query and 4 m explosion-mark radius
  as unproven placement and possible size contributors, not established root
  causes.
- Identifies existing `hitfx_trace 1`, `rocket.impact-mark.resolved`,
  `impact_decal.spawn`, and `impact_decal.render_submitted` evidence paths.
- The handoff changes no gameplay or source files; unrelated checkout edits
  remain untouched.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
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
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/regressions/collisions/README.md` |  |  |  | git checkout -- docs/regressions/collisions/README.md |
| `docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md` |  |  |  | git checkout -- docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md |
| `docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md` |  |  |  | git checkout -- docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/regressions/gameplay/README.md` |  |  |  | git checkout -- docs/regressions/gameplay/README.md |
| `docs/regressions/networking/README.md` |  |  |  | git checkout -- docs/regressions/networking/README.md |
| `docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md` |  |  |  | git checkout -- docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
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
| `docs/handoffs/rocket-impact-decal-no-float-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/rocket-impact-decal-no-float-handoff.md' |
| `docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md' |
| `docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md' |
| `docs/regressions/gameplay/console-grave-toggle-REG-2026-11-18.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/gameplay/console-grave-toggle-REG-2026-11-18.md' |
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

- `docs/skills/documentation-checker-v1.md`: review the handoff purpose,
  audience, scope, route references, and unsupported claims; see manual check
  results below.
- Documentation check: links and referenced source/config/log paths exist;
  the handoff states purpose, audience, scope, and what it does not claim.
- Documentation TODO scan found only the router/checker instructions and older
  changelog quotations describing the scan; no actionable TODO comment was
  found in the new handoff.
- No tests or builds were run; this is a documentation-only task.
- Read-only evidence: root dev-loop `--status` showed no run and idle game
  build. The run `20261118_093933` contains shockwave spawn/draw records but no
  queried rocket mark/decal trace events, so it is not decal-placement proof.
- Output: `docs/handoffs/rocket-impact-decal-no-float-handoff.md`.

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: no.
- The user reports a continuing visual bug; this session records a plan only
  and does not verify or claim a fix.
- Related regression index: `docs/regressions/gameplay/README.md`.

# Human acceptance

- Visual review: the user supplied a screenshot used as problem context; no
  new in-game review was performed in this documentation-only turn.
- Gameplay review: not performed.
- Multiplayer review:
- Still unverified: the no-floating behavior, current binary activation,
  target geometry, and human visual acceptance.

# Agent handoff context

- Owner: Codex
- Focus: Rocket impact decal placement handoff
- Files to check: handoff doc, rocket impact resolver, decal storage/render
  owners, `config/impact_decals.json`, and run `20261118_093933` journal.
- What another agent should not redo: initial source/spec inventory and the
  documented hypothesis list; these are planning inputs, not accepted fixes.
- Next exact step: follow the new handoff to reproduce the screenshot symptom
  on the active build, correlate resolver/spawn/render events by decal ID, fix
  the first divergence, and obtain multi-angle human visual acceptance.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "eac97971d34d03cbda79c4740797bb34485e023b0963c6bd0ad4726fb83be5c9", "dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "published_utc": "2026-11-18T09:50:03.870Z", "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 344
- Artifact: build\game\generation-344\game-g344.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_093933\client-01\events.jsonl
- Session ID: session-20261118-100332

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
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M docs/regressions/collisions/README.md
 M docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
 M docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/build-time-observability.md
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
?? docs/handoffs/rocket-impact-decal-no-float-handoff.md
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
?? docs/regressions/gameplay/console-grave-toggle-REG-2026-11-18.md
?? docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md
?? docs/regressions/gameplay/map-editor-keyboard-routing-REG-2026-11-18.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   2 +-
 config/gamemodes/funcounterstrike.json             |   8 +-
 config/healthbar.json                              |   2 -
 config/maps/funworld3.json                         |  85 ++++++++++++-
 config/movement.json                               |   1 +
 config/movement/movement-source.json               |  10 +-
 config/video-settings.json                         |   2 +-
 config/weaponcollisions.json                       |   2 +-
 config/weapons/rocket_launcher/config.json         |  12 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |   1 +
 devscripts/dev-loop.py                             |  40 ++++++
 devscripts/dev-profiles/npc-navigation.json        |   2 +-
 devscripts/dev_loop/game_build.py                  |  32 ++++-
 devscripts/dev_loop/gui.py                         | 122 +++++++++++++++++-
 devscripts/game_build_worker.py                    |  58 +++++++--
 docs/regressions/collisions/README.md              |   4 +-
 .../physical-objects-collision-REG-2026-10-01.md   |  69 +++++++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |   9 ++
 docs/regressions/dev-loop/README.md                |  16 ++-
 docs/regressions/gameplay/README.md                |  24 +++-
 docs/regressions/networking/README.md              |   2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    |  30 +++--
 docs/specs/dev-loop/build-time-observability.md    |  11 ++
 docs/specs/dev-loop/dashboard.md                   |  31 ++++-
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
 src/game/game-input-module.cpp                     |  29 ++++-
 src/gui/hud/player-nameplates.cpp                  |   8 ++
 src/gui/hud/player-nameplates.h                    |   3 +-
 src/gui/ui-text-input.cpp                          |  84 ++++++++++++-
 src/gui/ui-text-input.h                            |  11 ++
 src/mapeditor/map-editor.cpp                       |  61 +++++++--
 src/network/multiplayer-context.h                  |   4 +
 src/network/multiplayer-projectiles.cpp            |  19 +++
 src/npc/npc-nav-graph.cpp                          |  32 +++--
 src/npc/npc-navigator.cpp                          |  21 +++-
 src/npc/npc.cpp                                    |  54 +++++++-
 49 files changed, 1116 insertions(+), 136 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
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
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	docs/regressions/collisions/README.md
M	docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
M	docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/build-time-observability.md
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
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-18T05:35:43.892563Z",
    "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "eac97971d34d03cbda79c4740797bb34485e023b0963c6bd0ad4726fb83be5c9",
    "dll": "build\\game\\generation-344\\game-g344.dll",
    "generation": 344,
    "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-344\\game-g344.dll",
    "generation": 344,
    "published_utc": "2026-11-18T09:50:03.870Z",
    "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286",
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
