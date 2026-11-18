// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Recover crashed launch clients

# Task

- Task ID: session-20261118-102123
- Session ID: session-20261118-102123
- Owner: Codex
- Focus: dev-loop launch, process reconciliation, and crash artifact capture
- Summary: Capture dev-loop crash dumps and prevent dashboard launch from waiting on an exited or stale same-checkout MiMITA process.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-18T10:21:26+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Final commit: 43b89938b8aa29e3bffab982a544972f8dad7968

# Pre-existing changes

- The checkout already had extensive staged/unstaged work before this task,
  including pre-existing edits inside `devscripts/dev-loop.py`,
  `src/debug/crash-handler.cpp`, `src/main.cpp`, and both specs edited here.
  This task changed only the sections described below; all other existing
  hunks and untracked files were preserved. See the captured git state below.

# Requested behavior

- On the dashboard launch button, detect and replace a stale or crashed
  same-checkout `mimita.exe`, so an exited client does not consume the entire
  startup wait. Save crash reports and minidumps in the run-local project
  directory and show their paths in run journal events/status.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dev-loop.md` sections 3
  and 3.1; `docs/specs/debug-logging/debug-logging.md`, complete direct-file
  and process-output sink inventory.
- Exact requirements: process replacement is controlled and logged; crash
  artifacts for agent-run processes belong under `.dev/crashes/<run-id>/` and
  are referenced from `events.jsonl`.
- Why the change follows the specification: explicit launch button replacement
  is now documented as user-authorized for verified executable identities
  inside this checkout only; failed scans/cleanup abort launch. Crash events
  record artifact paths without reading LocalAppData.
- Conflicts or decisions: the existing unowned-process regression correctly
  describes the previous broad reaper. This user request explicitly authorizes
  same-checkout replacement from this launch button; cross-checkout and
  unverifiable identities remain excluded.

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
  `devscripts/dev-loop.py`: `_crash_artifacts`, `_record_child_exit`,
  `launch_latest`, `wait_for_logger_started`, `wait_for_journal_event`,
  `wait_for_hot_client_gate`, `reap_stray_mimita_processes`,
  `maintain_process`; `src/debug/crash-handler.cpp`: `buildCrashDir`,
  `writeMinidump`, `emitCrashReport`, `installCrashHandler`;
  `src/main.cpp`: `main` crash-dialog policy; dev-loop and debug-logging spec
  sections documented above.
- Old content or behavior: startup waits could continue after a child exited;
  process cleanup used non-strict broad path discovery and did not verify
  termination; agent-run crash output used the standalone default location;
  terminate reports did not request a minidump.
- New content or behavior: child exits are detected during readiness waits and
  recorded with crash artifact paths; explicit launch replacement strictly
  scans same-checkout MiMITA executables, rechecks PID/start/path identity,
  stops verified matches, and confirms exit before launching. Run-managed
  reports and dumps use `.dev/crashes/<run-id>/`; run-managed crash dialogs are
  suppressed so a modal cannot leave a crashed client looking alive.
- Reason: make a button press recover promptly from an old crashed client and
  preserve evidence for diagnosis.
- Why unrelated behavior is preserved: standalone crash directory/dialog
  behavior remains; process paths are constrained to this checkout and
  unverifiable scans fail closed; no executable files are deleted.

## Exact session-touched files

- `devscripts/dev-loop.py` — exit detection, crash path correlation, and safe
  same-checkout process reconciliation.
- `src/debug/crash-handler.cpp` — configured crash directory and dump writing
  for terminate reports.
- `src/main.cpp` — suppress modal dialogs for run-managed game processes.
- `docs/specs/dev-loop/dev-loop.md` — explicit launch-button replacement scope.
- `docs/specs/debug-logging/debug-logging.md` — update crash artifact sink
  disposition to match implementation.
- This changelog.

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
| `docs/specs/debug-logging/debug-logging.md` |  |  |  | git checkout -- docs/specs/debug-logging/debug-logging.md |
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
| `src/debug/crash-handler.cpp` |  |  |  | git checkout -- src/debug/crash-handler.cpp |
| `src/effects/effect-part-render.cpp` |  |  |  | git checkout -- src/effects/effect-part-render.cpp |
| `src/effects/effect-part.cpp` |  |  |  | git checkout -- src/effects/effect-part.cpp |
| `src/effects/effect-part.h` |  |  |  | git checkout -- src/effects/effect-part.h |
| `src/game/game-input-module.cpp` |  |  |  | git checkout -- src/game/game-input-module.cpp |
| `src/gui/hud/player-nameplates.cpp` |  |  |  | git checkout -- src/gui/hud/player-nameplates.cpp |
| `src/gui/hud/player-nameplates.h` |  |  |  | git checkout -- src/gui/hud/player-nameplates.h |
| `src/gui/ui-text-input.cpp` |  |  |  | git checkout -- src/gui/ui-text-input.cpp |
| `src/gui/ui-text-input.h` |  |  |  | git checkout -- src/gui/ui-text-input.h |
| `src/main.cpp` |  |  |  | git checkout -- src/main.cpp |
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

- Focused skill paths and results: `docs/skills/logging-checker-v1.md`,
  `docs/skills/efficiency-checker-v1.md`, and
  `docs/skills/spec-behavior-review-v1.md` were reviewed; no new unbounded
  polling, unmanaged log sink, or parallel state owner was added.
- Tests and exact commands: no test/build command run, per checkout build
  authority. `git diff --check -- <session-touched files>` completed with no
  whitespace errors; Git emitted only the expected LF-to-CRLF warning for the
  already mixed-line-ending dev-loop spec.
- Build status: no build requested or waited for. Changelog capture reported
  the existing dev-loop build status as failed/stale and latest successful
  build ID `2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5`;
  it predates and does not include these edits.
- One permitted `python devscripts/dev-loop.py --status` snapshot at
  `2026-11-18T10:21Z` still reported that same failed/stale build and an
  existing running session `20261118_100534` (server PID 10116, client PID
  31920). The launch button was not pressed during this code-edit session, so
  those old processes were left untouched.
- Runtime or hot-reload evidence: not verified with the updated executable.
  No claims are made that a fresh button launch or crash dump has been observed.
- Output files: runtime crash artifacts, when generated by a later run, are
  expected in `.dev/crashes/<run-id>/` and referenced from the run's
  `events.jsonl`.

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
- Still unverified: rebuild through the root dev-loop; launch after leaving a
  same-checkout MiMITA client alive; observe it exit before replacement
  startup; and induce/observe a crash dump plus the matching journal event.

# Agent handoff context

- Owner: Codex
- Focus: dev-loop launch, process reconciliation, and crash artifact capture
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "failed", "latest_build_stale": true, "latest_successful_build": "2026-11-18T10:05:30.909128Z", "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "eac97971d34d03cbda79c4740797bb34485e023b0963c6bd0ad4726fb83be5c9", "dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-344\\game-g344.dll", "generation": 344, "published_utc": "2026-11-18T09:50:03.870Z", "revision": "dd341cdbeef828dae16c9804593c0b730094baf1a29401b6d3652b9145ec1286", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 21.664, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-18T10:16:50.569Z"}}
- Generation: 344
- Artifact: build\game\generation-344\game-g344.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_100534\client-01\events.jsonl
- Session ID: session-20261118-102123

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
 M docs/specs/debug-logging/debug-logging.md
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
 M src/debug/crash-handler.cpp
 M src/effects/effect-part-render.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/game/game-input-module.cpp
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/gui/ui-text-input.cpp
 M src/gui/ui-text-input.h
 M src/main.cpp
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
config/accounts/default.json                       |  10 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   4 +-
 config/gamemodes/funcounterstrike.json             |   8 +-
 config/healthbar.json                              |   2 -
 config/maps/funworld3.json                         |  85 ++++++-
 config/movement.json                               |   1 +
 config/movement/movement-source.json               |  10 +-
 config/weaponcollisions.json                       |   2 +-
 config/weapons/rocket_launcher/config.json         |  12 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |   1 +
 devscripts/dev-loop.py                             | 272 +++++++++++++++++----
 devscripts/dev-profiles/npc-navigation.json        |   2 +-
 devscripts/dev_loop/game_build.py                  |  32 ++-
 devscripts/dev_loop/gui.py                         | 122 ++++++++-
 devscripts/game_build_worker.py                    |  58 ++++-
 docs/regressions/collisions/README.md              |   4 +-
 .../physical-objects-collision-REG-2026-10-01.md   |  69 ++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |   9 +
 docs/regressions/dev-loop/README.md                |  16 +-
 docs/regressions/gameplay/README.md                |  24 +-
 docs/regressions/networking/README.md              |   2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    |  30 ++-
 docs/specs/debug-logging/debug-logging.md          |   2 +-
 docs/specs/dev-loop/build-time-observability.md    |  11 +
 docs/specs/dev-loop/dashboard.md                   |  31 ++-
 docs/specs/dev-loop/dev-loop.md                    |  49 +++-
 docs/specs/entity-editor/entity-editor.md          |  13 +-
 docs/specs/gamemodes/funcounterstrike.md           |   3 +
 docs/specs/open-gameplay-issues.md                 |  14 +-
 docs/specs/performance/performance.md              |  33 +++
 src/combat/explosion-fx.cpp                        |  30 ++-
 src/combat/explosion-fx.h                          |   8 +-
 src/combat/weapon-rocket-launcher.cpp              | 137 ++++++++++-
 src/debug/crash-handler.cpp                        |  67 +++--
 src/effects/effect-part-render.cpp                 |  56 ++++-
 src/effects/effect-part.cpp                        |   7 +-
 src/effects/effect-part.h                          |   4 +-
 src/game/game-input-module.cpp                     |  29 ++-
 src/gui/hud/player-nameplates.cpp                  |   8 +
 src/gui/hud/player-nameplates.h                    |   3 +-
 src/gui/ui-text-input.cpp                          |  84 ++++++-
 src/gui/ui-text-input.h                            |  11 +
 src/main.cpp                                       |   4 +-
 src/mapeditor/map-editor.cpp                       |  61 ++++-
 src/network/multiplayer-context.h                  |   4 +
 src/network/multiplayer-projectiles.cpp            |  19 ++
 src/npc/npc-nav-graph.cpp                          |  32 ++-
 src/npc/npc-navigator.cpp                          |  21 +-
 src/npc/npc.cpp                                    |  54 +++-
 51 files changed, 1355 insertions(+), 219 deletions(-)
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
M	docs/specs/debug-logging/debug-logging.md
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
M	src/debug/crash-handler.cpp
M	src/effects/effect-part-render.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/game/game-input-module.cpp
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/gui/ui-text-input.cpp
M	src/gui/ui-text-input.h
M	src/main.cpp
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
    "build_status": "failed",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-18T10:05:30.909128Z",
    "latest_successful_build_id": "2026-11-18T10:05:30.909128Z:50972d79618bc0b72220dc0afba59b5e6890c7a0fb878a8245ce59d70c5d08c5",
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
    "compiled": 0,
    "elapsed_s": 21.664,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-18T10:16:50.569Z"
  }
}
```
