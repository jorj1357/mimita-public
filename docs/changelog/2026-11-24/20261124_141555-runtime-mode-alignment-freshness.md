// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Runtime mode alignment freshness

# Task

- Task ID: session-1f8b79e9d752
- Session ID: session-1f8b79e9d752
- Owner: resource-profiler
- Focus: Build resource sampler runtime alignment evidence
- Summary: Fixed a false runtime-alignment failure in the build resource sampler: same-run server/client presence now uses fresh role workload rows, while mode matching independently requires fresh authoritative mode observations. The retained generation-594 sidecar replay now identifies server25840/client12848 as one same-run pair with mode_unverified because fresh client mode was absent; it does not claim the mode. Added missing/stale-mode regression coverage. Focused suite passes 14/14 and py_compile passes. The root dev-loop daemon has not reloaded the change, so live sidecar verification remains pending.
- Status: BLOCKED
- Date, time, timezone: 2026-11-24T14:15:55+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d1ec3050f3275c83d302a83349f0a8db64ea3d74
- Final commit: d1ec3050f3275c83d302a83349f0a8db64ea3d74

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
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/impact_decals.json` |  |  |  | git checkout -- config/impact_decals.json |
| `config/input/gamepad.json` |  |  |  | git checkout -- config/input/gamepad.json |
| `config/ragdoll.json` |  |  |  | git checkout -- config/ragdoll.json |
| `config/tips.json` |  |  |  | git checkout -- config/tips.json |
| `config/weapons/bloodtest_ak47/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_ak47/config.json |
| `config/weapons/bloodtest_judgement/config.json` |  |  |  | git checkout -- config/weapons/bloodtest_judgement/config.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/build_resource_sampler.py` |  |  |  | git checkout -- devscripts/dev_loop/build_resource_sampler.py |
| `devscripts/quality/test_build_resource_sampler.py` |  |  |  | git checkout -- devscripts/quality/test_build_resource_sampler.py |
| `docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md` |  |  |  | git checkout -- docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md |
| `docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md |
| `docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md` |  |  |  | git checkout -- docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | git checkout -- docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` |  |  |  | git checkout -- docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md` |  |  |  | git checkout -- docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/networking/networking.md` |  |  |  | git checkout -- docs/specs/networking/networking.md |
| `docs/specs/vehicles/vehicles.md` |  |  |  | git checkout -- docs/specs/vehicles/vehicles.md |
| `src/combat/explosion-fx.cpp` |  |  |  | git checkout -- src/combat/explosion-fx.cpp |
| `src/combat/weapon-execution.cpp` |  |  |  | git checkout -- src/combat/weapon-execution.cpp |
| `src/config/camera-config.cpp` |  |  |  | git checkout -- src/config/camera-config.cpp |
| `src/config/camera-config.h` |  |  |  | git checkout -- src/config/camera-config.h |
| `src/debug/debug-visuals-labels.cpp` |  |  |  | git checkout -- src/debug/debug-visuals-labels.cpp |
| `src/debug/debug-visuals.h` |  |  |  | git checkout -- src/debug/debug-visuals.h |
| `src/engine/engine-tick-ui-game-hud.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui-game-hud.cpp |
| `src/engine/engine-tick-ui.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui.cpp |
| `src/engine/engine-tick-ui.h` |  |  |  | git checkout -- src/engine/engine-tick-ui.h |
| `src/engine/engine-tick.cpp` |  |  |  | git checkout -- src/engine/engine-tick.cpp |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `src/gui/ui-system-internal.h` |  |  |  | git checkout -- src/gui/ui-system-internal.h |
| `src/gui/ui-system-render.cpp` |  |  |  | git checkout -- src/gui/ui-system-render.cpp |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-system.h` |  |  |  | git checkout -- src/gui/ui-system.h |
| `src/input/gamepad-input.cpp` |  |  |  | git checkout -- src/input/gamepad-input.cpp |
| `src/input/gamepad-input.h` |  |  |  | git checkout -- src/input/gamepad-input.h |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/terminal/debug-commands.cpp` |  |  |  | git checkout -- src/terminal/debug-commands.cpp |
| `config/input/gamecontrollerdb.LICENSE.txt` |  |  |  | Remove-Item -LiteralPath 'config/input/gamecontrollerdb.LICENSE.txt' |
| `config/input/gamecontrollerdb.txt` |  |  |  | Remove-Item -LiteralPath 'config/input/gamecontrollerdb.txt' |
| `docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md' |
| `docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md' |
| `docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md' |
| `docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md' |
| `docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md' |
| `docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md' |
| `docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md' |
| `docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md' |
| `docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md' |
| `docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md' |
| `docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md' |

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
- Focus: Build resource sampler runtime alignment evidence
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d1ec3050f3275c83d302a83349f0a8db64ea3d74 / d1ec3050f3275c83d302a83349f0a8db64ea3d74
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T07:48:35.684015Z", "latest_successful_build_id": "2026-11-24T07:48:35.684015Z:52c3ec0ab05736cff06e7105a6966f96920790dde025b8d21ecdb49e24a0b122", "latest_successful_source_generation": 8}, "game_current": {"build_mode": "debug", "code_hash": "c0baac6a19291032736c5f6857de90174c7b9decd3f3c75360a3bd8a1aed46ba", "dll": "build\\game\\generation-594\\game-g594.dll", "generation": 594, "revision": "73a9b9633af039ba37021b644acceadf1bad50687ab7bf20a11b3eefa3a989b5", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-594\\game-g594.dll", "generation": 594, "published_utc": "2026-11-24T12:23:47.916Z", "revision": "73a9b9633af039ba37021b644acceadf1bad50687ab7bf20a11b3eefa3a989b5", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 88.549, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-24T07:51:09.286Z"}}
- Generation: 594
- Artifact: build\game\generation-594\game-g594.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_121332\server-01\events.jsonl
- Session ID: session-1f8b79e9d752

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
M config/analytics.json
 M config/gamemodes/bloodtest.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/hot-reload/packet-ledger.json
 M config/impact_decals.json
 M config/input/gamepad.json
 M config/ragdoll.json
 M config/tips.json
 M config/weapons/bloodtest_ak47/config.json
 M config/weapons/bloodtest_judgement/config.json
 m devscripts/blender-ai
 M devscripts/dev_loop/build_resource_sampler.py
 M devscripts/quality/test_build_resource_sampler.py
 M docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
 M docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
 M docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md
 M docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/networking/networking.md
 M docs/specs/vehicles/vehicles.md
 M src/combat/explosion-fx.cpp
 M src/combat/weapon-execution.cpp
 M src/config/camera-config.cpp
 M src/config/camera-config.h
 M src/debug/debug-visuals-labels.cpp
 M src/debug/debug-visuals.h
 M src/engine/engine-tick-ui-game-hud.cpp
 M src/engine/engine-tick-ui.cpp
 M src/engine/engine-tick-ui.h
 M src/engine/engine-tick.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/gui/gui-main.cpp
 M src/gui/ui-system-internal.h
 M src/gui/ui-system-render.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-system.h
 M src/input/gamepad-input.cpp
 M src/input/gamepad-input.h
 M src/network/multiplayer-tick.cpp
 M src/network/server-projectiles.cpp
 M src/terminal/debug-commands.cpp
