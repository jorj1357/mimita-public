// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Root dev-loop no-op timing and fail-open checkpoint

# Task

- Task ID: work-690eaa6d7614
- Session ID: work-690eaa6d7614
- Owner: cpp-latency-pressure-profile
- Focus: issue-136d4553ff92
- Summary: Measured a root dev-loop warm EXE no-op end to end, persisted its internal worker stages, and recorded the still-unattributed request overhead. Confirmed a failed optional presentation DLL stays cached and does not fail the successful EXE no-op. Kept the latency goal active and recorded the next instrumentation target.
- Status: in-progress
- Date, time, timezone: 2026-11-22T08:16:39+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca
- Final commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

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
| `build.py` |  |  |  | git checkout -- build.py |
| `build_game_dll.py` |  |  |  | git checkout -- build_game_dll.py |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/build.py` |  |  |  | git checkout -- devscripts/dev_loop/build.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/packet_registry_codegen.py` |  |  |  | git checkout -- devscripts/packet_registry_codegen.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `devscripts/source_manifest.py` |  |  |  | git checkout -- devscripts/source_manifest.py |
| `docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md` |  |  |  | git checkout -- docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md |
| `docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md` |  |  |  | git checkout -- docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md |
| `docs/gold/20261030_033224-agent-session-handoff-gold.md` |  |  |  | git checkout -- docs/gold/20261030_033224-agent-session-handoff-gold.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md` |  |  |  | git checkout -- docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md |
| `docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md` |  |  |  | git checkout -- docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | git checkout -- docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md` |  |  |  | git checkout -- docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md |
| `docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md |
| `docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md |
| `docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md |
| `docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md |
| `src/gui/ui-system-buttons.cpp` |  |  |  | git checkout -- src/gui/ui-system-buttons.cpp |
| `src/gui/ui-system-internal.h` |  |  |  | git checkout -- src/gui/ui-system-internal.h |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-text-input.cpp` |  |  |  | git checkout -- src/gui/ui-text-input.cpp |
| `src/main-systems.cpp` |  |  |  | git checkout -- src/main-systems.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/server-npcs.cpp` |  |  |  | git checkout -- src/network/server-npcs.cpp |
| `src/npc/npc-behavior-module.cpp` |  |  |  | git checkout -- src/npc/npc-behavior-module.cpp |
| `src/npc/npc-navigator.cpp` |  |  |  | git checkout -- src/npc/npc-navigator.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `src/npc/npc.h` |  |  |  | git checkout -- src/npc/npc.h |
| `brain.db` |  |  |  | Remove-Item -LiteralPath 'brain.db' |
| `devscripts/quality/test_dev_loop_exe_noop_identity.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_dev_loop_exe_noop_identity.py' |
| `devscripts/quality/test_packet_registry_codegen.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_packet_registry_codegen.py' |
| `devscripts/quality/test_source_manifest_cache.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_source_manifest_cache.py' |
| `docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md' |
| `docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md' |
| `docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md' |
| `docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md' |
| `docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md' |
| `docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_043800-fcs-npc-objective-route-recovery.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043800-fcs-npc-objective-route-recovery.md' |
| `docs/changelog/2026-11-22/20261122_043900-issue-266f4fcaf861-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043900-issue-266f4fcaf861-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_044500-issue-ce4c245a8a70-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_044500-issue-ce4c245a8a70-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_052200-ctx-fc9b2656cbcd-failopen-build-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_052200-ctx-fc9b2656cbcd-failopen-build-review.md' |
| `docs/changelog/2026-11-22/20261122_052200-issue-52b59bdd440c-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_052200-issue-52b59bdd440c-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_052200-long-running-goal-and-fail-open-diagnostics-gold.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_052200-long-running-goal-and-fail-open-diagnostics-gold.md' |
| `docs/changelog/2026-11-22/20261122_054942-issue-f8873319f12b-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_054942-issue-f8873319f12b-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_060000-fcs-room-rejoin-health-rebase-runtime-proof.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_060000-fcs-room-rejoin-health-rebase-runtime-proof.md' |
| `docs/changelog/2026-11-22/20261122_062200-issue-7e086ec27372-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_062200-issue-7e086ec27372-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_062509-issue-6d795125c36f-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_062509-issue-6d795125c36f-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_063500-persistent-manifest-cache-and-noop-measurements.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_063500-persistent-manifest-cache-and-noop-measurements.md' |
| `docs/changelog/2026-11-22/20261122_065100-issue-a6a32cacba8c-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_065100-issue-a6a32cacba8c-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_065500-issue-89efbbc86535-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_065500-issue-89efbbc86535-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_070217-fun-counter-strike-npc-objective-and-corner-recovery.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_070217-fun-counter-strike-npc-objective-and-corner-recovery.md' |
| `docs/changelog/2026-11-22/20261122_070248-build-wait-reviewer-overbudget-game-dll-wait.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_070248-build-wait-reviewer-overbudget-game-dll-wait.md' |
| `docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md' |
| `docs/changelog/2026-11-22/20261122_073556-issue-549680e53b26-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_073556-issue-549680e53b26-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_074200-issue-34969db31bf6-exe-build-setup-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_074200-issue-34969db31bf6-exe-build-setup-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_080200-issue-37dde12f63ff-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_080200-issue-37dde12f63ff-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_080900-issue-b24f9f79794d-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_080900-issue-b24f9f79794d-build-wait-review.md' |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-0498cbef4ef3.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-0498cbef4ef3.md' |
| `docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md' |
| `docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-727472d32238.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-727472d32238.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-34969db31bf6.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-34969db31bf6.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00e2e9243beb.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00e2e9243beb.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-1052cb7f28c4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-1052cb7f28c4.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-18050cc9b42e.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-18050cc9b42e.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2f6d585239f8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2f6d585239f8.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-37dde12f63ff.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-37dde12f63ff.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-467dc2860040.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-467dc2860040.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4fe1a7a2a4bf.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4fe1a7a2a4bf.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-61edee02549f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-61edee02549f.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6b55a3773999.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6b55a3773999.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6d795125c36f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6d795125c36f.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-7e086ec27372.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-7e086ec27372.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-86b3b2ce1e73.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-86b3b2ce1e73.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9c77739954f9.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9c77739954f9.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a6a32cacba8c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a6a32cacba8c.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a7464f307b2f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a7464f307b2f.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-b24f9f79794d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-b24f9f79794d.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bccd816a3abd.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bccd816a3abd.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c2346d3bbc82.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c2346d3bbc82.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c481f05923e4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c481f05923e4.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c845e5ba04db.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c845e5ba04db.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec91f5dc7366.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec91f5dc7366.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md' |

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

- Owner: cpp-latency-pressure-profile
- Focus: issue-136d4553ff92
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 9fd637d595a308483a9f82c88349a5915c82a0ca / 9fd637d595a308483a9f82c88349a5915c82a0ca
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-22T08:00:31.376619Z", "latest_successful_build_id": "2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "369a206bc5df5b256e552aacf1e853c0aa65309bc4a9c1437e4b8afb51dc1355", "dll": "build\\game\\generation-478\\game-g478.dll", "generation": 478, "revision": "16826795c09bcdea5dfac1470f132d4276cce7bda172938d1d559a213940b8b4", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-478\\game-g478.dll", "generation": 478, "published_utc": "2026-11-22T08:13:01.503Z", "revision": "16826795c09bcdea5dfac1470f132d4276cce7bda172938d1d559a213940b8b4", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 0.81, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T08:14:11.304Z"}}
- Generation: 478
- Artifact: build\game\generation-478\game-g478.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_081330_fcs-route-proof-applied-fallback\client-01\events.jsonl
- Session ID: work-690eaa6d7614

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
M build.py
 M build_game_dll.py
 M config/accounts/default.json
 M config/analytics.json
 M config/hot-reload/packet-ledger.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/build.py
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/packet_registry_codegen.py
 M devscripts/quality/test_game_build_snapshot.py
 M devscripts/source_manifest.py
 M docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
 M docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
 M docs/gold/20261030_033224-agent-session-handoff-gold.md
 M docs/issues/README.md
 M docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
 M docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md
 M docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system-internal.h
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/main-systems.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/server-npcs.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc-navigator.cpp
 M src/npc/npc.cpp
 M src/npc/npc.h
?? brain.db
?? devscripts/quality/test_dev_loop_exe_noop_identity.py
?? devscripts/quality/test_packet_registry_codegen.py
?? devscripts/quality/test_source_manifest_cache.py
?? docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md
?? docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md
?? docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md
?? docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md
?? docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md
?? docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md
?? docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md
?? docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md
?? docs/changelog/2026-11-22/20261122_043800-fcs-npc-objective-route-recovery.md
?? docs/changelog/2026-11-22/20261122_043900-issue-266f4fcaf861-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_044500-issue-ce4c245a8a70-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_052200-ctx-fc9b2656cbcd-failopen-build-review.md
?? docs/changelog/2026-11-22/20261122_052200-issue-52b59bdd440c-independent-review.md
?? docs/changelog/2026-11-22/20261122_052200-long-running-goal-and-fail-open-diagnostics-gold.md
?? docs/changelog/2026-11-22/20261122_054942-issue-f8873319f12b-independent-review.md
?? docs/changelog/2026-11-22/20261122_060000-fcs-room-rejoin-health-rebase-runtime-proof.md
?? docs/changelog/2026-11-22/20261122_062200-issue-7e086ec27372-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_062509-issue-6d795125c36f-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_063500-persistent-manifest-cache-and-noop-measurements.md
?? docs/changelog/2026-11-22/20261122_065100-issue-a6a32cacba8c-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_065500-issue-89efbbc86535-independent-review.md
?? docs/changelog/2026-11-22/20261122_070217-fun-counter-strike-npc-objective-and-corner-recovery.md
?? docs/changelog/2026-11-22/20261122_070248-build-wait-reviewer-overbudget-game-dll-wait.md
?? docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md
?? docs/changelog/2026-11-22/20261122_073556-issue-549680e53b26-independent-review.md
?? docs/changelog/2026-11-22/20261122_074200-issue-34969db31bf6-exe-build-setup-wait-review.md
?? docs/changelog/2026-11-22/20261122_080200-issue-37dde12f63ff-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_080900-issue-b24f9f79794d-build-wait-review.md
?? docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-0498cbef4ef3.md
?? docs/issues/bug/20261122-root-dev-loop-drops-runtime-journal-events-wait--issue-6e2d7b91e7d9.md
?? docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-727472d32238.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-34969db31bf6.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00e2e9243beb.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-1052cb7f28c4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-18050cc9b42e.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2f6d585239f8.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-37dde12f63ff.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-467dc2860040.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4fe1a7a2a4bf.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-61edee02549f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6b55a3773999.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6d795125c36f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-7e086ec27372.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-86b3b2ce1e73.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9c77739954f9.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a6a32cacba8c.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a7464f307b2f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-b24f9f79794d.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bccd816a3abd.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c2346d3bbc82.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c481f05923e4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-c845e5ba04db.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec91f5dc7366.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md
```

