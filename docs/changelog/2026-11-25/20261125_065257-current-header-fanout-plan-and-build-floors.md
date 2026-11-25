// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Current header fanout plan and build floors

# Task

- Task ID: session-00f89787705b
- Session ID: session-00f89787705b
- Owner: header-fanout-latency-plan
- Focus: issue-196c3045b4cb
- Summary: Captured the current generation-640 dependency graph, compared exact header invalidations from generations 634/635 with the one-source generation-640 compile/link floor, and appended an evidence-ranked staged plan to the canonical header issue. No C++ source or active build configuration changed.
- Status: progress
- Date, time, timezone: 2026-11-25T06:52:56+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 8fa06602be343fdaacacc1c07156a8595cc95035
- Final commit: 8fa06602be343fdaacacc1c07156a8595cc95035

# Pre-existing changes

The shared worktree was already dirty at session start. Its initial `git status --short` included unrelated config, issue, changelog, and C++ changes owned by other work. This session changed only the existing header-fanout issue by appending the new evidence/plan section, created this changelog, and generated the local ignored graph artifact. The changelog helper's captured diff/path inventory is a workspace snapshot, not attribution to this session. Do not run its generated `git checkout`/remove suggestions against other paths; that would overwrite or delete shared work.

- Exact status output: initial `git status --short` was read at 2026-11-25T06:44Z; full snapshot is captured below.
- Files not created or modified by this session: all source/config files and other issue/changelog files listed in the captured snapshot.

# Requested behavior

The user asked for a plan that reduces header fanout over time so editing a header does not trigger a roughly 21-minute build. The active goal remains <=10 seconds for a 15–20 C++ file Mode 3 batch with a live Juggernaut Horde client/server pair and measured resource/runtime proof.

# Specification alignment

- Current documents: `docs/architecture/headers/header-hygiene.md`; `docs/architecture/live-development/incremental-build-and-header-fanout.md`; `docs/operations/devscripts/dependency-graph.md`; `docs/operations/devscripts/cold-change-guard.md`; `docs/specs/dev-loop/build-time-observability.md`.
- Requirements followed: inspect revision-stamped compiler depfiles; distinguish actual invalidations from graph closure; preserve all required dependents; do not change ABI/object layout casually; do not modify PCH; use one root dev-loop authority for builds; separate build-time improvements from live activation proof.
- The new plan is recorded in the existing issue; no implementation or build authority changed.
- No scope conflict or user decision was needed.

# Exact implementation changes

## File: `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md`

- Lines/headings: appended heading at line 290, through the tranche acceptance at line 305.
- Old: no `2026-11-25 current depfile census and latency-ranked plan` section existed; the issue ended its prior discussion at the earlier census/coordination entries.
- New: added the current generation-640 depfile counts, exact gen634/gen635 invalidation/build evidence, the gen640 one-source compile/link counterexample, a seven-step staged plan, and acceptance criteria. The exact text is preserved in that issue section.
- Reason: prevent treating header fan-in as the only cost and rank changes by real invalidation/critical-path seconds.
- Unrelated behavior: no C++ source, build configuration, live process, or runtime was changed.

## File: `docs/changelog/2026-11-25/20261125_065257-current-header-fanout-plan-and-build-floors.md`

- New file; no prior content. Records this session, the workspace-attribution caveat, evidence, and validation.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md` | No current-census section | Appended lines 290–305 | Preserve current measured plan | Append-only issue history; do not remove |
| `docs/changelog/2026-11-25/20261125_065257-current-header-fanout-plan-and-build-floors.md` | No file | This record | Required per session | No reverse action |


# Captured workspace status warning

The automatic changed-path snapshot below includes pre-existing work from other agents and users. It is not a list of this session's edits. No reverse or cleanup command was run or is authorized by this record.

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

- Documentation review: `docs/skills/documentation-checker-v1.md`; the existing issue remains the canonical audience/purpose and this section appends evidence without changing requirements.
- Dependency graph command: `python devscripts/dependency_graph.py build/game/objects/b10e3a31b3ae8637 --top 1 --json .dev/perf-captures/header-fanout-20261125T0646Z.json`; completed successfully at 2026-11-25T06:48:18Z. Revision `308b20ce51a80eb391655fb4c842cd33ccf692ce36149766fddc2a63246ebe8e`, 2,113 depfiles, 644 sources, 600 headers, 16 stale sources. A prior console rendering in the same activation showed top 35 fan-in values; the complete report is in the local artifact.
- No tests, build, runtime command, or process restart was run. Root dev-loop status at 06:44Z reported idle/no pending batch, with latest artifact stale relative to current source state.
- Live pair PIDs 17988/13872 were observed alive at 06:46Z; this documentation work did not command or restart them.
- Output artifact: `.dev/perf-captures/header-fanout-20261125T0646Z.json` (local ignored measurement artifact).

# Measured evidence

- Current graph: PCH 615/644 TU closure; structured-log.h 341; physics-types.h 298; map-mesh-types.h 280; movement-types.h 276; config.h 274; weapon-types.h 273; player.h 237; packets.h 169; npc.h 139. These are compiler depfile closures, not direct textual include counts; 16 source depfiles are stale.
- Gen634: 181.396s total; 55 compiled/590 skipped; packets.h invalidated 40 TUs and server.h 4; LLD 15.410s. The summed per-TU compiler durations for packet-dependent jobs were 935.802s and overlap across workers, so this is not elapsed wall time.
- Gen635: 255.459s total; 13 actual weapon-fire.h invalidations; their per-TU compile durations summed to 603.808s with parallel overlap; compile phase wall time 95.041s.
- Gen640: one changed `.cpp`, 643 skipped; 131.437s total; changed TU 95.926s; LLD 11.375s; object scan 0.083s. Changed-source-preflight is separately reported as 96.171s and is not added to compile time because phase overlap semantics are unresolved.
- None is an before/after header-split result; no optimization claim is made. Runtime frame/tick values are not applicable to this documentation-only session.

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

- Owner: header-fanout-latency-plan
- Focus: issue-196c3045b4cb
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
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_061355\server-01\events.jsonl
- Session ID: session-00f89787705b

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
 M docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
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
 M docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
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
?? docs/changelog/2026-11-25/20261125_063747-generation-640-juggernaut-cpu-baseline-and-callsite-profile.md
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
 docs/issues/README.md                              |  29 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  52 +++
 ...-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |  16 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  37 ++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   6 +
 ...time-journal-events-wait--issue-6e2d7b91e7d9.md |  12 +
 ...nt-choppiness-against-the-issue-16500baeb0cc.md |   6 +
 ...n-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |   6 +
 ...e-dll-build-at-game-build-issue-9aeec1fbae9d.md |  38 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  18 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  20 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  12 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 164 +++++++
 ...nd-issue-discussion-panel-issue-6e528c4dc127.md |  56 +++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 520 +++++++++++++++++++++
 ...ts-and-diagnose-mimita-cp-issue-dc2f873290a7.md |  48 ++
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   6 +
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |  61 +++
 src/network/multiplayer-projectiles.cpp            |  67 ++-
 src/network/server-projectiles.cpp                 |   8 +-
 src/physics/physical-entity.cpp                    |  26 +-
 src/sim/simulate-tick.cpp                          |  21 +-
 src/vehicles/vehicle-system.cpp                    |  35 +-
 27 files changed, 1262 insertions(+), 28 deletions(-)
```

## Captured changed paths

```text
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
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
M	docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
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
