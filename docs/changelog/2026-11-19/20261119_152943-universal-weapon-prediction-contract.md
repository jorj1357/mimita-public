// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Universal weapon prediction contract

# Task

- Task ID: session-20261119-universal-weapon-prediction-spec
- Session ID: session-20261119-universal-weapon-prediction-spec
- Owner: Codex
- Focus: documentation
- Summary: Aligned weapon, networking, and logging specifications around immediate client prediction, 99.99% hit agreement, mapped-time cooldown queuing, and mismatch evidence.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-19T15:29:39+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Pre-existing changes

The shared checkout contained extensive unrelated edits before this session. The full status snapshot is preserved under Automatic evidence. This session modified only the four specification files listed below and created this changelog. No existing source, config, build, process, or runtime state was changed.

# Requested behavior

- Every weapon family predicts its locally visible action and plausible contact immediately.
- Aim for at least 99.99% exact target and damage agreement for eligible predicted hits across the full supported RTT and actor/weapon matrix; investigate every mismatch to its first divergence.
- Judge cooldown at the mapped firing tick. Queue legal delayed requests in order when server processing is behind; do not grant extra shots or reject a legal shot solely because it was processed late.
- Predict melee contacts and projectile/explosion outcomes, then adopt or reconcile against the server result without duplicate damage/effects.
- Record the correlated prediction-to-authority event chain and reasons in each client's and server's events.jsonl.

# Specification alignment

- Current specification paths: docs/specs/weapons/weapons.md; docs/specs/networking/networking.md; docs/specs/networking/contact-authority.md; docs/specs/debug-logging/debug-logging.md.
- Exact requirements: one shared contact/damage path, client-first prediction, bounded server plausibility validation, historical contact-time mapping, mapped-time cooldown checks, 99.99% predicted-versus-authoritative damage agreement, and correlated mismatch diagnostics.
- Why the change follows the specification: the authoritative contract now states the requested behavior for every weapon family and actor pairing, keeps shared health/death/score under server ownership, and treats the 99.99% number as a measured target rather than a guarantee.
- Conflicts or decisions: the earlier melee-only server-confirmed feedback bullet, server-determines-result wording for projectiles/melee, and 99% high-RTT target were superseded. Current implementation remains incomplete: melee contact claims are gated by melee_contact.enabled in src/combat/melee-client.cpp, and the generic batch path in src/network/server-packet-handlers.cpp is specialized to Swordsword physical-contact weapons. src/network/server-attack.cpp checks cooldown against the processing tick. This session records intended behavior; it does not claim these code paths were fixed.

# Exact implementation changes

## docs/specs/networking/contact-authority.md

- Lines/functions/headings: Client-first authority policy; Universal weapon prediction and match target; Damage families / Projectile and Melee; Accuracy targets; Late contact policy.
- Old content or behavior: hitscan-specific client-first authority; melee damage feedback server-confirmed only; exclusions could omit a target-life mismatch.
- New content or behavior: all weapon families and actor pairings predict; local hit/damage/impulse feedback is provisional; server validates/adopts/corrects; every post-prediction rejection remains in the accuracy denominator; mismatch fields and correlation chain are required.
- Reason: make the desired trust/prediction model explicit without moving authoritative shared-state writes to clients.
- Why unrelated behavior is preserved: this is a specification-only change; damage sink, geometry ownership, and damage formulas were not edited.

## docs/specs/networking/networking.md

- Lines/functions/headings: sections 11–12 Generic request architecture/AttackRequest; sections 45.1–45.2; Weapon request ordering under server load; Melee prediction-first contact; Weapon prediction accuracy target.
- Old content or behavior: client never sent a candidate target/explosion result; cooldown implied server-current-tick checking; melee was described as an exception; RTT >= 100 ms had a lower 99% target.
- New content or behavior: bounded client claims are explicitly candidates rather than authority; mapped action tick controls cooldown; legal requests queue/deduplicate under backlog; fixed-tick simulation remains authoritative; all weapon families use universal prediction; 99.99% applies across supported RTT.
- Reason: separate packet/server processing delay from configured weapon cadence.
- Why unrelated behavior is preserved: no packet schema or runtime implementation was changed.

## docs/specs/weapons/weapons.md