## Captured diff stat

```text
build.py                                           | 163 +++++++++++++--
 build_game_dll.py                                  |   7 +-
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   4 +-
 config/hot-reload/packet-ledger.json               | 160 +++++++--------
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |   6 +
 devscripts/dev_loop/build.py                       | 190 +++++++++++++++++-
 devscripts/dev_loop/game_build.py                  |  97 +++++++--
 devscripts/game_build_worker.py                    | 144 ++++++++++---
 devscripts/packet_registry_codegen.py              | 164 ++++++++++++---
 devscripts/quality/test_game_build_snapshot.py     | 110 ++++++++++
 devscripts/source_manifest.py                      | 130 +++++++++++-
 ...ld-wait-reviewer-fcs-npc-kill-plant-evidence.md |  43 +++-
 ...-autonomous-gameplay-investigation-and-proof.md |   6 +
 .../20261030_033224-agent-session-handoff-gold.md  |  33 +++
 docs/issues/README.md                              |  82 ++++++--
 ...ntology-for-project-and-a-issue-75220121d993.md |  40 ++++
 ...lity-and-behavior-behind--issue-82cdc1277704.md |  37 ++++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |  35 ++++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  96 +++++++++
 ...ocal-ai-player-controller-issue-f8873319f12b.md |  36 ++++
 ...move-avoidable-build-and--issue-e044a931c54b.md |   6 +
 ...budget-exe-build-at-setup-issue-8aae0621645f.md |  63 ++++++
 ...e-dll-build-at-game-build-issue-6bcfb6252dfa.md |  25 +++
 ...e-dll-build-at-game-build-issue-a59a0058df76.md |  28 +++
 ...e-dll-build-at-game-build-issue-ec19949cbd21.md |  45 +++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  72 +++++++
 ...nd-observe-context-bundle-issue-6907072bb155.md |   6 +
 ...s-to-the-latest-file-atom-issue-52b59bdd440c.md |  38 ++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  32 +++
 ...-a-specific-tick-for-proo-issue-89efbbc86535.md |  38 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  81 ++++++++
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  16 ++
 ...the-user-adds-new-context-issue-a271b0296faa.md |   6 +
 ...-and-build-to-proof-links-issue-b396205aaecf.md |   6 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 223 ++++++++++++++++++++-
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |   6 +
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  14 ++
 ...ss-agent-interference-in--issue-c4f2df8edeba.md |  14 ++
 ...te-special-contact-paths--issue-49ded3edbe47.md |  39 ++++
 src/gui/ui-system-buttons.cpp                      |   4 +
 src/gui/ui-system-internal.h                       |   1 +
 src/gui/ui-system.cpp                              |  38 +++-
 src/gui/ui-text-input.cpp                          |   2 +
 src/main-systems.cpp                               |  20 +-
 src/network/multiplayer-tick.cpp                   |   8 +-
 src/network/server-npcs.cpp                        |   1 -
 src/npc/npc-behavior-module.cpp                    |   2 +
 src/npc/npc-navigator.cpp                          |  61 +++++-
 src/npc/npc.cpp                                    | 157 +++++++++++----
 src/npc/npc.h                                      |   1 -
 52 files changed, 2366 insertions(+), 272 deletions(-)
```

