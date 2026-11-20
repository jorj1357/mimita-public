// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Review over-budget exe-build wait issue-00b133ae7e13

# Task

- Task ID: session-9461763073f8
- Session ID: session-9461763073f8
- Owner: build-wait-reviewer
- Focus: build-friction
- Summary: Reviewed the exe-build compiling wait using build metrics because the active run journal was pruned; identified single-TU compile contention (12+12 workers on 12 logical CPUs) plus source churn causing repeated stale rebuilds; recorded recurrence and wake-prompt truncation evidence. No source edits.
- Status: reviewed
- Date, time, timezone: 2026-11-20T20:12:46+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 6d92afa35b434790e1b42ebb9667be69a216cde2
- Final commit: 6d92afa35b434790e1b42ebb9667be69a216cde2

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
| `.opencode/plugins/mimita-user-context.ts` |  |  |  | git checkout -- .opencode/plugins/mimita-user-context.ts |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/build_friction.py` |  |  |  | git checkout -- devscripts/build_friction.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/scenarios/user/dust2_ramp_collision_v1.json` |  |  |  | git checkout -- devscripts/scenarios/user/dust2_ramp_collision_v1.json |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md` |  |  |  | git checkout -- docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` |  |  |  | git checkout -- docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md |
| `docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md` |  |  |  | git checkout -- docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |
| `docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md` |  |  |  | git checkout -- docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md` |  |  |  | git checkout -- docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md |
| `docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md |
| `docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md |
| `docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-wake.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-wake.md |
| `docs/specs/dev-loop/multi-instance.md` |  |  |  | git checkout -- docs/specs/dev-loop/multi-instance.md |
| `docs/specs/world-projects/world-edit-commands.md` |  |  |  | git checkout -- docs/specs/world-projects/world-edit-commands.md |
| `docs/specs/world-projects/world-projects.md` |  |  |  | git checkout -- docs/specs/world-projects/world-projects.md |
| `docs/workflows/issue-intake-and-triage.md` |  |  |  | git checkout -- docs/workflows/issue-intake-and-triage.md |
| `src/actor/player-ai.cpp` |  |  |  | git checkout -- src/actor/player-ai.cpp |
| `src/debug/structured-log.cpp` |  |  |  | git checkout -- src/debug/structured-log.cpp |
| `src/engine/engine-tick-combat.cpp` |  |  |  | git checkout -- src/engine/engine-tick-combat.cpp |
| `src/engine/engine-tick-ui-replay-hud.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui-replay-hud.cpp |
| `src/mapeditor/map-editor-projects.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-projects.cpp |
| `src/mapeditor/map-editor-projects.h` |  |  |  | git checkout -- src/mapeditor/map-editor-projects.h |
| `src/network/multiplayer-packets.cpp` |  |  |  | git checkout -- src/network/multiplayer-packets.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/reliable-gameplay-events.cpp` |  |  |  | git checkout -- src/network/reliable-gameplay-events.cpp |
| `src/network/server-gamemode.cpp` |  |  |  | git checkout -- src/network/server-gamemode.cpp |
| `src/network/server-packets.cpp` |  |  |  | git checkout -- src/network/server-packets.cpp |
| `src/physics/movement/actor-final-pose.cpp` |  |  |  | git checkout -- src/physics/movement/actor-final-pose.cpp |
| `src/physics/movement/physics-collision-core.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-core.cpp |
| `src/physics/movement/physics-collision-glb-main.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-glb-main.cpp |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` |  |  |  | git checkout -- src/physics/movement/physics-collision-glb-sweep-slide.cpp |
| `.opencode/skills/` |  |  |  | Remove-Item -LiteralPath '.opencode/skills/' |
| `devscripts/scenarios/world_live_fork_v1.json` |  |  |  | Remove-Item -LiteralPath 'devscripts/scenarios/world_live_fork_v1.json' |
| `docs/changelog/2026-11-20/20261120_145200-end-to-end-consistency-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_145200-end-to-end-consistency-investigation.md' |
| `docs/changelog/2026-11-20/20261120_150557-end-to-end-connection-reliability-issue-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_150557-end-to-end-connection-reliability-issue-intake.md' |
| `docs/changelog/2026-11-20/20261120_151900-consistency-evidence-follow-up.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_151900-consistency-evidence-follow-up.md' |
| `docs/changelog/2026-11-20/20261120_152300-dashboard-responsiveness.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_152300-dashboard-responsiveness.md' |
| `docs/changelog/2026-11-20/20261120_152837-connection-reliability-evidence-continuation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_152837-connection-reliability-evidence-continuation.md' |
| `docs/changelog/2026-11-20/20261120_155514-connection-reliability-evidence-continuation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_155514-connection-reliability-evidence-continuation.md' |
| `docs/changelog/2026-11-20/20261120_155700-payload-cart-transform-diagnostics.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_155700-payload-cart-transform-diagnostics.md' |
| `docs/changelog/2026-11-20/20261120_160817-investigate-arcade-capsule-world-collision-response.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_160817-investigate-arcade-capsule-world-collision-response.md' |
| `docs/changelog/2026-11-20/20261120_162609-connection-hud-status-and-reliability-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_162609-connection-hud-status-and-reliability-trace.md' |
| `docs/changelog/2026-11-20/20261120_163747-tdm-connection-health-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_163747-tdm-connection-health-evidence.md' |
| `docs/changelog/2026-11-20/20261120_170116-build-wait-wake-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_170116-build-wait-wake-review.md' |
| `docs/changelog/2026-11-20/20261120_170327-track-unresponsive-game-clients-in-connection-reliability-work.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_170327-track-unresponsive-game-clients-in-connection-reliability-work.md' |
| `docs/changelog/2026-11-20/20261120_171135-capture-unresponsive-tdm-client-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_171135-capture-unresponsive-tdm-client-evidence.md' |
| `docs/changelog/2026-11-20/20261120_172153-trace-tdm-join-and-visually-confirm-hung-clients.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_172153-trace-tdm-join-and-visually-confirm-hung-clients.md' |
| `docs/changelog/2026-11-20/20261120_172800-build-wait-wake-review-groundstate-compile-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_172800-build-wait-wake-review-groundstate-compile-failure.md' |
| `docs/changelog/2026-11-20/20261120_172803-review-over-budget-game-dll-build-wait-issue-40dfaa2a803c.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_172803-review-over-budget-game-dll-build-wait-issue-40dfaa2a803c.md' |
| `docs/changelog/2026-11-20/20261120_173728-trace-the-tdm-reconnect-packet-gap-and-visual-status-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_173728-trace-the-tdm-reconnect-packet-gap-and-visual-status-evidence.md' |
| `docs/changelog/2026-11-20/20261120_1745-reconnect-send-ack-trace.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_1745-reconnect-send-ack-trace.md' |
| `docs/changelog/2026-11-20/20261120_174957-ice-packet-receive-metrics.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_174957-ice-packet-receive-metrics.md' |
| `docs/changelog/2026-11-20/20261120_181000-reconnect-token-log-redaction.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_181000-reconnect-token-log-redaction.md' |
| `docs/changelog/2026-11-20/20261120_181507-investigate-arcade-capsule-surface-catching.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_181507-investigate-arcade-capsule-surface-catching.md' |
| `docs/changelog/2026-11-20/20261120_181800-reconnect-token-lookup-failure-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_181800-reconnect-token-lookup-failure-evidence.md' |
| `docs/changelog/2026-11-20/20261120_184600-reliable-event-roster-removal-diagnostic.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_184600-reliable-event-roster-removal-diagnostic.md' |
| `docs/changelog/2026-11-20/20261120_185318-fix-arcade-step-up-retries-to-preserve-capsule-sliding.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_185318-fix-arcade-step-up-retries-to-preserve-capsule-sliding.md' |
| `docs/changelog/2026-11-20/20261120_185500-visual-evidence-correlation-proposal.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_185500-visual-evidence-correlation-proposal.md' |
| `docs/changelog/2026-11-20/20261120_190000-current-dll-wait-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_190000-current-dll-wait-evidence.md' |
| `docs/changelog/2026-11-20/20261120_192707-publish-arcade-collision-build-and-diagnose-pinned-test-startup.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_192707-publish-arcade-collision-build-and-diagnose-pinned-test-startup.md' |
| `docs/changelog/2026-11-20/20261120_193000-reconnect-logger-startup-stall.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_193000-reconnect-logger-startup-stall.md' |
| `docs/changelog/2026-11-20/20261120_193739-log-retention-and-storage-efficiency-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_193739-log-retention-and-storage-efficiency-intake.md' |
| `docs/changelog/2026-11-20/20261120_193957-audit-arcade-capsule-paths-and-trace-pinned-run-startup-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_193957-audit-arcade-capsule-paths-and-trace-pinned-run-startup-failure.md' |
| `docs/changelog/2026-11-20/20261120_194700-recheck-arcade-capsule-slide-proof.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_194700-recheck-arcade-capsule-slide-proof.md' |
| `docs/coordination/decisions/20261120_165520-task-decision-b55d21cac5a5.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261120_165520-task-decision-b55d21cac5a5.md' |
| `docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-4e3ef676ba20.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-4e3ef676ba20.md' |
| `docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md' |
| `docs/issues/bug/20261120-repair-replay-export-and-build-the-local-video-e-issue-612ce59ed73d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-repair-replay-export-and-build-the-local-video-e-issue-612ce59ed73d.md' |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md' |
| `docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md' |
| `docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md' |
| `docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md' |
| `docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md' |
| `docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6bc05eadfb7f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6bc05eadfb7f.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-87f1017235e5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-87f1017235e5.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-c2cebb751fb4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-c2cebb751fb4.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-e84cfa28c7f7.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-e84cfa28c7f7.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md' |
| `docs/issues/capability/20261120-any-device-normalized-input-for-gameplay-and-cre-issue-22e043046cd0.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261120-any-device-normalized-input-for-gameplay-and-cre-issue-22e043046cd0.md' |
| `docs/issues/capability/20261120-decentralized-durable-world-sharing-and-creator--issue-2ecdfa48ec44.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261120-decentralized-durable-world-sharing-and-creator--issue-2ecdfa48ec44.md' |
| `docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md' |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-c23580c9fff1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-c23580c9fff1.md' |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md' |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md' |

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

- Owner: build-wait-reviewer
- Focus: build-friction
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 6d92afa35b434790e1b42ebb9667be69a216cde2 / 6d92afa35b434790e1b42ebb9667be69a216cde2
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "pending", "latest_build_stale": true, "latest_successful_build": "2026-11-20T19:55:20.256758Z", "latest_successful_build_id": "2026-11-20T19:55:20.256758Z:20be3bab4353981d41a818bcc2e8ecc77066633d9601db243cb15ecff172346c", "latest_successful_source_generation": 33}, "game_current": {"build_mode": "release", "code_hash": "ca0faa220bad6cb0474e6a676f6db489b75b0306b975ab68a448b8527c8733a4", "dll": "build\\game\\generation-411\\game-g411.dll", "generation": 411, "revision": "f75e8d2cbf72a725f8cd9e0876ff842b1f5622576c9a4389e4a78b70ddbb750b", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-411\\game-g411.dll", "generation": 411, "published_utc": "2026-11-20T20:09:36.848Z", "revision": "f75e8d2cbf72a725f8cd9e0876ff842b1f5622576c9a4389e4a78b70ddbb750b", "status": "ready"}, "progress": {"compiled": 1, "elapsed_s": 264.068, "phase": "build_complete", "result": "success", "skipped": 86, "updated_utc": "2026-11-20T19:55:14.214Z"}}
- Generation: 411
- Artifact: build\game\generation-411\game-g411.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_195841\server-01\events.jsonl
- Session ID: session-9461763073f8

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
M .opencode/plugins/mimita-user-context.ts
 M config/analytics.json
 M config/hot-reload/packet-ledger.json
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/build_friction.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/scenarios/user/dust2_ramp_collision_v1.json
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
 M docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md
 M docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-wake.md
 M docs/specs/dev-loop/multi-instance.md
 M docs/specs/world-projects/world-edit-commands.md
 M docs/specs/world-projects/world-projects.md
 M docs/workflows/issue-intake-and-triage.md
 M src/actor/player-ai.cpp
 M src/debug/structured-log.cpp
 M src/engine/engine-tick-combat.cpp
 M src/engine/engine-tick-ui-replay-hud.cpp
 M src/mapeditor/map-editor-projects.cpp
 M src/mapeditor/map-editor-projects.h
 M src/network/multiplayer-packets.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/reliable-gameplay-events.cpp
 M src/network/server-gamemode.cpp
 M src/network/server-packets.cpp
 M src/physics/movement/actor-final-pose.cpp
 M src/physics/movement/physics-collision-core.cpp
 M src/physics/movement/physics-collision-glb-main.cpp
 M src/physics/movement/physics-collision-glb-sweep-slide.cpp
?? .opencode/skills/
?? devscripts/scenarios/world_live_fork_v1.json
?? docs/changelog/2026-11-20/20261120_145200-end-to-end-consistency-investigation.md
?? docs/changelog/2026-11-20/20261120_150557-end-to-end-connection-reliability-issue-intake.md
?? docs/changelog/2026-11-20/20261120_151900-consistency-evidence-follow-up.md
?? docs/changelog/2026-11-20/20261120_152300-dashboard-responsiveness.md
?? docs/changelog/2026-11-20/20261120_152837-connection-reliability-evidence-continuation.md
?? docs/changelog/2026-11-20/20261120_155514-connection-reliability-evidence-continuation.md
?? docs/changelog/2026-11-20/20261120_155700-payload-cart-transform-diagnostics.md
?? docs/changelog/2026-11-20/20261120_160817-investigate-arcade-capsule-world-collision-response.md
?? docs/changelog/2026-11-20/20261120_162609-connection-hud-status-and-reliability-trace.md
?? docs/changelog/2026-11-20/20261120_163747-tdm-connection-health-evidence.md
?? docs/changelog/2026-11-20/20261120_170116-build-wait-wake-review.md
?? docs/changelog/2026-11-20/20261120_170327-track-unresponsive-game-clients-in-connection-reliability-work.md
?? docs/changelog/2026-11-20/20261120_171135-capture-unresponsive-tdm-client-evidence.md
?? docs/changelog/2026-11-20/20261120_172153-trace-tdm-join-and-visually-confirm-hung-clients.md
?? docs/changelog/2026-11-20/20261120_172800-build-wait-wake-review-groundstate-compile-failure.md
?? docs/changelog/2026-11-20/20261120_172803-review-over-budget-game-dll-build-wait-issue-40dfaa2a803c.md
?? docs/changelog/2026-11-20/20261120_173728-trace-the-tdm-reconnect-packet-gap-and-visual-status-evidence.md
?? docs/changelog/2026-11-20/20261120_1745-reconnect-send-ack-trace.md
?? docs/changelog/2026-11-20/20261120_174957-ice-packet-receive-metrics.md
?? docs/changelog/2026-11-20/20261120_181000-reconnect-token-log-redaction.md
?? docs/changelog/2026-11-20/20261120_181507-investigate-arcade-capsule-surface-catching.md
?? docs/changelog/2026-11-20/20261120_181800-reconnect-token-lookup-failure-evidence.md
?? docs/changelog/2026-11-20/20261120_184600-reliable-event-roster-removal-diagnostic.md
?? docs/changelog/2026-11-20/20261120_185318-fix-arcade-step-up-retries-to-preserve-capsule-sliding.md
?? docs/changelog/2026-11-20/20261120_185500-visual-evidence-correlation-proposal.md
?? docs/changelog/2026-11-20/20261120_190000-current-dll-wait-evidence.md
?? docs/changelog/2026-11-20/20261120_192707-publish-arcade-collision-build-and-diagnose-pinned-test-startup.md
?? docs/changelog/2026-11-20/20261120_193000-reconnect-logger-startup-stall.md
?? docs/changelog/2026-11-20/20261120_193739-log-retention-and-storage-efficiency-intake.md
?? docs/changelog/2026-11-20/20261120_193957-audit-arcade-capsule-paths-and-trace-pinned-run-startup-failure.md
?? docs/changelog/2026-11-20/20261120_194700-recheck-arcade-capsule-slide-proof.md
?? docs/coordination/decisions/20261120_165520-task-decision-b55d21cac5a5.md
?? docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-4e3ef676ba20.md
?? docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md
?? docs/issues/bug/20261120-repair-replay-export-and-build-the-local-video-e-issue-612ce59ed73d.md
?? docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
?? docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md
?? docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md
?? docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md
?? docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
?? docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
?? docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6bc05eadfb7f.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-87f1017235e5.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-c2cebb751fb4.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-e84cfa28c7f7.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
?? docs/issues/capability/20261120-any-device-normalized-input-for-gameplay-and-cre-issue-22e043046cd0.md
?? docs/issues/capability/20261120-decentralized-durable-world-sharing-and-creator--issue-2ecdfa48ec44.md
?? docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
?? docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-c23580c9fff1.md
?? docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
?? docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
```