- Lines/functions/headings: AttackRequest and Full grenade launcher flow, phases 4–5.
- Old content or behavior: projectile/melee requests sent intent only and server determined the result; cooldown compared to the current processing tick.
- New content or behavior: all weapon requests carry action time/sequence and bounded prediction claims; server adopts plausible historical results; next allowed fire tick derives from validated action tick.
- Reason: align weapon flow with prediction and queue semantics.
- Why unrelated behavior is preserved: examples remain conceptual; no ABI or code was changed.

## docs/specs/debug-logging/debug-logging.md

- Lines/functions/headings: System-specific logging / Weapons; Weapon prediction and mismatch evidence.
- Old content or behavior: general weapon logs did not explicitly require mapped/receive/process timing or queue and explosion prediction reconciliation fields.
- New content or behavior: specify IDs, ticks, queue/server Hz/RTT context, result/reason, first-divergence event, and explosion reconciliation evidence.
- Reason: make misses traceable from client prediction through server damage and replication.
- Why unrelated behavior is preserved: retained pre-existing camera-control documentation in this already modified file.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| docs/specs/networking/contact-authority.md | Partial client-first and melee-specific rules | Unified prediction, accuracy, and mismatch contract | Establish the desired shared behavior | Restore only this file's session diff |
| docs/specs/networking/networking.md | Conflicting late-processing, cooldown, melee, and RTT rules | Mapped-time queue and universal prediction contract | Clarify server-load behavior | Restore only this file's session diff |
| docs/specs/weapons/weapons.md | Projectile/melee server-first wording and process-tick cooldown | Predicted claims and validated-action-tick cooldown | Align generic attack flow | Restore only this file's session diff |
| docs/specs/debug-logging/debug-logging.md | General weapon diagnostics | Correlated prediction/mismatch evidence contract | Require actionable journals | Restore only this file's session diff; preserve pre-existing camera edits |
| docs/changelog/2026-11-19/20261119_152943-universal-weapon-prediction-contract.md | Absent | Added this session record | Required session evidence | Remove only if user requests reversal |

# Contract / ABI changes

- Contract: documentation contract only; no packet, ABI, or runtime change.
- Version bump: none.
- Compatibility: implementation must migrate every weapon family before claiming universal behavior.
- Reverse: restore the four specification diffs together to avoid reintroducing contradictions.

# State / schema changes

- None. No JSONL schema or persisted state changed.

# Deleted paths

- None.

# Diagnostics

- Owner/category: specified for the existing weapon/contact/network StructuredLogger owners; no runtime logger was edited.
- Input: attack/request, contact, projectile, and explosion IDs; mapped action/contact tick; receive/process tick; generations; weapon revision; prediction and authority outcomes; queue delay/depth, server Hz, RTT/jitter when timing is implicated.
- Decision: accepted, rejected, adopted, or corrected with stable reason and first divergent stage.
- Output: correlated client/server events.jsonl records through damage application, replication, and reconciliation.
- Failure or rejection reason: distinguish genuine cooldown from backlog/history expiry and geometry, team, generation, duplicate, or other validation causes.
- Rate limiting: log state transitions and request decisions, not every tick.

# Validation

- Focused skill paths and results: docs/skills/spec-writing-v1.md; docs/skills/spec-behavior-review-v1.md; docs/skills/documentation-checker-v1.md. Reviewed the old/new policy contradictions and routed links.
- Tests and exact commands: no tests run (documentation-only change). Ran rg contradiction/link searches and git diff --check for the four specification files; no whitespace errors. The TODO scan found no actionable documentation TODO matching the required patterns.
- Build status: no build requested or run. Any build identity in the generated automatic snapshot is ambient checkout status, not evidence from this session.
- Runtime or hot-reload evidence: none; this session changes desired behavior only.
- Output files: the four specifications and this changelog.

# Measured evidence

- Before values: existing spec included a 99% target for RTT >= 100 ms and contradictory melee/server-first wording.
- After values: one 99.99% target across supported RTT and one client-prediction/server-referee contract; runtime behavior not measured.
- Timestamps: changelog timestamp is UTC; user-facing date is 2026-11-19 America/New_York.
- Tick/frame/network measurements: none collected.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: no new runtime failure was reproduced; this spec change documents the intended response to existing client/server disagreement reports.
- Related regression paths: docs/regressions/networking/client-prediction-server-REG-2026-10-29.md; docs/regressions/networking/README.md.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed; all-family prediction and 99.99% agreement still require implementation and real runtime evidence.
- Multiplayer review: not performed; player/player and the full actor/weapon/RTT matrix remain outstanding.
- Still unverified: code conformance, queue behavior under low server Hz, explosion adoption/correction, melee prediction for every melee type, and measured accuracy.