## Captured changed paths

```text
M	build.py
M	build_game_dll.py
M	config/accounts/default.json
M	config/analytics.json
M	config/hot-reload/packet-ledger.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/build.py
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/packet_registry_codegen.py
M	devscripts/quality/test_game_build_snapshot.py
M	devscripts/source_manifest.py
M	docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
M	docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
M	docs/gold/20261030_033224-agent-session-handoff-gold.md
M	docs/issues/README.md
M	docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
M	docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/improvement/20261121-track-and-eliminate-cross-agent-interference-in--issue-c4f2df8edeba.md
M	docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
M	src/gui/ui-system-buttons.cpp
M	src/gui/ui-system-internal.h
M	src/gui/ui-system.cpp
M	src/gui/ui-text-input.cpp
M	src/main-systems.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/server-npcs.cpp
M	src/npc/npc-behavior-module.cpp
M	src/npc/npc-navigator.cpp
M	src/npc/npc.cpp
M	src/npc/npc.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-22T08:00:31.376619Z",
    "latest_successful_build_id": "2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "369a206bc5df5b256e552aacf1e853c0aa65309bc4a9c1437e4b8afb51dc1355",
    "dll": "build\\game\\generation-478\\game-g478.dll",
    "generation": 478,
    "revision": "16826795c09bcdea5dfac1470f132d4276cce7bda172938d1d559a213940b8b4",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-478\\game-g478.dll",
    "generation": 478,
    "published_utc": "2026-11-22T08:13:01.503Z",
    "revision": "16826795c09bcdea5dfac1470f132d4276cce7bda172938d1d559a213940b8b4",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 0.81,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-22T08:14:11.304Z"
  }
}
```

