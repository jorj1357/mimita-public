// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Correct gen642 changed-source preflight timing interpretation

# Task

- Task ID: session-4e3021eb05d5
- Session ID: session-4e3021eb05d5
- Owner: mode3-latency-continuation
- Focus: issue-136d4553ff92
- Summary: Corrected the recorded gen642 phase decomposition after source-path and JSONL review. The 8.765s changed-source preflight contains the edited NPC translation unit's 8.724s compile; it is not extra pre-compilation overhead. Sent the correction to the build-latency issue and requested the A/B owner confirm run release. No build, runtime command, sample, daemon restart, or source edit.
- Status: investigation
- Date, time, timezone: 2026-11-25T09:33:43+00:00
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
| `config/debuglogger.json` |  |  |  | git checkout -- config/debuglogger.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/build_resource_sampler.py` |  |  |  | git checkout -- devscripts/dev_loop/build_resource_sampler.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/quality/test_build_resource_sampler.py` |  |  |  | git checkout -- devscripts/quality/test_build_resource_sampler.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md` |  |  |  | git checkout -- docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |
| `docs/issues/bug/20261121-broadcast-message-read-receipts-must-be-per-agen-issue-803e5246274e.md` |  |  |  | git checkout -- docs/issues/bug/20261121-broadcast-message-read-receipts-must-be-per-agen-issue-803e5246274e.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` |  |  |  | git checkout -- docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md |
| `docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md` |  |  |  | git checkout -- docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md |
| `docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md` |  |  |  | git checkout -- docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md` |  |  |  | git checkout -- docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md` |  |  |  | git checkout -- docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/npc/npc-combat.cpp` |  |  |  | git checkout -- src/npc/npc-combat.cpp |
| `src/physics/movement/physics-collision-glb-setup.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-glb-setup.cpp |
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
| `docs/changelog/2026-11-25/20261125_063747-generation-640-juggernaut-cpu-baseline-and-callsite-profile.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_063747-generation-640-juggernaut-cpu-baseline-and-callsite-profile.md' |
| `docs/changelog/2026-11-25/20261125_065257-current-header-fanout-plan-and-build-floors.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_065257-current-header-fanout-plan-and-build-floors.md' |
| `docs/changelog/2026-11-25/20261125_065338-cpu-profile-history-and-next-a-b.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_065338-cpu-profile-history-and-next-a-b.md' |
| `docs/changelog/2026-11-25/20261125_0758-independent-issue-review-issue-a8a9bfe5f5f6.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0758-independent-issue-review-issue-a8a9bfe5f5f6.md' |
| `docs/changelog/2026-11-25/20261125_0808-cpu-cache-lookup-a-b-result.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0808-cpu-cache-lookup-a-b-result.md' |
| `docs/changelog/2026-11-25/20261125_081127-dll-attempt-deadline.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_081127-dll-attempt-deadline.md' |
| `docs/changelog/2026-11-25/20261125_0830-independent-issue-review-issue-803e5246274e.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0830-independent-issue-review-issue-803e5246274e.md' |
| `docs/changelog/2026-11-25/20261125_085619-gen642-edit-to-live-and-sampler-critical-path.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_085619-gen642-edit-to-live-and-sampler-critical-path.md' |
| `docs/changelog/2026-11-25/20261125_0912-async-resource-sampler-start.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0912-async-resource-sampler-start.md' |
| `docs/changelog/2026-11-25/20261125_0924-independent-issue-review-issue-2a5c5da53565.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-25/20261125_0924-independent-issue-review-issue-2a5c5da53565.md' |
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

