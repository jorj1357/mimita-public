// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest aiming, AK damage, NPC body holes, organs, and clean 240 FPS

# Task

- Task ID: session-20261125-060456
- Session ID: session-20261125-060456
- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: generations 632, 634, 635, 637, 638, 639 and 640 published. Runtime evidence: run 20261125_052838 (damage/hole/organ/kill/corpse spawn), 20261125_060235 (clean 240 FPS and server tick). Not proven: corpse.hit visual event, visual acceptance of the AK scale/arm pose, 60 Hz 99.99%. Issues: issue-2e7b0cd6f868 evidence, issue-16500baeb0cc clean FPS, issue-ffcc6b95dec6 NPC spikes.
- Summary: Fixed the Bloodtest physical-aim and damage path end to end. The AK viewmodel was 2.0x on a 2.475 m GLB (revolver GLB is 0.415 m), putting the muzzle 5-7 m from the player; viewmodel.scale is now 0.17 (0.42 m). The physical right arm now converges the gun at the actual camera-ray target distance instead of a fixed 100 m point. The server no longer multiplies authored damage curves by physical impact energy; the AK now applies damage 65 close, 55 at 50 m, 45 at 80 m+ with a 3.0x headshot multiplier. Confirmed NPC body-part holes now reach every client through the NPC damage packet bodyPart/punchThrough fields and Player::addPartDamageHole. Runtime proof on generation 637 run 20261125_052838: combat.hit.resolved NPC-1000 torso 8.89 m damage 65; client bloodtest.body.hole NPC-1000 torso hole_count 1/2 radius 0.14 through=true; bloodtest.blood-effect.applied damage 65 force 0.65 organ_chunks 1 flesh 2 mist 19; second hit killed and ragdoll.corpse.spawned. NPC-1001 and NPC-1002 also showed holes at 4.13 m and 2.99 m. Clean generation 640 run 20261125_060235 measured 229.5-246.7 FPS, p50 3.74-3.94 ms, p95 5.59-6.37 ms, p99 7.08-7.60 ms; the prior 18-26 FPS reading was caused by eight leftover stopped-run mimita processes, which were terminated. Server 60 Hz is still not 99.99% because NPC ticks spike to 32-48 ms even with 1-2 NPCs, tracked as issue-ffcc6b95dec6. Corpse visual feedback sweeps were added in generations 638-640, but corpse.hit was not yet observed because post-kill shots missed the settled ragdoll or the server terminal event removed the client projectile first.
- Status: built; AK damage 65/55/45 and 3x head, NPC holes and organ chunks proven, corpse hit event pending
- Date, time, timezone: 2026-11-25T06:04:58+00:00
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
| `config/movement/movement-slowdm.json` |  |  |  | git checkout -- config/movement/movement-slowdm.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` |  |  |  | git checkout -- docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md |
| `docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md` |  |  |  | git checkout -- docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md` |  |  |  | git checkout -- docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md |
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

- Owner: opencode-deepseek-v4-1
- Focus: Build evidence: generations 632, 634, 635, 637, 638, 639 and 640 published. Runtime evidence: run 20261125_052838 (damage/hole/organ/kill/corpse spawn), 20261125_060235 (clean 240 FPS and server tick). Not proven: corpse.hit visual event, visual acceptance of the AK scale/arm pose, 60 Hz 99.99%. Issues: issue-2e7b0cd6f868 evidence, issue-16500baeb0cc clean FPS, issue-ffcc6b95dec6 NPC spikes.
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
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_060235\dev-loop\events.jsonl
- Session ID: session-20261125-060456

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
 M config/movement/movement-slowdm.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
 M docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
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
?? docs/changelog/2026-11-25/20261125_gen635-compile-cost-attribution.md
?? docs/changelog/2026-11-25/20261125_gen638-one-source-build-latency-and-live-activation.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-228f1f95a42d.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-9c1fc3e3e668.md
```

## Captured diff stat

```text
.../gamemodes/masterlist/gamemode-masterlist.json  |   6 +
 config/movement/movement-slowdm.json               |   4 +-
 config/weapons/bloodtest_ak47/config.json          |   8 +-
 devscripts/blender-ai                              |   0
 docs/issues/README.md                              |  19 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  52 +++++
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  37 ++++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   6 +
 ...nt-choppiness-against-the-issue-16500baeb0cc.md |   6 +
 ...n-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  14 ++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  12 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  90 ++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 232 +++++++++++++++++++++
 ...ts-and-diagnose-mimita-cp-issue-dc2f873290a7.md |  48 +++++
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |  20 ++
 src/network/multiplayer-projectiles.cpp            |  67 +++++-
 src/network/server-projectiles.cpp                 |   8 +-
 src/physics/physical-entity.cpp                    |  26 ++-
 src/sim/simulate-tick.cpp                          |  21 +-
 src/vehicles/vehicle-system.cpp                    |  35 +++-
 21 files changed, 692 insertions(+), 25 deletions(-)
```

## Captured changed paths

```text
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/movement/movement-slowdm.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
M	docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
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