## Follow-up activation: request visibility, report-only failures, and recovery

- Added a CLI wait breakdown to `devscripts/dev-loop.py`: total wait, state-file read time, sleep time, poll count, and terminal status. Polling is 100 ms rather than 500 ms.
- A first attempt to publish build status before optional reports deadlocked because `save_state()` reacquired the dev-loop's non-reentrant lock. The 90-second waiter timed out while the completed no-op remained marked `building`. This was corrected by moving the early `save_state()` call outside the lock. After the stuck daemon could not accept the stop request, only its checkout-owned PID 20556 was stopped; the existing watchdog launched daemon PID 34076. Server PID 26748 survived, and no client or extra game process was launched by this task.
- The corrected path publishes primary build success before report-only scans run. Each scan has a four-second timeout and its outcome/duration is recorded separately. A timed-out report-only scan is fail-open and does not change the successful EXE result.

### Follow-up measurements

| Request | Result | End-to-end | Worker / wait detail |
|---|---|---:|---|
| `2026-11-22T08:32:29.828Z-55caf2f5` | EXE no-op succeeded; 0 compiled, 87 skipped. Optional presentation DLL failed with exit 1 after 21.616s. | 27.573s | Worker 25.571s; CLI observed success about 0.4s after worker completion. |
| `2026-11-22T08:33:26.342Z-1a4f5b97` | EXE no-op succeeded; 0 compiled, 87 skipped. Optional DLL failure was `cached_failure_skip` at 0s. | 6.026s | Worker 4.123s; CLI wait 4.547s, with 0.203s reading state and 4.344s sleeping over 44 polls. |

