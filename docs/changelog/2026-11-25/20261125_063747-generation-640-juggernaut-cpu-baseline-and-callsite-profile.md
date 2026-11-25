// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Generation 640 Juggernaut CPU baseline and callsite profile

# Task

- Task ID: session-46b4d9e96e28
- Session ID: session-46b4d9e96e28
- Owner: root-mode3-latency
- Focus: edit-to-live-cpu-baseline
- Summary: Captured a bounded host/PID CPU and RAM baseline alongside the existing in-game callsite profiler on a pinned generation-640 Juggernaut Horde run. The run had 67 NPCs and one player; server tick averages were near the 60 Hz budget, while every client frame exceeded the 4.167 ms target. Server callsite estimates ranked updateOneNpc and doCollisions highest; client estimates were sparse. Evidence is linked from issue-ed3ac775283d and issue-136d4553ff92. No source change, build, or runtime restart was made.
- Status: baseline-captured
- Date, time, timezone: 2026-11-25T06:37:47+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 8fa06602be343fdaacacc1c07156a8595cc95035
- Final commit: 8fa06602be343fdaacacc1c07156a8595cc95035

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
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` |  |  |  | git checkout -- docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md |
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` |  |  |  | git checkout -- docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md |
| `docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md` |  |  |  | git checkout -- docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md` |  |  |  | git checkout -- docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/physics/physical-entity.cpp` |  |  |  | git checkout -- src/physics/physical-entity.cpp |
| `src/sim/simulate-tick.cpp` |  |  |  | git checkout -- src/sim/simulate-tick.cpp |
| `src/vehicles/vehicle-system.cpp` |  |  |  | git checkout -- src/vehicles/vehicle-system.cpp |
| `config/analytics.json` |  |  |  | Remove-Item -LiteralPath 'config/analytics.json' |
| `docs/changelog/2026-11-25/20261125_0515-rl-code-pass-4-prediction-120hz-events.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0515-rl-code-pass-4-prediction-120hz-events.md' |
| `docs/changelog/2026-11-25/20261125_0530-matched-bloodtest-resource-baseline.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0530-matched-bloodtest-resource-baseline.md' |
| `docs/changelog/2026-11-25/20261125_0536-independent-issue-review-issue-be1adb51b6ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0536-independent-issue-review-issue-be1adb51b6ac.md' |
| `docs/changelog/2026-11-25/20261125_0601-independent-issue-review-issue-62ab3daa615d.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0601-independent-issue-review-issue-62ab3daa615d.md' |
| `docs/changelog/2026-11-25/20261125_060459-bloodtest-aiming-ak-damage-npc-body-holes-organs-and-clean-240-fps.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_060459-bloodtest-aiming-ak-damage-npc-body-holes-organs-and-clean-240-fps.md' |
| `docs/changelog/2026-11-25/20261125_0607-independent-build-wait-review-issue-228f1f95a42d.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0607-independent-build-wait-review-issue-228f1f95a42d.md' |
| `docs/changelog/2026-11-25/20261125_0610-cpu-optimization-pass-prerequisites.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0610-cpu-optimization-pass-prerequisites.md' |
| `docs/changelog/2026-11-25/20261125_0611-independent-build-wait-review-issue-9c1fc3e3e668.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0611-independent-build-wait-review-issue-9c1fc3e3e668.md' |
| `docs/changelog/2026-11-25/20261125_0619-independent-build-wait-review-issue-8af9b97c9a2a.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0619-independent-build-wait-review-issue-8af9b97c9a2a.md' |
| `docs/changelog/2026-11-25/20261125_gen635-compile-cost-attribution.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_gen635-compile-cost-attribution.md' |
| `docs/changelog/2026-11-25/20261125_gen638-one-source-build-latency-and-live-activation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_gen638-one-source-build-latency-and-live-activation.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-228f1f95a42d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-228f1f95a42d.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md' |
| `docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-9c1fc3e3e668.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-9c1fc3e3e668.md' |

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