?? config/input/gamecontrollerdb.LICENSE.txt
?? config/input/gamecontrollerdb.txt
?? docs/changelog/2026-11-24/20261124_0845-issue-3af736bb9337-independent-review.md
?? docs/changelog/2026-11-24/20261124_0915-mode3-latency-ownership-and-runtime-recheck.md
?? docs/changelog/2026-11-24/20261124_0946-issue-816c05804889-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md
?? docs/changelog/2026-11-24/20261124_1230-issue-9a301a157e7c-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1236-issue-8576955c4fb5-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_123857-bloodtest-presentation-slice-json-hud-policy-ui-ca-bodycam-fov-bullet-shockwaves-gore-tuning.md
?? docs/changelog/2026-11-24/20261124_1239-issue-b18e63894585-independent-review.md
?? docs/changelog/2026-11-24/20261124_1250-mode3-module-generation-refresh-source-trace.md
?? docs/changelog/2026-11-24/20261124_1257-issue-0322fa45b0e8-game-dll-build-wait-review.md
?? docs/changelog/2026-11-24/20261124_1329-issue-13d080318e1c-independent-review.md
?? docs/changelog/2026-11-24/20261124_135058-mode3-current-resource-gate.md
?? docs/changelog/2026-11-24/20261124_140354-cpu-profile-generation-gate.md
?? docs/coordination/decisions/20261124_102412-conflict-c1dde7354156-decision-b4ccee465b05.md
?? docs/coordination/decisions/20261124_111458-retain-gen590-changelog-append-decision-33d62425dbfd.md
?? docs/coordination/decisions/20261124_115516-retain-gen591-593-evidence-decision-777163429a6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-3472692a57c2.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-6613d3354c65.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8576955c4fb5.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-8b5c92a76d97.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-9a301a157e7c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-c7d9edb5e68b.md
```

## Captured diff stat

```text
config/analytics.json                              |   2 +-
 config/gamemodes/bloodtest.json                    |  19 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |  15 +
 config/hot-reload/packet-ledger.json               | 160 +++---
 config/impact_decals.json                          |  11 +-
 config/input/gamepad.json                          |   1 +
 config/ragdoll.json                                |  13 +-
 config/tips.json                                   |   2 +-
 config/weapons/bloodtest_ak47/config.json          |  51 +-
 config/weapons/bloodtest_judgement/config.json     |  35 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/build_resource_sampler.py      |  33 +-
 devscripts/quality/test_build_resource_sampler.py  |  56 ++
 ...source-correction-and-blocked-paired-profile.md |  99 ++++
 docs/issues/README.md                              |  40 +-
 ...rs-fail-to-consistently-e-issue-b95574b63f29.md |  62 ++
 ...-not-fire-at-opposing-act-issue-1b2463da5666.md |   6 +
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |  16 +
 ...igation-regresses-into-co-issue-b18e63894585.md |  43 ++
 ...time-journal-events-wait--issue-6e2d7b91e7d9.md |   8 +
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  41 ++
 ...e-dll-build-at-game-build-issue-dad513d47108.md |  34 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  36 ++
 ...budget-exe-build-at-setup-issue-128ca34b21c6.md |  39 ++
 ...e-dll-build-at-game-build-issue-816c05804889.md |  22 +
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |   8 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  20 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  15 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  12 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 515 +++++++++++++++++
 ...ry-client-frame-below-240-issue-13d080318e1c.md |  31 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 628 +++++++++++++++++++++
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   6 +
 docs/specs/dev-loop/agent-coordination.md          |  42 ++
 docs/specs/networking/networking.md                |  36 +-
 docs/specs/vehicles/vehicles.md                    |  37 ++
 src/combat/explosion-fx.cpp                        |  48 ++
 src/combat/weapon-execution.cpp                    |   9 +
 src/config/camera-config.cpp                       |   1 +
 src/config/camera-config.h                         |   1 +
 src/debug/debug-visuals-labels.cpp                 |   1 +
 src/debug/debug-visuals.h                          |   1 +
 src/engine/engine-tick-ui-game-hud.cpp             |   9 +-
 src/engine/engine-tick-ui.cpp                      |   5 +-
 src/engine/engine-tick-ui.h                        |   3 +-
 src/engine/engine-tick.cpp                         |  22 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |  11 +
 src/gamemode/gamemode.cpp                          |  15 +
 src/gamemode/gamemode.h                            |  17 +
 src/gui/gui-main.cpp                               |  17 +-
 src/gui/ui-system-internal.h                       |   1 +
 src/gui/ui-system-render.cpp                       |  20 +-
 src/gui/ui-system.cpp                              |  32 +-
 src/gui/ui-system.h                                |  13 +-
 src/input/gamepad-input.cpp                        | 129 +++++
 src/input/gamepad-input.h                          |   2 +
 src/network/multiplayer-tick.cpp                   |  95 ++--
 src/network/server-projectiles.cpp                 |  38 +-
 src/terminal/debug-commands.cpp                    |  25 +
 59 files changed, 2510 insertions(+), 199 deletions(-)