## Captured diff stat

```text
.opencode/plugins/mimita-user-context.ts           | 117 +--
 config/analytics.json                              |   2 +-
 config/hot-reload/packet-ledger.json               |  14 +-
 devscripts/agent_coord.py                          |   9 +-
 devscripts/blender-ai                              |   0
 devscripts/build_friction.py                       |   4 +-
 devscripts/dev-loop.py                             |   6 +-
 devscripts/dev_loop/gui.py                         | 841 +++++++++++++++------
 devscripts/dev_loop/parallel_test_runs.py          |  28 +
 .../scenarios/user/dust2_ramp_collision_v1.json    |  10 +
 ...e-issues-preview-overlay-first-frame-context.md |  49 ++
 docs/issues/README.md                              |  99 ++-
 ...tency-limbs-only-capsule--issue-da337d1fe2f3.md | 133 ++++
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  55 ++
 ...lision-and-mace-active-po-issue-503f3e410c01.md |  20 +
 ...-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md |  33 +
 ...ocal-ai-player-controller-issue-f8873319f12b.md |  47 ++
 ...move-avoidable-build-and--issue-e044a931c54b.md |   6 +
 ...end-to-end-implementation-issue-565902eed8a7.md |  36 +
 ...nd-observe-context-bundle-issue-6907072bb155.md |  12 +
 ...le-restart-and-self-recov-issue-6fc8f853a674.md |   7 +
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  12 +
 ...ture-slices-through-proof-issue-848a5ba01470.md |  26 +
 ...the-user-adds-new-context-issue-a271b0296faa.md |  18 +
 docs/specs/dev-loop/agent-coordination.md          |   1 -
 docs/specs/dev-loop/agent-wake.md                  |   8 +
 docs/specs/dev-loop/multi-instance.md              |  22 +-
 docs/specs/world-projects/world-edit-commands.md   |   3 +-
 docs/specs/world-projects/world-projects.md        |  10 +
 docs/workflows/issue-intake-and-triage.md          |   8 +-
 src/actor/player-ai.cpp                            | 321 +++++++-
 src/debug/structured-log.cpp                       |  61 +-
 src/engine/engine-tick-combat.cpp                  |  19 +-
 src/engine/engine-tick-ui-replay-hud.cpp           |  24 +-
 src/mapeditor/map-editor-projects.cpp              |  90 ++-
 src/mapeditor/map-editor-projects.h                |   6 +
 src/network/multiplayer-packets.cpp                |  17 +-
 src/network/multiplayer-tick.cpp                   |   4 +
 src/network/reliable-gameplay-events.cpp           |  34 +
 src/network/server-gamemode.cpp                    | 104 ++-
 src/network/server-packets.cpp                     |  26 +-
 src/physics/movement/actor-final-pose.cpp          |  36 +
 src/physics/movement/physics-collision-core.cpp    |  15 +
 .../movement/physics-collision-glb-main.cpp        |  47 ++
 .../movement/physics-collision-glb-sweep-slide.cpp | 386 ++++++++--
 45 files changed, 2393 insertions(+), 433 deletions(-)
```