- Owner: mode3-latency-continuation
- Focus: issue-136d4553ff92
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 8fa06602be343fdaacacc1c07156a8595cc95035 / 8fa06602be343fdaacacc1c07156a8595cc95035
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-25T01:21:28.987918Z", "latest_successful_build_id": "2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b", "latest_successful_source_generation": 58}, "game_current": {"build_mode": "debug", "code_hash": "f34c0731cec4a05a04db5fff6e116841d11c37a0a97c03e688ee6ff198bcf18b", "dll": "build\\game\\generation-642\\game-g642.dll", "generation": 642, "revision": "86410e26a676a68638f2da6190003fa633dba0ba83b4a32f09b90d901b6a8903", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-642\\game-g642.dll", "generation": 642, "published_utc": "2026-11-25T08:43:58.832Z", "revision": "86410e26a676a68638f2da6190003fa633dba0ba83b4a32f09b90d901b6a8903", "status": "ready"}, "progress": {"compiled": 2, "elapsed_s": 13.827, "phase": "build_complete", "result": "success", "skipped": 85, "updated_utc": "2026-11-25T01:21:26.486Z"}}
- Generation: 642
- Artifact: build\game\generation-642\game-g642.dll
- Journal path: C:\mimita-v9\logs\2026-11-25\20261125_082735\server-01\events.jsonl
- Session ID: session-4e3021eb05d5

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
M config/debuglogger.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/weapons/bloodtest_ak47/config.json
 m devscripts/blender-ai
 M devscripts/dev_loop/build_resource_sampler.py
 M devscripts/dev_loop/game_build.py
 M devscripts/quality/test_build_resource_sampler.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/bug/20261121-broadcast-message-read-receipts-must-be-per-agen-issue-803e5246274e.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
 M docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
 M docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
 M src/network/multiplayer-projectiles.cpp
 M src/network/server-projectiles.cpp
 M src/npc/npc-combat.cpp
 M src/physics/movement/physics-collision-glb-setup.cpp
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
?? docs/changelog/2026-11-25/20261125_065257-current-header-fanout-plan-and-build-floors.md
?? docs/changelog/2026-11-25/20261125_065338-cpu-profile-history-and-next-a-b.md
?? docs/changelog/2026-11-25/20261125_0758-independent-issue-review-issue-a8a9bfe5f5f6.md
?? docs/changelog/2026-11-25/20261125_0808-cpu-cache-lookup-a-b-result.md
?? docs/changelog/2026-11-25/20261125_081127-dll-attempt-deadline.md
?? docs/changelog/2026-11-25/20261125_0830-independent-issue-review-issue-803e5246274e.md
?? docs/changelog/2026-11-25/20261125_085619-gen642-edit-to-live-and-sampler-critical-path.md
?? docs/changelog/2026-11-25/20261125_0912-async-resource-sampler-start.md
?? docs/changelog/2026-11-25/20261125_0924-independent-issue-review-issue-2a5c5da53565.md
?? docs/changelog/2026-11-25/20261125_gen635-compile-cost-attribution.md
?? docs/changelog/2026-11-25/20261125_gen638-one-source-build-latency-and-live-activation.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-228f1f95a42d.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-8af9b97c9a2a.md
?? docs/issues/build-friction/20261125-wait-over-budget-game-dll-build-at-game-build-issue-9c1fc3e3e668.md
```

## Captured diff stat

```text
config/debuglogger.json                            |    2 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |    6 +
 config/weapons/bloodtest_ak47/config.json          |    8 +-
 devscripts/blender-ai                              |    0
 devscripts/dev_loop/build_resource_sampler.py      |  209 ++-
 devscripts/dev_loop/game_build.py                  |  296 ++++-
 devscripts/quality/test_build_resource_sampler.py  |  158 ++-
 docs/issues/README.md                              |   41 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |   52 +
 ...-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |   16 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |   37 +
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |    6 +
 ...receipts-must-be-per-agen-issue-803e5246274e.md |   43 +
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |   46 +
 ...time-journal-events-wait--issue-6e2d7b91e7d9.md |   12 +
 ...nt-choppiness-against-the-issue-16500baeb0cc.md |    6 +
 ...n-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md |    6 +
 ...e-dll-build-at-game-build-issue-9aeec1fbae9d.md |   38 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   18 +
 ...a-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md |   46 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   34 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |   24 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md |   38 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   12 +
 ...-and-build-to-proof-links-issue-b396205aaecf.md |   16 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  682 ++++++++++
 ...nd-issue-discussion-panel-issue-6e528c4dc127.md |   56 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 1373 ++++++++++++++++++++
 ...ts-and-diagnose-mimita-cp-issue-dc2f873290a7.md |   48 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |    6 +
 ...gh-inclusion-headers-npc--issue-196c3045b4cb.md |   61 +
 src/network/multiplayer-projectiles.cpp            |   67 +-
 src/network/server-projectiles.cpp                 |    8 +-
 src/npc/npc-combat.cpp                             |    4 +
 .../movement/physics-collision-glb-setup.cpp       |   56 +-
 src/physics/physical-entity.cpp                    |   26 +-
 src/sim/simulate-tick.cpp                          |   21 +-
 src/vehicles/vehicle-system.cpp                    |   35 +-
 38 files changed, 3457 insertions(+), 156 deletions(-)