# Agent handoff context

- Owner: Codex
- Focus: documentation contract complete; implementation remains outstanding.
- Files to check: the four changed specifications, src/combat/melee-client.cpp, src/network/server-packet-handlers.cpp, src/network/server-attack.cpp, and the client-prediction/server-authority regression.
- What another agent should not redo: the spec now defines one universal prediction/referee policy and mapped-tick cooldown semantics; it is not evidence that code/runtime already comply.
- Next exact step: serialize the shared-source investigation; trace the first divergence for current melee, cooldown, and explosion reports, then migrate all weapon families to the generic prediction/contact/result chain and validate through the root dev-loop journals.
# Automatic evidence

The generated status/diff snapshot below covers the entire shared checkout,
including unrelated pre-existing work. It is not the changed-file list for
this session; the focused session file list is above. The captured build and
journal identities are ambient checkout state, not task validation. No build
or runtime scenario was run for this documentation-only change.

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 214849f07f9a4bc045d1461b1dbaf463a2084a85 / 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-19T15:17:05.094869Z", "latest_successful_build_id": "2026-11-19T15:17:05.094869Z:731b9a39e924b00277ab2638178caa0cd5dc514edd4e57112728e874ecbf4a92", "latest_successful_source_generation": 33}, "game_current": {"build_mode": "debug", "code_hash": "17b92c099667fe9efe87de6a027279dcef93a86f2ddceca9172f1c9a48f8a44b", "dll": "build\\game\\generation-355\\game-g355.dll", "generation": 355, "revision": "b86b94f763bcbbd7158c0758dee7218f6cdf6c3c9420536bb826db70acfbd3ef", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-355\\game-g355.dll", "generation": 355, "published_utc": "2026-11-19T15:13:10.246Z", "revision": "b86b94f763bcbbd7158c0758dee7218f6cdf6c3c9420536bb826db70acfbd3ef", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 31.758, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-19T15:17:02.801Z"}}
- Generation: 355
- Artifact: build\game\generation-355\game-g355.dll
- Journal path: C:\mimita-v9\logs\2026-11-19\20261119_152651\client-01\events.jsonl
- Session ID: session-20261119-universal-weapon-prediction-spec

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
 M config/audio/music-settings.json
 M config/gui/main-menu.json
 M config/gui/map-editor.json
 M config/hot-reload/packet-ledger.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/game_build_worker.py
 M devscripts/source_manifest.py
 M docs/architecture/live-development/incremental-build-and-header-fanout.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/hotreload/README.md
 M docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
 M docs/specs/animations/animations.md
 M docs/specs/camera/camera.md
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
 M docs/specs/weapons/weapons.md
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
 M src/mapeditor/map-editor-commands.cpp
 M src/mapeditor/map-editor-projects.cpp
 M src/mapeditor/map-editor-projects.h
 M src/mapeditor/map-editor.cpp
 M src/mapeditor/map-editor.h
 M src/network/server-gamemode.cpp
 M src/network/server-gamemode.h
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
?? config/gui/create-menu.json
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
?? docs/handoffs/20261119_014155-full_creator_map_monster_mode_implementation.md
?? docs/handoffs/20261119_144118-juggernaut-60hz-runtime-handoff.md
?? docs/handoffs/dll-build-one-second-investigation.md
?? docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md
?? docs/regressions/hotreload/mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md
```

## Captured diff stat

```text
AGENTS.md                                          |   10 +
 config/accounts/default.json                       |    6 +-
 config/analytics.json                              |    4 +-
 config/audio/music-settings.json                   |    2 +-
 config/gui/main-menu.json                          |    8 +
 config/gui/map-editor.json                         |   20 +
 config/hot-reload/packet-ledger.json               |   40 +-
 devscripts/blender-ai                              |    0
 devscripts/dev-loop.py                             |   68 +-
 devscripts/dev_loop/game_build.py                  |  204 +++-
 devscripts/dev_loop/gui.py                         |  207 +++-
 devscripts/game_build_worker.py                    | 1163 ++++++++++++++++----
 devscripts/source_manifest.py                      |   86 +-
 .../incremental-build-and-header-fanout.md         |   27 +
 docs/regressions/dev-loop/README.md                |   13 +-
 docs/regressions/hotreload/README.md               |   10 +-
 .../server-tick-budget-REG-2026-11-03.md           |   23 +
 docs/specs/animations/animations.md                |   14 +
 docs/specs/camera/camera.md                        |   27 +
 docs/specs/creators/monster-authoring.md           |    4 +-
 docs/specs/debug-logging/canonical-jsonl.md        |   12 +
 docs/specs/debug-logging/debug-logging.md          |   34 +
 docs/specs/dev-loop/build-time-observability.md    |  197 +++-
 docs/specs/dev-loop/dev-loop.md                    |   23 +-
 docs/specs/gamemodes/zombie-attack.md              |   30 +
 docs/specs/hotreload/hotreload.md                  |   23 +
 docs/specs/movement/movement.md                    |    2 +
 docs/specs/networking/contact-authority.md         |   78 +-
 docs/specs/networking/networking.md                |   68 +-
 docs/specs/weapons/weapons.md                      |   38 +-
 src/engine/engine-tick-camera.cpp                  |   61 +
 src/engine/engine-tick-state.cpp                   |   63 ++
 src/entities/player-animation.cpp                  |   63 +-
 src/game/server-command-dispatch-module.cpp        |   39 +-
 src/game/server-runtime-module.cpp                 |   62 ++
 src/gui/gui-main.cpp                               |  122 ++
 src/gui/gui-main.h                                 |   20 +
 src/gui/menus/main-menu.cpp                        |    3 +-
 src/gui/menus/main-menu.h                          |    1 +
 src/input/input-poll.cpp                           |   22 +-
 src/mapeditor/map-editor-commands.cpp              |  121 ++
 src/mapeditor/map-editor-projects.cpp              |  218 ++++
 src/mapeditor/map-editor-projects.h                |   44 +
 src/mapeditor/map-editor.cpp                       |  259 ++++-
 src/mapeditor/map-editor.h                         |   20 +
 src/network/server-gamemode.cpp                    |   42 +
 src/network/server-gamemode.h                      |    6 +
 src/npc/npc-nav-graph.cpp                          |   65 +-
 src/npc/npc-nav-graph.h                            |    3 +-
 src/npc/npc-navigator.cpp                          |    4 +-
 src/physics/movement/physics-collision-body.cpp    |    8 +-
 .../movement/physics-collision-glb-main.cpp        |   81 +-
 .../movement/physics-collision-glb-sweep-slide.cpp |   29 +-
 src/physics/movement/physics-collision-shared.h    |    1 +
 src/physics/movement/physics-scratch.h             |    5 +
 src/world/world-gltf-loader.cpp                    |    7 +-
 src/world/world-gltf-loader.h                      |    4 +-
 57 files changed, 3352 insertions(+), 462 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gui/main-menu.json