## Captured changed paths

```text
M	.opencode/plugins/mimita-user-context.ts
M	config/analytics.json
M	config/hot-reload/packet-ledger.json
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/build_friction.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/scenarios/user/dust2_ramp_collision_v1.json
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261120-profile-and-reduce-full-dev-loop-dashboard-redra-issue-dd1c5da31ecb.md
M	docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/feature/20261120-creator-workflow-end-to-end-implementation-issue-565902eed8a7.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261120-map-editor-feature-slices-through-proof-issue-848a5ba01470.md
M	docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-wake.md
M	docs/specs/dev-loop/multi-instance.md
M	docs/specs/world-projects/world-edit-commands.md
M	docs/specs/world-projects/world-projects.md
M	docs/workflows/issue-intake-and-triage.md
M	src/actor/player-ai.cpp
M	src/debug/structured-log.cpp
M	src/engine/engine-tick-combat.cpp
M	src/engine/engine-tick-ui-replay-hud.cpp
M	src/mapeditor/map-editor-projects.cpp
M	src/mapeditor/map-editor-projects.h
M	src/network/multiplayer-packets.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/reliable-gameplay-events.cpp
M	src/network/server-gamemode.cpp
M	src/network/server-packets.cpp
M	src/physics/movement/actor-final-pose.cpp
M	src/physics/movement/physics-collision-core.cpp
M	src/physics/movement/physics-collision-glb-main.cpp
M	src/physics/movement/physics-collision-glb-sweep-slide.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "pending",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-20T19:55:20.256758Z",
    "latest_successful_build_id": "2026-11-20T19:55:20.256758Z:20be3bab4353981d41a818bcc2e8ecc77066633d9601db243cb15ecff172346c",
    "latest_successful_source_generation": 33
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "ca0faa220bad6cb0474e6a676f6db489b75b0306b975ab68a448b8527c8733a4",
    "dll": "build\\game\\generation-411\\game-g411.dll",
    "generation": 411,
    "revision": "f75e8d2cbf72a725f8cd9e0876ff842b1f5622576c9a4389e4a78b70ddbb750b",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-411\\game-g411.dll",
    "generation": 411,
    "published_utc": "2026-11-20T20:09:36.848Z",
    "revision": "f75e8d2cbf72a725f8cd9e0876ff842b1f5622576c9a4389e4a78b70ddbb750b",
    "status": "ready"
  },
  "progress": {
    "compiled": 1,
    "elapsed_s": 264.068,
    "phase": "build_complete",
    "result": "success",
    "skipped": 86,
    "updated_utc": "2026-11-20T19:55:14.214Z"
  }
}
```
