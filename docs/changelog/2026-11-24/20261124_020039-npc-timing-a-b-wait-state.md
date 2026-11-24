// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# NPC timing A/B wait state

# Task

- Task ID: work-48758c844231
- Session ID: work-48758c844231
- Owner: resource-profiler
- Focus: cpu
- Summary: Revalidated the user-requested CPU iteration loop against live state. Latest matched source evidence: run20261123_195307 reported updateOneNpc at 36.16% sampled server-process cycles (~2.76% whole-host CPU) and doCollisions at 8.61% (~0.65% host); these are estimates, not internal line attribution. Confirmed the current root runtime is Bloodtest with 0 NPC and multiple clients, and the root has an unrelated pending cold-only dev-launch-modes batch. Coordinated with the hot-reload latency owner and deferred the NPC source/build change to avoid disrupting their run or mixing build targets. Next action: matched same-process timing-on/off A/B on pinned 67-NPC Horde, with 60Hz/FPS/gameplay checks.
- Status: partial
- Date, time, timezone: 2026-11-24T02:00:25+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/maps/dust2cyberiav4.json` |  |  |  | git checkout -- config/maps/dust2cyberiav4.json |
| `config/tips.json` |  |  |  | git checkout -- config/tips.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-launch-modes.json` |  |  |  | git checkout -- devscripts/dev-launch-modes.json |
| `docs/features/npc-behavior.md` |  |  |  | git checkout -- docs/features/npc-behavior.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md` |  |  |  | git checkout -- docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md` |  |  |  | git checkout -- docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` |  |  |  | git checkout -- docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md |
| `docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md` |  |  |  | git checkout -- docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md |
| `src/config/movement-config.cpp` |  |  |  | git checkout -- src/config/movement-config.cpp |
| `src/engine/engine-tick-net.cpp` |  |  |  | git checkout -- src/engine/engine-tick-net.cpp |
| `src/engine/engine-tick-ui-game-hud.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui-game-hud.cpp |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `src/input/input-commands.cpp` |  |  |  | git checkout -- src/input/input-commands.cpp |
| `src/input/input-frame.h` |  |  |  | git checkout -- src/input/input-frame.h |
| `src/input/input-poll.cpp` |  |  |  | git checkout -- src/input/input-poll.cpp |
| `src/input/input-state.h` |  |  |  | git checkout -- src/input/input-state.h |
| `src/network/movement-validation.h` |  |  |  | git checkout -- src/network/movement-validation.h |
| `src/network/multiplayer-context.h` |  |  |  | git checkout -- src/network/multiplayer-context.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/multiplayer-reconcile.cpp` |  |  |  | git checkout -- src/network/multiplayer-reconcile.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/schema/packet-common.h` |  |  |  | git checkout -- src/network/schema/packet-common.h |
| `src/network/server-packets.cpp` |  |  |  | git checkout -- src/network/server-packets.cpp |
| `src/network/server-players.cpp` |  |  |  | git checkout -- src/network/server-players.cpp |
| `src/network/server.h` |  |  |  | git checkout -- src/network/server.h |
| `src/physics/movement/movement-conversion.cpp` |  |  |  | git checkout -- src/physics/movement/movement-conversion.cpp |
| `src/physics/movement/movement-step.cpp` |  |  |  | git checkout -- src/physics/movement/movement-step.cpp |
| `src/physics/movement/movement-step.h` |  |  |  | git checkout -- src/physics/movement/movement-step.h |
| `src/physics/movement/movement-types.h` |  |  |  | git checkout -- src/physics/movement/movement-types.h |
| `src/sim/simulate-tick.cpp` |  |  |  | git checkout -- src/sim/simulate-tick.cpp |
| `docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md' |
| `docs/changelog/2026-11-24/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/' |
| `docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md' |
| `docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md` |  |  |  | Remove-Item -LiteralPath 'docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md' |
| `docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md' |
| `docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md' |
| `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md' |

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

- Owner: resource-profiler
- Focus: cpu
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T01:44:42.533263Z", "latest_successful_build_id": "2026-11-24T01:44:42.533263Z:1327fa12ce01906a90e6a843b83fbaa2b0a24bbbe6ea2acee68507a714e6c7df", "latest_successful_source_generation": 49}, "game_current": {"build_mode": "debug", "code_hash": "9ef0a299edc21c7fae5f44dcecd66c18eb2b3e838a4cbef1fa11f0849a210f46", "dll": "build\\game\\generation-577\\game-g577.dll", "generation": 577, "revision": "4c7283b8d4f8f8014d86c21cd03ee12c132b1120375bad424ceb373c5b9ff612", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-577\\game-g577.dll", "generation": 577, "published_utc": "2026-11-24T01:06:33.148Z", "revision": "4c7283b8d4f8f8014d86c21cd03ee12c132b1120375bad424ceb373c5b9ff612", "status": "ready"}, "progress": {"compiled": 23, "elapsed_s": 149.681, "phase": "build_complete", "result": "success", "skipped": 64, "updated_utc": "2026-11-24T01:44:30.928Z"}}
- Generation: 577
- Artifact: build\game\generation-577\game-g577.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_015025\client-04\events.jsonl
- Session ID: work-48758c844231

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
 M config/gamemodes/bloodtest.json
 M config/hot-reload/packet-ledger.json
 M config/maps/dust2cyberiav4.json
 M config/tips.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M docs/features/npc-behavior.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
 M src/config/movement-config.cpp
 M src/engine/engine-tick-net.cpp
 M src/engine/engine-tick-ui-game-hud.cpp
 M src/entities/player.h
 M src/gui/gui-main.cpp
 M src/input/input-commands.cpp
 M src/input/input-frame.h
 M src/input/input-poll.cpp
 M src/input/input-state.h
 M src/network/movement-validation.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/network/multiplayer-reconcile.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/schema/packet-common.h
 M src/network/server-packets.cpp
 M src/network/server-players.cpp
 M src/network/server.h
 M src/physics/movement/movement-conversion.cpp
 M src/physics/movement/movement-step.cpp
 M src/physics/movement/movement-step.h
 M src/physics/movement/movement-types.h
 M src/sim/simulate-tick.cpp
?? docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md
?? docs/changelog/2026-11-24/
?? docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md
?? docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
?? docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md
?? docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   4 +-
 config/gamemodes/bloodtest.json                    |   6 +-
 config/hot-reload/packet-ledger.json               | 232 +++++++--------
 config/maps/dust2cyberiav4.json                    | 110 ++++++-
 config/tips.json                                   |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-launch-modes.json                   |   1 +
 docs/features/npc-behavior.md                      |  73 +++++
 docs/issues/README.md                              |  39 ++-
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  40 +++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  10 +
 ...ness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |   8 +
 ...et-exe-build-at-compiling-issue-00b133ae7e13.md |  37 +++
 ...budget-exe-build-at-setup-issue-723326f11472.md |   6 +
 ...ild-queue-at-build-pendin-issue-af958c3e10eb.md |  36 +++
 ...e-dll-build-at-game-build-issue-84ec2bf27598.md |  38 +++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   8 +
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 +++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  34 +++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  52 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md | 113 +++++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  33 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 331 +++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |  88 ++++++
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |   6 +
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |  12 +
 src/config/movement-config.cpp                     |  20 ++
 src/engine/engine-tick-net.cpp                     |   4 +
 src/engine/engine-tick-ui-game-hud.cpp             |  22 ++
 src/entities/player.h                              |   1 +
 src/gui/gui-main.cpp                               |   2 +-
 src/input/input-commands.cpp                       |   2 +
 src/input/input-frame.h                            |   1 +
 src/input/input-poll.cpp                           |   2 +
 src/input/input-state.h                            |   2 +
 src/network/movement-validation.h                  |   3 +-
 src/network/multiplayer-context.h                  |   1 +
 src/network/multiplayer-projectiles.cpp            |   5 +
 src/network/multiplayer-reconcile.cpp              |   1 +
 src/network/multiplayer-tick.cpp                   |   1 +
 src/network/schema/packet-common.h                 |   3 +
 src/network/server-packets.cpp                     |   4 +
 src/network/server-players.cpp                     |  28 +-
 src/network/server.h                               |   1 +
 src/physics/movement/movement-conversion.cpp       |   4 +
 src/physics/movement/movement-step.cpp             |  83 +++++-
 src/physics/movement/movement-step.h               |   8 +
 src/physics/movement/movement-types.h              |  27 ++
 src/sim/simulate-tick.cpp                          |  38 +++
 51 files changed, 1476 insertions(+), 164 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/hot-reload/packet-ledger.json
M	config/maps/dust2cyberiav4.json
M	config/tips.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	docs/features/npc-behavior.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
M	src/config/movement-config.cpp
M	src/engine/engine-tick-net.cpp
M	src/engine/engine-tick-ui-game-hud.cpp
M	src/entities/player.h
M	src/gui/gui-main.cpp
M	src/input/input-commands.cpp
M	src/input/input-frame.h
M	src/input/input-poll.cpp
M	src/input/input-state.h
M	src/network/movement-validation.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/network/multiplayer-reconcile.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/schema/packet-common.h
M	src/network/server-packets.cpp
M	src/network/server-players.cpp
M	src/network/server.h
M	src/physics/movement/movement-conversion.cpp
M	src/physics/movement/movement-step.cpp
M	src/physics/movement/movement-step.h
M	src/physics/movement/movement-types.h
M	src/sim/simulate-tick.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-24T01:44:42.533263Z",
    "latest_successful_build_id": "2026-11-24T01:44:42.533263Z:1327fa12ce01906a90e6a843b83fbaa2b0a24bbbe6ea2acee68507a714e6c7df",
    "latest_successful_source_generation": 49
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "9ef0a299edc21c7fae5f44dcecd66c18eb2b3e838a4cbef1fa11f0849a210f46",
    "dll": "build\\game\\generation-577\\game-g577.dll",
    "generation": 577,
    "revision": "4c7283b8d4f8f8014d86c21cd03ee12c132b1120375bad424ceb373c5b9ff612",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-577\\game-g577.dll",
    "generation": 577,
    "published_utc": "2026-11-24T01:06:33.148Z",
    "revision": "4c7283b8d4f8f8014d86c21cd03ee12c132b1120375bad424ceb373c5b9ff612",
    "status": "ready"
  },
  "progress": {
    "compiled": 23,
    "elapsed_s": 149.681,
    "phase": "build_complete",
    "result": "success",
    "skipped": 64,
    "updated_utc": "2026-11-24T01:44:30.928Z"
  }
}
```