The second request's worker stages were source identity 0.042s, setup 0.331s, build script 3.609s, post-processing 0.139s, artifact validation 0.053s, and no publication. Worker attribution reconciled to zero. The build driver's own console said `Time: 0.89 sec`, while `build/build-timing.json` recorded 2.826s; that internal timing discrepancy remains unresolved. The CLI request fell from the prior measured 10.148s no-op sample to 6.026s (40.6% lower), but edited-source activation and the DLL path remain unproven.

The optional `exe_cutover_gate.py --report-only` took 0.566s. `cap_render_gate.py --report-only` hit its four-second timeout and was recorded as `failed_open`; the primary EXE request had already returned success. This confirms failure containment, while also showing that optional report work still occupies the coordinator after the primary result and needs a later scheduling/caching decision.

### Verification

- `python -m py_compile devscripts/dev-loop.py` passed after the lock-placement correction.
- `git diff --check -- devscripts/dev-loop.py` passed.
- Two root dev-loop EXE requests completed successfully; each reported 0 compiled and 87 skipped. The unchanged retry confirmed the optional presentation-DLL failure cache.
- The live server remained PID 26748. No runtime reload or gameplay activation was attempted, so this is build/request timing evidence only.

### Next checkpoint

- Reconcile the build driver's printed 0.89s with its 2.826s timing JSON and the 3.609s subprocess wall.
- Continue the changed-one-translation-unit DLL path, preserving the root build authority and requiring a matching runtime journal before claiming live activation.
- Keep this latency goal active; the measured 6.026s is a warm EXE no-op, not the requested edit-to-live acceptance.