```

## Captured changed paths

```text
M	config/analytics.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	config/impact_decals.json
M	config/input/gamepad.json
M	config/ragdoll.json
M	config/tips.json
M	config/weapons/bloodtest_ak47/config.json
M	config/weapons/bloodtest_judgement/config.json
M	devscripts/blender-ai
M	devscripts/dev_loop/build_resource_sampler.py
M	devscripts/quality/test_build_resource_sampler.py
M	docs/changelog/2026-11-24/20261124_0810-gen587-resource-correction-and-blocked-paired-profile.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
M	docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
M	docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-setup-issue-128ca34b21c6.md
M	docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-816c05804889.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/networking/networking.md
M	docs/specs/vehicles/vehicles.md
M	src/combat/explosion-fx.cpp
M	src/combat/weapon-execution.cpp
M	src/config/camera-config.cpp
M	src/config/camera-config.h
M	src/debug/debug-visuals-labels.cpp
M	src/debug/debug-visuals.h
M	src/engine/engine-tick-ui-game-hud.cpp
M	src/engine/engine-tick-ui.cpp
M	src/engine/engine-tick-ui.h
M	src/engine/engine-tick.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/gui/gui-main.cpp
M	src/gui/ui-system-internal.h
M	src/gui/ui-system-render.cpp
M	src/gui/ui-system.cpp
M	src/gui/ui-system.h
M	src/input/gamepad-input.cpp
M	src/input/gamepad-input.h
M	src/network/multiplayer-tick.cpp
M	src/network/server-projectiles.cpp
M	src/terminal/debug-commands.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-24T07:48:35.684015Z",
    "latest_successful_build_id": "2026-11-24T07:48:35.684015Z:52c3ec0ab05736cff06e7105a6966f96920790dde025b8d21ecdb49e24a0b122",
    "latest_successful_source_generation": 8
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "c0baac6a19291032736c5f6857de90174c7b9decd3f3c75360a3bd8a1aed46ba",
    "dll": "build\\game\\generation-594\\game-g594.dll",
    "generation": 594,
    "revision": "73a9b9633af039ba37021b644acceadf1bad50687ab7bf20a11b3eefa3a989b5",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-594\\game-g594.dll",
    "generation": 594,
    "published_utc": "2026-11-24T12:23:47.916Z",
    "revision": "73a9b9633af039ba37021b644acceadf1bad50687ab7bf20a11b3eefa3a989b5",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 88.549,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-24T07:51:09.286Z"
  }
}
```

## Runtime-mode freshness correction

### Exact code changes

- `devscripts/dev_loop/build_resource_sampler.py:884-886`: before, server and client eligibility both required a fresh `journal_mode` row, so a same-run client with fresh frame/NPC metrics but no recent mode event was excluded from pairing. The old client clause was:

  ```python
  clients = [row for row in rows if row.get("role") == "client" and
             fresh(row, "client_frame_window", "client_npc_view_window",
                   "journal_mode")]
  ```

  After, it is:

  ```python
  clients = [row for row in rows if row.get("role") == "client" and
             fresh(row, "client_frame_window", "client_npc_view_window")]
  ```

  Server eligibility likewise now requires only fresh `generation_npc_player` and `server_tick_window` rows. Pair identity is selected using PID-verified runtime context and the shared run ID.
- `devscripts/dev_loop/build_resource_sampler.py:897-923`: before, the mode comparison read `journal.actual_mode` without independently validating it, while eligibility indirectly required freshness. After, both server/client mode values are accepted only if their authoritative `journal_mode` rows are fresh; summary includes the mode observation timestamps and an explicit reason (`fresh_authoritative_mode_missing_or_stale` or `fresh_authoritative_modes_disagree`). When mode proof is missing, the output retains `same_run_server_client=true` and reports `status=mode_unverified`; `actual_mode` stays null. This preserves uncertainty and avoids treating an old mode event as current.
- `devscripts/quality/test_build_resource_sampler.py:499-552`: added tests for same-run pairing when client mode is absent and for rejecting stale mode evidence. No prior text was replaced.
- `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d:3971`: appended exact replay findings for gap `build-resource-sampling:game-dll-20261124T122208532874Z-63061a`.

### Direct replay and limits

- Replayed the final saved sample from `.dev/build-timings/game-dll-20261124T122208532874Z-63061a.resource.jsonl` at its recorded time, 2026-11-24T12:23:47.954Z. The sampled context has server PID25840 and client PID12848, both mapped to run `20261124_121332`. The updated code returns `same_run_server_client=true`, `status=mode_unverified`, server mode `bloodtest`, client mode null, and the explicit missing/stale-mode reason.
- This corrects same-run pair detection but does not establish the client's actual mode, so it does not close the sidecar's mode/workload evidence gap. It is a telemetry classification fix, not a MiMITA runtime CPU optimization.
- Tests: `python -m unittest -v devscripts.quality.test_build_resource_sampler` passed all 14 tests; `python -m py_compile devscripts/dev_loop/build_resource_sampler.py devscripts/quality/test_build_resource_sampler.py` passed. `git diff --check` passed for both changed code files.
- No compiler/build, runtime restart, game command, or live sampling was performed. The root dev-loop daemon has not reloaded the updated Python module. I messaged `root-mode3-latency` to coordinate a safe daemon reload and append the result to the DLL latency issue. Verify the next real sidecar after that reload before treating this as live telemetry proof.

### Goal status

No runtime optimization has been measured or retained yet. This pass improved the reliability of evidence collection; the next actual CPU pass still requires an owner-approved generation-aligned workload and safe memory headroom, followed by the bounded callsite probe and one controlled code change.