```

## Captured changed paths

```text
M	config/debuglogger.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/weapons/bloodtest_ak47/config.json
M	devscripts/blender-ai
M	devscripts/dev_loop/build_resource_sampler.py
M	devscripts/dev_loop/game_build.py
M	devscripts/quality/test_build_resource_sampler.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/bug/20261121-broadcast-message-read-receipts-must-be-per-agen-issue-803e5246274e.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
M	docs/issues/bug/20261124-diagnose-bloodtest-client-choppiness-against-the-issue-16500baeb0cc.md
M	docs/issues/bug/20261125-bloodtest-npc-simulation-spikes-exceed-the-60-hz-issue-ffcc6b95dec6.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261122-consolidate-runtime-tests-and-diagnose-mimita-cp-issue-dc2f873290a7.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/issues/tech-debt/20261123-header-fan-out-split-high-inclusion-headers-npc--issue-196c3045b4cb.md
M	src/network/multiplayer-projectiles.cpp
M	src/network/server-projectiles.cpp
M	src/npc/npc-combat.cpp
M	src/physics/movement/physics-collision-glb-setup.cpp
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
    "code_hash": "f34c0731cec4a05a04db5fff6e116841d11c37a0a97c03e688ee6ff198bcf18b",
    "dll": "build\\game\\generation-642\\game-g642.dll",
    "generation": 642,
    "revision": "86410e26a676a68638f2da6190003fa633dba0ba83b4a32f09b90d901b6a8903",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-642\\game-g642.dll",
    "generation": 642,
    "published_utc": "2026-11-25T08:43:58.832Z",
    "revision": "86410e26a676a68638f2da6190003fa633dba0ba83b4a32f09b90d901b6a8903",
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

## Follow-up: critical-path floor and owner state

At 2026-11-25T09:40:49Z, server PID10644 and client PID7452 were still alive in run20261125_082735. Their journals advanced through09:40:31Z; the server reported game generation642, 67 NPCs, and 1 player. The logger A/B setting remains npc_combat=errors. No CPU sample was taken and no A/B improvement is claimed. Coordination still marked cpu-a-b-verification active with last_seen09:20:26Z, but its recorded PID9492 was absent from the OS process list. The CPU measurement agent confirmed it can profile the gen642 pair only after that A/B owner releases it. I left the runtime and config unchanged.

For attempt game-dll-20261125T084339926983Z-be8067, the edited-TU compile took8.724s and the following monolithic LLD step took2.360s, sequentially. Their 11.084s sum already exceeds the 10s target before coherence checks, publish, or activation. This is a measured lower bound for that attempt, not a universal toolchain limit. Async sampler startup can recover about3.95s, but cannot by itself meet the target; the remaining path needs faster compilation or a path that avoids recompiling the full translation unit and/or relinking the monolithic DLL.

No build, daemon restart, runtime command, or manual sample was started in this session.
