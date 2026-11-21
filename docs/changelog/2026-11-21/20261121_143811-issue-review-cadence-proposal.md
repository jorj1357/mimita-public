// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Issue review cadence proposal

# Task

- Task ID: work-3088049cecd4
- Session ID: work-3088049cecd4
- Owner: cadence-review-design
- Focus: coordination,issue-review
- Summary: Reviewed coordination/self-improvement policy against current scripts; found review-at-activation/waiting and periodic audit are specified, but no fixed activation cadence or scheduler is enforced. Appended a low-noise, measurable rotating-review proposal to issue-91e3384b219b and cold-handoff guidance to issue-70cbf4d13c02. No source code, screenshots, OCR, or build changes.
- Status: partial
- Date, time, timezone: 2026-11-21T14:38:04+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

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
| `config/behavior-profiles.json` |  |  |  | git checkout -- config/behavior-profiles.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/movement/movement-heavy.json` |  |  |  | git checkout -- config/movement/movement-heavy.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/agent-wake.py` |  |  |  | git checkout -- devscripts/agent-wake.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md` |  |  |  | git checkout -- docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | git checkout -- docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md |
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` |  |  |  | git checkout -- docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md |
| `docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md` |  |  |  | git checkout -- docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md |
| `docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md` |  |  |  | git checkout -- docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md |
| `docs/specs/dev-loop/agent-build-queue.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-build-queue.md |
| `docs/specs/dev-loop/agent-context-contract.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-context-contract.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/workflows/issue-intake-and-triage.md` |  |  |  | git checkout -- docs/workflows/issue-intake-and-triage.md |
| `src/engine/engine-tick-combat.cpp` |  |  |  | git checkout -- src/engine/engine-tick-combat.cpp |
| `src/engine/engine-tick-net.cpp` |  |  |  | git checkout -- src/engine/engine-tick-net.cpp |
| `src/engine/engine-tick-net.h` |  |  |  | git checkout -- src/engine/engine-tick-net.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/schema/packet-combat.h` |  |  |  | git checkout -- src/network/schema/packet-combat.h |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `devscripts/quality/test_agent_wake.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_agent_wake.py' |
| `devscripts/quality/test_build_coordinator.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_build_coordinator.py' |
| `docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md' |
| `docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md' |
| `docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md' |
| `docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md' |
| `docs/changelog/2026-11-21/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/' |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md' |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md' |
| `docs/issues/bug/20261121-actor-outlines-jitter-away-from-actor-positions-issue-23e49d77be9f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-actor-outlines-jitter-away-from-actor-positions-issue-23e49d77be9f.md' |
| `docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md' |
| `docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md' |
| `docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md' |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md' |
| `docs/issues/bug/20261121-fun-counter-strike-npc-aim-and-time-to-kill-shou-issue-0a2a3fc00768.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-fun-counter-strike-npc-aim-and-time-to-kill-shou-issue-0a2a3fc00768.md' |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md' |
| `docs/issues/bug/20261121-surface-marks-stretch-to-contact-plane-and-remai-issue-3eaa6c834b9a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-surface-marks-stretch-to-contact-plane-and-remai-issue-3eaa6c834b9a.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-exe-build-at-starting-issue-054ea1bcda9b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-exe-build-at-starting-issue-054ea1bcda9b.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-450cfc80a0b9.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-450cfc80a0b9.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-ad873eff97f3.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-ad873eff97f3.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md' |
| `docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md' |
| `docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md' |
| `docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md' |
| `docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md' |
| `docs/issues/capability/20261121-composable-disaster-ingredients-and-community-au-issue-4df5f174932e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-composable-disaster-ingredients-and-community-au-issue-4df5f174932e.md' |
| `docs/issues/capability/20261121-grenade-physical-throw-and-arc-for-fun-counter-s-issue-91544c0fa36a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-grenade-physical-throw-and-arc-for-fun-counter-s-issue-91544c0fa36a.md' |
| `docs/issues/capability/20261121-local-npc-conversation-and-lightweight-social-re-issue-761c7ed90bd5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-local-npc-conversation-and-lightweight-social-re-issue-761c7ed90bd5.md' |
| `docs/issues/capability/20261121-long-horizon-vr-cross-platform-live-creator-and--issue-5534074d9309.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-long-horizon-vr-cross-platform-live-creator-and--issue-5534074d9309.md' |
| `docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md' |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md' |
| `docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md' |
| `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md' |
| `docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md' |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md' |
| `docs/issues/improvement/20261121-review-and-sort-the-new-autonomy-vehicle-and-pla-issue-4392f56334ef.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-review-and-sort-the-new-autonomy-vehicle-and-pla-issue-4392f56334ef.md' |
| `docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md' |
| `docs/issues/without/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/without/' |

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

