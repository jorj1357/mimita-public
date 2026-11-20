// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Mode 1 melee client prediction

# Task

- Task ID: session-20261119-170053
- Session ID: session-20261119-170053
- Owner: Codex
- Focus: Mode 1 melee networking and contact
- Summary: Focused Chainsaw, Mace, and SwordSword on per-tick Mode 1 contact claims, immediate provisional health prediction, plausible claim bounds, and Mode 2 phase specification. Runtime confirmation still awaits an artifact containing these edits.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T17:00:56+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

- Exact status output: The checkout already contained broad unrelated changes
  across creator tooling, dev-loop/build code, networking specs, movement, and
  rendering. The automatically captured path list below records them. This
  session changed only the seven focused files listed in its implementation
  section and table; all other paths were preserved.
- Files not created or modified by this session: Every captured changed path
  except the seven focused paths below.

# Requested behavior

Focus only on Chainsaw, Mace, and SwordSword for Mode 1: contact is continuously
active while equipped, physical contact determines damage/force, the client
predicts immediately, and the server validates plausible historical contact.
Specify Mode 2's idle/windup/active/recovery phases but do not implement it.
Defer Spy Knife and Godball.

# Specification alignment

- Current specification paths: `docs/specs/weapons/melee/melee-weapons.md` and
  the pre-existing `docs/specs/networking/contact-authority.md` target contract.
- Exact requirements: Mode 1 may contact once per target per weapon per 60 Hz
  tick without an attack click; Mode 2 owns a hitbox only during authored
  active ticks; clients predict and the server validates/adopts plausible
  historical contact.
- Why the change follows the specification: the client now checks contact
  outside an active action and predicts the remote health overlay immediately;
  selected configs send at one-tick cadence; Mace is no longer excluded by the
  generic server batch handler's animation-only gate.
- Conflicts or decisions: the server's existing actor contact scan remains a
  fallback, so this is not yet a fully single-producer/single-query migration.
  Runtime acceptance and rejection reconciliation are not proven because no
  executable containing these edits has been published and exercised.

# Exact implementation changes

## Files: client and server source

- `src/combat/melee-client.cpp:410` (`tickClientMeleeContactClaims`): removed
  the early return that stopped contact checks while no melee action was
  active. Added `mpApplyPredictedDamage` on the contact tick and records
  predicted NPC damage for existing confirmation/mismatch handling. Claims
  continue using the configured batch cadence; the three selected configs now
  set it to one tick. Action windows no longer alter Mode 1 contact damage.
- `src/network/server-packet-handlers.cpp:1065`
  (`handleGenericMeleeContactBatch`): old `!set || !set->animationOnly` gate
  rejected Mace; it now requires only a registered move set. Mode 1 claims do
  not use action active windows to alter damage.
- `src/network/server-physical-contact-actors.cpp:658`: disabled action-window
  damage overrides for configured Mode 1 contact weapons; measured relative
  speed/directness and configured contact values remain the inputs.
- Unrelated source behavior is preserved; no packet ABI or new packet type was
  added.

## Files: weapon config and melee specification

- `config/weapons/{chainsaw,mace,swordsword}/config.json`: changed claim cadence
  from 6 ticks to 1. Aligned contact tolerance, target radius, and derived
  reach with the existing 0.75 m / 0.9 m / `0` policy for the selected three.
- `docs/specs/weapons/melee/melee-weapons.md:189`: defined continuous Mode 1
  contact, force/speed damage, immediate provisional health prediction, and
  one-tick claim cadence for the selected weapons; explicitly deferred Spy
  Knife and Godball.
- `docs/specs/weapons/melee/melee-weapons.md:256`: replaced Mode 2 TODO with
  `idle -> windup -> active -> recovery -> idle`; the authored hitbox exists
  only during active ticks. Mode 2 remains unimplemented.
- `docs/specs/weapons/melee/melee-weapons.md:1350`: labeled the existing
  server contact scan as a migration gap rather than claiming full
  single-path behavior.