M	config/gui/map-editor.json
M	config/hot-reload/packet-ledger.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/game_build_worker.py
M	devscripts/source_manifest.py
M	docs/architecture/live-development/incremental-build-and-header-fanout.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/hotreload/README.md
M	docs/regressions/performance/server-tick-budget-REG-2026-11-03.md
M	docs/specs/animations/animations.md
M	docs/specs/camera/camera.md
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
M	docs/specs/weapons/weapons.md
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
M	src/mapeditor/map-editor-commands.cpp
M	src/mapeditor/map-editor-projects.cpp
M	src/mapeditor/map-editor-projects.h
M	src/mapeditor/map-editor.cpp
M	src/mapeditor/map-editor.h
M	src/network/server-gamemode.cpp
M	src/network/server-gamemode.h
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
    "code_hash": "17b92c099667fe9efe87de6a027279dcef93a86f2ddceca9172f1c9a48f8a44b",
    "dll": "build\\game\\generation-355\\game-g355.dll",
    "generation": 355,
    "revision": "b86b94f763bcbbd7158c0758dee7218f6cdf6c3c9420536bb826db70acfbd3ef",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-355\\game-g355.dll",
    "generation": 355,
    "published_utc": "2026-11-19T15:13:10.246Z",
    "revision": "b86b94f763bcbbd7158c0758dee7218f6cdf6c3c9420536bb826db70acfbd3ef",
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