- Owner: cadence-review-design
- Focus: coordination,issue-review
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "d70d49fd185ff12108e1f95c11c42f66e52c201f296d222d9cbba819c9e9a2f6", "dll": "build\\game\\generation-420\\game-g420.dll", "generation": 420, "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-420\\game-g420.dll", "generation": 420, "published_utc": "2026-11-21T13:59:59.761Z", "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f", "status": "ready"}, "progress": {"compiled": 17, "elapsed_s": 2727.517, "phase": "build_complete", "result": "success", "skipped": 70, "updated_utc": "2026-11-21T02:26:10.895Z"}}
- Generation: 420
- Artifact: build\game\generation-420\game-g420.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_135248_01\server-01\events.jsonl
- Session ID: work-3088049cecd4

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
 M config/behavior-profiles.json
 M config/hot-reload/packet-ledger.json
 M config/movement/movement-heavy.json
 M config/movement/movement-source.json
 M config/video-settings.json
 M devscripts/agent-wake.py
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/agent-context-contract.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/workflows/issue-intake-and-triage.md
 M src/engine/engine-tick-combat.cpp
 M src/engine/engine-tick-net.cpp
 M src/engine/engine-tick-net.h
 M src/network/multiplayer-projectiles.cpp
 M src/network/schema/packet-combat.h
 M src/network/server-projectiles.cpp
 M src/npc/npc.cpp
?? devscripts/quality/test_agent_wake.py
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-actor-outlines-jitter-away-from-actor-positions-issue-23e49d77be9f.md
?? docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
?? docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md
?? docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-fun-counter-strike-npc-aim-and-time-to-kill-shou-issue-0a2a3fc00768.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/bug/20261121-surface-marks-stretch-to-contact-plane-and-remai-issue-3eaa6c834b9a.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/build-friction/20261121-wait-over-budget-exe-build-at-starting-issue-054ea1bcda9b.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-450cfc80a0b9.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-ad873eff97f3.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md
?? docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md
?? docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md
?? docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
?? docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
?? docs/issues/capability/20261121-composable-disaster-ingredients-and-community-au-issue-4df5f174932e.md
?? docs/issues/capability/20261121-grenade-physical-throw-and-arc-for-fun-counter-s-issue-91544c0fa36a.md
?? docs/issues/capability/20261121-local-npc-conversation-and-lightweight-social-re-issue-761c7ed90bd5.md
?? docs/issues/capability/20261121-long-horizon-vr-cross-platform-live-creator-and--issue-5534074d9309.md
?? docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
?? docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
?? docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
?? docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
?? docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
?? docs/issues/improvement/20261121-review-and-sort-the-new-autonomy-vehicle-and-pla-issue-4392f56334ef.md
?? docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
?? docs/issues/without/
```

## Captured diff stat

```text
config/accounts/default.json                       |   8 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  23 ++-
 config/hot-reload/packet-ledger.json               |   6 +-
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/agent-wake.py                           |  30 +++-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 154 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  21 ++-
 devscripts/dev_loop/parallel_test_runs.py          |  56 ++++++--
 devscripts/game_build_worker.py                    |  93 ++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  30 +++-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +++-
 docs/issues/README.md                              |  64 +++++++--
 ...ventory-can-omit-or-miscl-issue-35fd6df3d947.md |  26 ++++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |   6 +
 ...lision-and-mace-active-po-issue-503f3e410c01.md |   6 +
 ...nch-opencode-api-key-expi-issue-36579a10979a.md |   6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md | 123 ++++++++++++++++
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |   6 +
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 +++
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md | 115 +++++++++++++++
 ...s-to-the-latest-file-atom-issue-52b59bdd440c.md |  36 +++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  26 ++++
 ...le-restart-and-self-recov-issue-6fc8f853a674.md |  18 +++
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  50 +++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 ++++
 docs/specs/dev-loop/agent-build-queue.md           |  11 ++
 docs/specs/dev-loop/agent-context-contract.md      |  12 ++
 docs/specs/dev-loop/agent-coordination.md          |  23 +++
 docs/specs/dev-loop/agent-self-improvement.md      |  36 +++++
 docs/specs/dev-loop/build-time-observability.md    |  22 +++
 docs/workflows/issue-intake-and-triage.md          |  16 +++
 src/engine/engine-tick-combat.cpp                  |   8 +-
 src/engine/engine-tick-net.cpp                     |   9 ++
 src/engine/engine-tick-net.h                       |   5 +
 src/network/multiplayer-projectiles.cpp            |  15 +-
 src/network/schema/packet-combat.h                 |   2 +-
 src/network/server-projectiles.cpp                 |   5 +
 src/npc/npc.cpp                                    |  11 +-
 45 files changed, 1110 insertions(+), 89 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/hot-reload/packet-ledger.json
M	config/movement/movement-heavy.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/agent-wake.py
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/agent-context-contract.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/workflows/issue-intake-and-triage.md
M	src/engine/engine-tick-combat.cpp
M	src/engine/engine-tick-net.cpp
M	src/engine/engine-tick-net.h
M	src/network/multiplayer-projectiles.cpp
M	src/network/schema/packet-combat.h
M	src/network/server-projectiles.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-21T02:26:50.306117Z",
    "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "d70d49fd185ff12108e1f95c11c42f66e52c201f296d222d9cbba819c9e9a2f6",
    "dll": "build\\game\\generation-420\\game-g420.dll",
    "generation": 420,
    "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-420\\game-g420.dll",
    "generation": 420,
    "published_utc": "2026-11-21T13:59:59.761Z",
    "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f",
    "status": "ready"
  },
  "progress": {
    "compiled": 17,
    "elapsed_s": 2727.517,
    "phase": "build_complete",
    "result": "success",
    "skipped": 70,
    "updated_utc": "2026-11-21T02:26:10.895Z"
  }
}
```