- Reverse only the focused lines/files listed above; do not revert the
  pre-existing dirty paths captured below.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gui/main-menu.json` |  |  |  | git checkout -- config/gui/main-menu.json |
| `config/gui/map-editor.json` |  |  |  | git checkout -- config/gui/map-editor.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/weapons/chainsaw/config.json` | 0.9 m / 1.0 m / derived reach; 6 ticks | 0.75 m / 0.9 m / derived reach; 1 tick | match focused Mode 1 server validation and cadence | restore these four values only |
| `config/weapons/mace/config.json` | 2.0 m / 1.1 m / 50 m reach; 6 ticks | 0.75 m / 0.9 m / derived reach; 1 tick | prevent implausible remote claims and remove batch delay | restore these four values only |
| `config/weapons/swordsword/config.json` | 5.75 m / 4.9 m / 10 m reach; 6 ticks | 0.75 m / 0.9 m / derived reach; 1 tick | prevent implausible remote claims and remove batch delay | restore these four values only |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/source_manifest.py` |  |  |  | git checkout -- devscripts/source_manifest.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/architecture/live-development/incremental-build-and-header-fanout.md` |  |  |  | git checkout -- docs/architecture/live-development/incremental-build-and-header-fanout.md |
| `docs/architecture/live-development/module-migration-plan.md` |  |  |  | git checkout -- docs/architecture/live-development/module-migration-plan.md |
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
| `docs/specs/weapons/melee/melee-weapons.md` | Mode 2 TODO; batch guidance suggested 2–3 ticks; scope named Spy Knife | specified Mode 2 phases; focused three use 1 tick; Spy Knife/Godball deferred | match the selected user scope and target behavior | reverse only the changed sections |
| `docs/specs/weapons/weapons.md` |  |  |  | git checkout -- docs/specs/weapons/weapons.md |
| `src/combat/melee-client.cpp` | stopped outside active moves; waited for server-confirmed health | checks contact continuously; applies provisional health and sends one-tick claims | immediate Mode 1 prediction | reverse only this session's diff |
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
| `src/network/server-packet-handlers.cpp` | generic batch required animation-only move set | accepts any registered move set; Mode 1 ignores active-window damage overrides | allow Mace through the shared claim resolver | reverse only this session's diff |
| `src/network/server-physical-contact-actors.cpp` | Mode 1 damage profile could depend on active action window | configured Mode 1 contact uses its physical contact profile outside action windows | make action animate the weapon without opening/closing damage | reverse only this session's diff |
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
| `docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md' |
| `docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md' |
| `docs/handoffs/dll-build-one-second-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/dll-build-one-second-investigation.md' |
| `docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md' |
| `docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md' |
| `docs/specs/hotreload/hot-layer-latency-and-language.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/hotreload/hot-layer-latency-and-language.md' |

# Contract / ABI changes

None. Existing melee batch packet and shared contact/damage resolver are reused.

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

No schema changes. Runtime weapon config values changed only for the three
focused weapons.

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

Existing `melee.client.contact_claim`, `melee.contact.batch.received`, and
server accepted/rejected contact events remain the evidence chain. Client
claim event feedback now distinguishes `predicted_then_reconciled` from
`claim_only`.

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- `git diff --check`: passed after removing whitespace on the edited Mace line;
  remaining output was Git's pre-existing LF/CRLF working-copy warnings.
- `python devscripts/weapon_json_sync.py --check`: passed, masterlist=1,
  definitions=36, errors=0.
- Build status: no build requested or awaited. The dev-loop still reports the
  prior artifact `2026-11-19T15:17:05.094869Z` / source generation 33 while
  this checkout is at source generation 104; its status is `game_build_failed`.
- Runtime evidence: not established. Run `20261119_165514` has the old build
  ID `2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92`.
  No melee action was run on an executable containing these changes.
- Build notes queued before compiler-input edits: `669e9d4c181e` for client and
  packet-handler changes; `f0541da2a7fb` for actor-contact profile changes.

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

No gameplay measurements yet. The static cadence is configured to one fixed
tick for the selected three; packet throughput and predicted-vs-accepted
agreement require a fresh live run.

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

No confirmed regression recorded. Source behavior is not runtime-verified.

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

Pending. Human review must confirm contact feels immediate and visible hits
match server-applied damage in actual play.

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

Continue by observing the root dev-loop's natural build result, launch the
exact artifact through its supported control path, and exercise Chainsaw,
Mace, and SwordSword against NPC and player targets. Compare correlated client
claim, server receipt/decision, and damage/reconciliation events. Preserve the
current shared checkout and its unrelated edits.

- Owner: Codex
- Focus: Mode 1 melee networking and contact
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 214849f07f9a4bc045d1461b1dbaf463a2084a85 / 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T15:17:05.094869Z", "latest_successful_build_id": "2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92", "latest_successful_source_generation": 33}, "game_current": {"build_mode": "debug", "code_hash": "25270f2f65a1543ed2849d70dfb3b948bf62a2529b9212fc1d7e7b3b7a7efe53", "dll": "build\\game\\generation-366\\game-g366.dll", "generation": 366, "revision": "5790ee8c1a4bad2e5572ee004c22514e42b5722bb9fd2904469b55d275b2ca35", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-366\\game-g366.dll", "generation": 366, "published_utc": "2026-11-19T16:52:49.000Z", "revision": "5790ee8c1a4bad2e5572ee004c22514e42b5722bb9fd2904469b55d275b2ca35", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 31.758, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-19T15:17:02.801Z"}}
- Generation: 366
- Artifact: build\game\generation-366\game-g366.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_165514\server-01\events.jsonl
- Session ID: session-20261119-170053

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
 config/hot-reload/packet-ledger.json               |   40 +-
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
 docs/specs/weapons/melee/melee-weapons.md          |   69 +-
 docs/specs/weapons/weapons.md                      |   38 +-
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
 67 files changed, 4209 insertions(+), 520 deletions(-)
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
    "code_hash": "25270f2f65a1543ed2849d70dfb3b948bf62a2529b9212fc1d7e7b3b7a7efe53",
    "dll": "build\\game\\generation-366\\game-g366.dll",
    "generation": 366,
    "revision": "5790ee8c1a4bad2e5572ee004c22514e42b5722bb9fd2904469b55d275b2ca35",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-366\\game-g366.dll",
    "generation": 366,
    "published_utc": "2026-11-19T16:52:49.000Z",
    "revision": "5790ee8c1a4bad2e5572ee004c22514e42b5722bb9fd2904469b55d275b2ca35",
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