- Owner: root-mode3-latency
- Focus: edit-to-live-cpu-baseline
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 8fa06602be343fdaacacc1c07156a8595cc95035 / 8fa06602be343fdaacacc1c07156a8595cc95035
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-25T01:21:28.987918Z", "latest_successful_build_id": "2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b", "latest_successful_source_generation": 58}, "game_current": {"build_mode": "debug", "code_hash": "2ebef9fa9e03ec90e707f59f2e2ffe35d0ad8a1e83a026b3ac52111dff225eab", "dll": "build\\game\\generation-640\\game-g640.dll", "generation": 640, "revision": "308b20ce51a80eb391655fb4c842cd33ccf692ce36149766fddc2a63246ebe8e", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-640\\game-g640.dll", "generation": 640, "published_utc": "2026-11-25T05:55:04.947Z", "revision": "308b20ce51a80eb391655fb4c842cd33ccf692ce36149766fddc2a63246ebe8e", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 13.827, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-25T01:21:26.486Z"}}
- Generation: 640
- Artifact: build\game\generation-640\game-g640.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_061355\client-01\events.jsonl
- Session ID: session-46b4d9e96e28

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
M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
 M docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
 M docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
 M src/network/multiplayer-projectiles.cpp
 M src/network/server-projectiles.cpp
 M src/physics/physical-entity.cpp
 M src/sim/simulate-tick.cpp
 M src/vehicles/vehicle-system.cpp
?? config/analytics.json
?? docs/changelog/2026-11-25/20261125_0515-rl-code-pass-4-prediction-120hz-events.md
?? docs/changelog/2026-11-25/20261125_0530-matched-bloodtest-resource-baseline.md
?? docs/changelog/2026-11-25/20261125_0536-independent-issue-review-issue-be1adb51b6ac.md
?? docs/changelog/2026-11-25/20261125_0601-independent-issue-review-issue-62ab3daa615d.md
?? docs/changelog/2026-11-25/20261125_060459-bloodtest-aiming-ak-damage-npc-body-holes-organs-and-clean-240-fps.md
?? docs/changelog/2026-11-25/20261125_0607-independent-build-wait-review-issue-228f1f95a42d.md
?? docs/changelog/2026-11-25/20261125_0610-cpu-optimization-pass-prerequisites.md
?? docs/changelog/2026-11-25/20261125_0611-independent-build-wait-review-issue-9c1fc3e3e668.md
?? docs/changelog/2026-11-25/20261125_0619-independent-build-wait-review-issue-8af9b97c9a2a.md
?? docs/changelog/2026-11-25/20261125_gen635-compile-cost-attribution.md
?? docs/changelog/2026-11-25/20261125_gen638-one-source-build-latency-and-live-activation.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-228f1f95a42d.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-9c1fc3e3e668.md
```

## Captured diff stat

```text
.../gamemodes/masterlist/gamemode-masterlist.json  |   6 +
 config/weapons/bloodtest_ak47/config.json          |   8 +-
 devscripts/blender-ai                              |   0
 docs/issues/README.md                              |  25 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  52 +++
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  37 ++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   6 +
 ...time-journal-events-wait--issue-6e2d7b91e7d9.md |  12 +
 ...nt-choppiness-against-the-issue-16500baeb0cc.md |   6 +
 ...n-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |   6 +
 ...e-dll-build-at-game-build-issue-9aeec1fbae9d.md |  38 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  18 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  14 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  12 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 154 +++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 494 +++++++++++++++++++++
 ...ts-and-diagnose-mimita-cp-issue-dc2f873290a7.md |  48 ++
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   6 +
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |  20 +
 src/network/multiplayer-projectiles.cpp            |  67 ++-
 src/network/server-projectiles.cpp                 |   8 +-
 src/physics/physical-entity.cpp                    |  26 +-
 src/sim/simulate-tick.cpp                          |  21 +-
 src/vehicles/vehicle-system.cpp                    |  35 +-
 25 files changed, 1105 insertions(+), 26 deletions(-)
```

## Captured changed paths

```text
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
M	docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
M	docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
M	src/network/multiplayer-projectiles.cpp
M	src/network/server-projectiles.cpp
M	src/physics/physical-entity.cpp
M	src/sim/simulate-tick.cpp
M	src/vehicles/vehicle-system.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-25T01:21:28.987918Z",
    "latest_successful_build_id": "2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b",
    "latest_successful_source_generation": 58
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "2ebef9fa9e03ec90e707f59f2e2ffe35d0ad8a1e83a026b3ac52111dff225eab",
    "dll": "build\\game\\generation-640\\game-g640.dll",
    "generation": 640,
    "revision": "308b20ce51a80eb391655fb4c842cd33ccf692ce36149766fddc2a63246ebe8e",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-640\\game-g640.dll",
    "generation": 640,
    "published_utc": "2026-11-25T05:55:04.947Z",
    "revision": "308b20ce51a80eb391655fb4c842cd33ccf692ce36149766fddc2a63246ebe8e",
    "status": "ready"
  },
  "progress": {
    "compiled": 2,
    "elapsed_s": 13.827,
    "phase": "build_complete",
    "result": "success",
    "skipped": 85,
    "updated_utc": "2026-11-25T01:21:26.486Z"
  }
}
```
