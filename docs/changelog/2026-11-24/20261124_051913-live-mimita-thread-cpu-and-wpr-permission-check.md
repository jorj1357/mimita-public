// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Live MiMITA Thread CPU and WPR Permission Check

# Task

- Task ID: session-0534d04adc60
- Session ID: session-0534d04adc60
- Owner: resource-profiler
- Focus: performance
- Summary: Captured a bounded per-thread CPU sample for the live Juggernaut server and an unrelated Bloodtest client, then tried WPR from regular and RunAs shells. WPR never recorded because the profiling privilege was disabled; no ETL was produced.
- Status: in-progress
- Date, time, timezone: 2026-11-24T05:19:11+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

# Pre-existing changes

- Exact status output: The shared checkout contains extensive concurrent changes. This session changed only the CPU issue and added its sample CSV and this changelog; no source, runtime, or build files were changed.
- Files not created or modified by this session: The other paths in the generated workspace status inventory below are pre-existing or concurrent work and were left untouched.

# Requested behavior

- Continue the resource profiling goal by narrowing CPU ownership without disturbing the shared runtime, then use function-level evidence to choose optimization candidates.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`; `docs/specs/dev-loop/agent-coordination.md`; `docs/specs/dev-loop/build-time-observability.md`.
- Exact requirements: Record UTC interval, CPU normalization per logical CPU and whole host, process/workload identity with unknowns, sample artifact, and avoid claiming build/runtime proof outside the sampled window.
- Why the change follows the specification: The sample labels two unrelated processes separately, records host saturation and RAM pressure, identifies hot thread IDs without pretending they are function names, and captures WPR's exact denial.
- Conflicts or decisions: WPR could not collect stacks because Windows denied `SeSystemProfilePrivilege` (error `0xc5585011`). The shared root server has no connected client; the separate Bloodtest client is not its pair. A high-thread CPU sample does not prove source-level cost.

# Exact implementation changes

## File: `.dev/perf-captures/cpu-threads-live-20261124T0508Z.csv`

- Lines/functions/headings: 768 process/thread rows across12 intervals; capture wall-clock elapsed17.294s.
- Old content or behavior: No prior artifact; the CSV was newly created.
- New content or behavior: Host CPU mean99.99%, RAM-use mean95.79%, free RAM minimum526.9MiB. Server PID33892 averaged211.76% one core /17.65% host; unrelated Bloodtest client PID18752 averaged129.80% one core /10.82% host. Highest sampled TIDs were server24268 at8.219 CPU seconds and client34244 at10.594 CPU seconds.
- Reason: Narrow the busiest execution threads before obtaining stack samples.
- Why unrelated behavior is preserved: Read-only process/thread CPU and memory counters only; no runtime commands, priorities, build, or configuration changed.

## File: `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`

- Lines/functions/headings: Appended `Live thread CPU sample and stack-profile blocker — 2026-11-24`.
- Old content or behavior: No existing issue text removed or rewritten.
- New content or behavior: Added sample interval, host/process normalization, separate process identities, top thread CPU seconds, workload/journal limitations, and WPR error `0xc5585011`.
- Reason: Preserve actionable evidence and make the next profile step explicit.
- Why unrelated behavior is preserved: Append-only issue update; no source code changed.

## WPR stack-profile attempt

- Command: `wpr -start CPU -filemode`.
- Result: Initial attempt failed before recording with “Failed to enable the policy to profile system performance” (`0xc5585011`). A user-authorized `Start-Process -Verb RunAs` attempt spawned PowerShell PID27240 and WPR PID31600; those remained stuck while `wpr -status` said not recording. A separate RunAs `whoami /priv` check reported `SeSystemProfilePrivilege Disabled`. I stopped only my own exact wrapper/WPR processes. The ETL appeared after the stop: it is 36,700,160 bytes, spans05:22:32.259515Z–05:24:16.263949Z (104.004s), and has zero lost events, but `xperf -a profile -detail` reports no sampled profile data. Thus the capture is valid as a trace container but contains no CPU profile stacks; the attempt also exceeded the intended five seconds. Filename timestamp says05:15Z but actual start was05:22Z. WPR is now idle. Free disk fell from about4.45GB to3.34GB during the interval, but causation is unknown because other activity was ongoing.
- Next: Verify a visibly elevated Administrator PowerShell token and confirm `SeSystemProfilePrivilege` is available before another bounded trace. Inspect Local Security Policy only if it remains unavailable; do not change rights or policy without review.

- Follow-up: A visible RunAs PowerShell window (PID21196; title `Administrator: Windows PowerShell 5.1`) was launched, but the supported Computer Use inventory could not enumerate it. I closed only this session-created shell; WPR remains idle. No commands were entered in it and no policy changed.

## File: `.dev/perf-captures/wpr-privilege-check-20261124T0524Z.txt`

- Lines/functions/headings: One captured line: `SeSystemProfilePrivilege Profile system performance Disabled`.
- Old content or behavior: No prior file; this diagnostic output was newly created.
- New content or behavior: Records only the profile-system-performance privilege name and token status; it contains no account/group listing.
- Reason: Determine why the elevated WPR attempt could not start without collecting unrelated account information.
- Why unrelated behavior is preserved: Read-only token inspection; no policy or membership was changed.

## File: `.dev/perf-captures/mimita-cpu-stacks-20261124T0515Z.etl`

- Lines/functions/headings: Trace covers05:22:32.259515Z–05:24:16.263949Z; 36,700,160 bytes.
- Old content or behavior: No prior file; trace appeared after WPR stopped.
- New content or behavior: `xperf -a tracestats` validated the ETL with zero lost buffers/events; `xperf -a profile -detail` reported no sampled profile data.
- Reason: Verify whether the attempted WPR capture can support CPU callstack attribution.
- Why unrelated behavior is preserved: The trace is read-only evidence and no runtime or system policy was changed.

## Validation and limitations

- Read-only check: dev-loop published generation581 and no active build; WPR was idle before and after the failed capture.
- No tests/builds were requested. The changelog gate should be run after editing.
- The root server journal showed nearby NPC movement/combat events after the sample, but no exact in-window workload summary; do not assert a live NPC count from this sample.
- The exact permission-check output file contains only the privilege name/status; it is not a full user/group inventory.
- The later ETL check corrected the earlier no-file status: an ETL was produced, but it spans104s and has no sampled profile data. It cannot support function-level CPU attribution.
- 240-FPS client frame times, continuous 60-Hz server ticks, gameplay correctness, GPU usage, disk/network throughput, and function-level CPU shares remain unverified.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/maps/dust2cyberiav4.json` |  |  |  | git checkout -- config/maps/dust2cyberiav4.json |
| `config/movement/movement-slowdm.json` |  |  |  | git checkout -- config/movement/movement-slowdm.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/postfx.json` |  |  |  | git checkout -- config/postfx.json |
| `config/tips.json` |  |  |  | git checkout -- config/tips.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/features/npc-behavior.md` |  |  |  | git checkout -- docs/features/npc-behavior.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md` |  |  |  | git checkout -- docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md` |  |  |  | git checkout -- docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md |
| `docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` |  |  |  | git checkout -- docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md |
| `docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md` |  |  |  | git checkout -- docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md |
| `docs/issues/improvement/20261123-complete-the-bloodtest-bodycam-presentation-spec-issue-e9f582f8d2d0.md` |  |  |  | git checkout -- docs/issues/improvement/20261123-complete-the-bloodtest-bodycam-presentation-spec-issue-e9f582f8d2d0.md |
| `docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md` |  |  |  | git checkout -- docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md |
| `docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md` |  |  |  | git checkout -- docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md |
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/operations/devscripts/script-inventory.md` |  |  |  | git checkout -- docs/operations/devscripts/script-inventory.md |
| `docs/specs/dev-loop/agent-proof-and-verification.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-proof-and-verification.md |
| `docs/specs/dev-loop/multi-instance.md` |  |  |  | git checkout -- docs/specs/dev-loop/multi-instance.md |
| `docs/specs/dev-loop/visual-proof-screenshots.md` |  |  |  | git checkout -- docs/specs/dev-loop/visual-proof-screenshots.md |
| `docs/specs/gamemodes/bodycam.md` |  |  |  | git checkout -- docs/specs/gamemodes/bodycam.md |
| `docs/specs/graphics/graphics-post-processing.md` |  |  |  | git checkout -- docs/specs/graphics/graphics-post-processing.md |
| `docs/workflows/runtime-scenario-validation.md` |  |  |  | git checkout -- docs/workflows/runtime-scenario-validation.md |
| `src/config/movement-config.cpp` |  |  |  | git checkout -- src/config/movement-config.cpp |
| `src/engine/engine-tick-net.cpp` |  |  |  | git checkout -- src/engine/engine-tick-net.cpp |
| `src/engine/engine-tick-ui-game-hud.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui-game-hud.cpp |
| `src/engine/engine-tick-ui.cpp` |  |  |  | git checkout -- src/engine/engine-tick-ui.cpp |
| `src/engine/engine-tick-ui.h` |  |  |  | git checkout -- src/engine/engine-tick-ui.h |
| `src/engine/engine-tick.cpp` |  |  |  | git checkout -- src/engine/engine-tick.cpp |
| `src/entities/aimbody-config.h` |  |  |  | git checkout -- src/entities/aimbody-config.h |
| `src/entities/player.h` |  |  |  | git checkout -- src/entities/player.h |
| `src/gamemode/gamemode-runtime-resolver.cpp` |  |  |  | git checkout -- src/gamemode/gamemode-runtime-resolver.cpp |
| `src/gamemode/gamemode.cpp` |  |  |  | git checkout -- src/gamemode/gamemode.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `src/gui/gui-main.cpp` |  |  |  | git checkout -- src/gui/gui-main.cpp |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-system.h` |  |  |  | git checkout -- src/gui/ui-system.h |
| `src/input/input-commands.cpp` |  |  |  | git checkout -- src/input/input-commands.cpp |
| `src/input/input-frame.h` |  |  |  | git checkout -- src/input/input-frame.h |
| `src/input/input-poll.cpp` |  |  |  | git checkout -- src/input/input-poll.cpp |
| `src/input/input-state.h` |  |  |  | git checkout -- src/input/input-state.h |
| `src/network/community-match-client.cpp` |  |  |  | git checkout -- src/network/community-match-client.cpp |
| `src/network/movement-validation.h` |  |  |  | git checkout -- src/network/movement-validation.h |
| `src/network/multiplayer-context.h` |  |  |  | git checkout -- src/network/multiplayer-context.h |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/multiplayer-reconcile.cpp` |  |  |  | git checkout -- src/network/multiplayer-reconcile.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/network/schema/packet-common.h` |  |  |  | git checkout -- src/network/schema/packet-common.h |
| `src/network/server-gamemode.cpp` |  |  |  | git checkout -- src/network/server-gamemode.cpp |
| `src/network/server-packets.cpp` |  |  |  | git checkout -- src/network/server-packets.cpp |
| `src/network/server-players.cpp` |  |  |  | git checkout -- src/network/server-players.cpp |
| `src/network/server.h` |  |  |  | git checkout -- src/network/server.h |
| `src/physics/movement/movement-conversion.cpp` |  |  |  | git checkout -- src/physics/movement/movement-conversion.cpp |
| `src/physics/movement/movement-step.cpp` |  |  |  | git checkout -- src/physics/movement/movement-step.cpp |
| `src/physics/movement/movement-step.h` |  |  |  | git checkout -- src/physics/movement/movement-step.h |
| `src/physics/movement/movement-types.h` |  |  |  | git checkout -- src/physics/movement/movement-types.h |
| `src/ragdoll/ragdoll-mode.cpp` |  |  |  | git checkout -- src/ragdoll/ragdoll-mode.cpp |
| `src/render/post-fx.cpp` |  |  |  | git checkout -- src/render/post-fx.cpp |
| `src/render/post-fx.h` |  |  |  | git checkout -- src/render/post-fx.h |
| `src/sim/simulate-tick.cpp` |  |  |  | git checkout -- src/sim/simulate-tick.cpp |
| `devscripts/bloodtest-quick.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/bloodtest-quick.py' |
| `devscripts/evidence-watch.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/evidence-watch.py' |
| `devscripts/quality/test_dev_loop_cli_run_identity.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_dev_loop_cli_run_identity.py' |
| `docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md' |
| `docs/changelog/2026-11-24/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-24/' |
| `docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md` |  |  |  | Remove-Item -LiteralPath 'docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md' |
| `docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md` |  |  |  | Remove-Item -LiteralPath 'docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md' |
| `docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md' |
| `docs/handoffs/2026-11-24-live-evidence-snapshot-time-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/2026-11-24-live-evidence-snapshot-time-handoff.md' |
| `docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md' |
| `docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md' |
| `docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md' |
| `docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md' |
| `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md' |
| `docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md' |
| `docs/specs/dev-loop/live-evidence-observer.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/live-evidence-observer.md' |
| `docs/specs/simulation/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/simulation/' |

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
- Focus: performance
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T03:30:52.372066Z", "latest_successful_build_id": "2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6", "latest_successful_source_generation": 20}, "game_current": {"build_mode": "debug", "code_hash": "0e7ff7a9733ced73fc1f53610cfd4e5639af1feaa60c840eae7651ff10ed2e35", "dll": "build\\game\\generation-581\\game-g581.dll", "generation": 581, "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-581\\game-g581.dll", "generation": 581, "published_utc": "2026-11-24T04:35:40.515Z", "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 59.525, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-24T03:30:44.219Z"}}
- Generation: 581
- Artifact: build\game\generation-581\game-g581.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_022440_01\server-01\events.jsonl
- Session ID: session-0534d04adc60

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
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/hot-reload/packet-ledger.json
 M config/maps/dust2cyberiav4.json
 M config/movement/movement-slowdm.json
 M config/movement/movement-source.json
 M config/postfx.json
 M config/tips.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/parallel_test_runs.py
 M docs/ROUTER.md
 M docs/features/npc-behavior.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
 M docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
 M docs/issues/improvement/20261123-complete-the-bloodtest-bodycam-presentation-spec-issue-e9f582f8d2d0.md
 M docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
 M docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/operations/devscripts/script-inventory.md
 M docs/specs/dev-loop/agent-proof-and-verification.md
 M docs/specs/dev-loop/multi-instance.md
 M docs/specs/dev-loop/visual-proof-screenshots.md
 M docs/specs/gamemodes/bodycam.md
 M docs/specs/graphics/graphics-post-processing.md
 M docs/workflows/runtime-scenario-validation.md
 M src/config/movement-config.cpp
 M src/engine/engine-tick-net.cpp
 M src/engine/engine-tick-ui-game-hud.cpp
 M src/engine/engine-tick-ui.cpp
 M src/engine/engine-tick-ui.h
 M src/engine/engine-tick.cpp
 M src/entities/aimbody-config.h
 M src/entities/player.h
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/gui/gui-main.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-system.h
 M src/input/input-commands.cpp
 M src/input/input-frame.h
 M src/input/input-poll.cpp
 M src/input/input-state.h
 M src/network/community-match-client.cpp
 M src/network/movement-validation.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/network/multiplayer-reconcile.cpp
 M src/network/multiplayer-tick.cpp
 M src/network/schema/packet-common.h
 M src/network/server-gamemode.cpp
 M src/network/server-packets.cpp
 M src/network/server-players.cpp
 M src/network/server.h
 M src/physics/movement/movement-conversion.cpp
 M src/physics/movement/movement-step.cpp
 M src/physics/movement/movement-step.h
 M src/physics/movement/movement-types.h
 M src/ragdoll/ragdoll-mode.cpp
 M src/render/post-fx.cpp
 M src/render/post-fx.h
 M src/sim/simulate-tick.cpp
?? devscripts/bloodtest-quick.py
?? devscripts/evidence-watch.py
?? devscripts/quality/test_dev_loop_cli_run_identity.py
?? docs/changelog/2026-11-23/20261123_2350-one-brain-emergent-fields-reference.md
?? docs/changelog/2026-11-24/
?? docs/coordination/decisions/20261124_002757-issue-136d4553ff92-decision-3f62c4f8633a.md
?? docs/gold/2026-11-23-one-brain-emergent-fields-research-reference.md
?? docs/gold/2026-11-24-juggernaut-horde-overwhelming-human-evidence.md
?? docs/handoffs/2026-11-24-live-evidence-snapshot-time-handoff.md
?? docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md
?? docs/issues/bug/20261124-agent-screenshot-captures-desktop-content-instea-issue-130ee745052a.md
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
?? docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md
?? docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md
?? docs/issues/improvement/20261124-require-runtime-event-proof-for-every-gameplay-c-issue-dad84e4e7c50.md
?? docs/specs/dev-loop/live-evidence-observer.md
?? docs/specs/simulation/
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   4 +-
 config/gamemodes/bloodtest.json                    |  12 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   2 +
 config/hot-reload/packet-ledger.json               | 236 ++++----
 config/maps/dust2cyberiav4.json                    | 110 +++-
 config/movement/movement-slowdm.json               |  12 +-
 config/movement/movement-source.json               |   2 +-
 config/postfx.json                                 |   5 +
 config/tips.json                                   |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  27 +
 devscripts/dev_loop/cli.py                         |  28 +
 devscripts/dev_loop/parallel_test_runs.py          |  10 +-
 docs/ROUTER.md                                     |   4 +
 docs/features/npc-behavior.md                      |  85 +++
 docs/issues/README.md                              |  63 ++-
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  40 ++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  18 +
 ...ness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |   8 +
 ...et-exe-build-at-compiling-issue-00b133ae7e13.md |  37 ++
 ...et-exe-build-at-compiling-issue-693e63059a7c.md |  38 ++
 ...budget-exe-build-at-setup-issue-723326f11472.md |  39 ++
 ...ild-queue-at-build-pendin-issue-af958c3e10eb.md |  36 ++
 ...e-dll-build-at-game-build-issue-191782d7d9c5.md |  37 ++
 ...e-dll-build-at-game-build-issue-41a872c08642.md |  45 ++
 ...e-dll-build-at-game-build-issue-6641a6c6fdf4.md |  41 ++
 ...e-dll-build-at-game-build-issue-84ec2bf27598.md |  38 ++
 ...e-dll-build-at-game-build-issue-f66e6e82656b.md |  49 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  75 +++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 ++
 ...physics-and-car-based-mod-issue-e5a086b0a441.md |  45 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  34 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md | 106 ++++
 ...-a-specific-tick-for-proo-issue-89efbbc86535.md |   6 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md | 177 ++++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  33 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 621 +++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 382 +++++++++++++
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  48 ++
 ...bodycam-presentation-spec-issue-e9f582f8d2d0.md |   6 +
 ...-at-client-reconciliation-issue-651bb3982693.md |  10 +
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |  12 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   8 +
 docs/operations/devscripts/script-inventory.md     |   2 +
 .../specs/dev-loop/agent-proof-and-verification.md |  34 ++
 docs/specs/dev-loop/multi-instance.md              |   6 +
 docs/specs/dev-loop/visual-proof-screenshots.md    |  34 +-
 docs/specs/gamemodes/bodycam.md                    |  41 +-
 docs/specs/graphics/graphics-post-processing.md    |  49 +-
 docs/workflows/runtime-scenario-validation.md      |   7 +
 src/config/movement-config.cpp                     |  20 +
 src/engine/engine-tick-net.cpp                     |   4 +
 src/engine/engine-tick-ui-game-hud.cpp             |  22 +
 src/engine/engine-tick-ui.cpp                      |   4 +-
 src/engine/engine-tick-ui.h                        |   2 +-
 src/engine/engine-tick.cpp                         |  17 +-
 src/entities/aimbody-config.h                      |   6 +
 src/entities/player.h                              |   1 +
 src/gamemode/gamemode-runtime-resolver.cpp         |   4 +
 src/gamemode/gamemode.cpp                          |   3 +
 src/gamemode/gamemode.h                            |   2 +
 src/gui/gui-main.cpp                               |   2 +-
 src/gui/ui-system.cpp                              |  22 +-
 src/gui/ui-system.h                                |   3 +-
 src/input/input-commands.cpp                       |   2 +
 src/input/input-frame.h                            |   1 +
 src/input/input-poll.cpp                           |   2 +
 src/input/input-state.h                            |   2 +
 src/network/community-match-client.cpp             |  15 +
 src/network/movement-validation.h                  |   3 +-
 src/network/multiplayer-context.h                  |   1 +
 src/network/multiplayer-projectiles.cpp            |   5 +
 src/network/multiplayer-reconcile.cpp              |   1 +
 src/network/multiplayer-tick.cpp                   |   1 +
 src/network/schema/packet-common.h                 |   3 +
 src/network/server-gamemode.cpp                    |   1 +
 src/network/server-packets.cpp                     |   4 +
 src/network/server-players.cpp                     |  28 +-
 src/network/server.h                               |   1 +
 src/physics/movement/movement-conversion.cpp       |   4 +
 src/physics/movement/movement-step.cpp             |  83 ++-
 src/physics/movement/movement-step.h               |   8 +
 src/physics/movement/movement-types.h              |  27 +
 src/ragdoll/ragdoll-mode.cpp                       |  35 +-
 src/render/post-fx.cpp                             |   1 +
 src/render/post-fx.h                               |   6 +
 src/sim/simulate-tick.cpp                          |  38 ++
 89 files changed, 2990 insertions(+), 216 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	config/maps/dust2cyberiav4.json
M	config/movement/movement-slowdm.json
M	config/movement/movement-source.json
M	config/postfx.json
M	config/tips.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/parallel_test_runs.py
M	docs/ROUTER.md
M	docs/features/npc-behavior.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261123-isolated-test-run-readiness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-00b133ae7e13.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-compiling-issue-693e63059a7c.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-723326f11472.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-af958c3e10eb.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-191782d7d9c5.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-41a872c08642.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6641a6c6fdf4.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md
M	docs/issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
M	docs/issues/improvement/20261123-complete-the-bloodtest-bodycam-presentation-spec-issue-e9f582f8d2d0.md
M	docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
M	docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/operations/devscripts/script-inventory.md
M	docs/specs/dev-loop/agent-proof-and-verification.md
M	docs/specs/dev-loop/multi-instance.md
M	docs/specs/dev-loop/visual-proof-screenshots.md
M	docs/specs/gamemodes/bodycam.md
M	docs/specs/graphics/graphics-post-processing.md
M	docs/workflows/runtime-scenario-validation.md
M	src/config/movement-config.cpp
M	src/engine/engine-tick-net.cpp
M	src/engine/engine-tick-ui-game-hud.cpp
M	src/engine/engine-tick-ui.cpp
M	src/engine/engine-tick-ui.h
M	src/engine/engine-tick.cpp
M	src/entities/aimbody-config.h
M	src/entities/player.h
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/gui/gui-main.cpp
M	src/gui/ui-system.cpp
M	src/gui/ui-system.h
M	src/input/input-commands.cpp
M	src/input/input-frame.h
M	src/input/input-poll.cpp
M	src/input/input-state.h
M	src/network/community-match-client.cpp
M	src/network/movement-validation.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/network/multiplayer-reconcile.cpp
M	src/network/multiplayer-tick.cpp
M	src/network/schema/packet-common.h
M	src/network/server-gamemode.cpp
M	src/network/server-packets.cpp
M	src/network/server-players.cpp
M	src/network/server.h
M	src/physics/movement/movement-conversion.cpp
M	src/physics/movement/movement-step.cpp
M	src/physics/movement/movement-step.h
M	src/physics/movement/movement-types.h
M	src/ragdoll/ragdoll-mode.cpp
M	src/render/post-fx.cpp
M	src/render/post-fx.h
M	src/sim/simulate-tick.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-24T03:30:52.372066Z",
    "latest_successful_build_id": "2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6",
    "latest_successful_source_generation": 20
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "0e7ff7a9733ced73fc1f53610cfd4e5639af1feaa60c840eae7651ff10ed2e35",
    "dll": "build\\game\\generation-581\\game-g581.dll",
    "generation": 581,
    "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-581\\game-g581.dll",
    "generation": 581,
    "published_utc": "2026-11-24T04:35:40.515Z",
    "revision": "ae6b8012f78e4e7549b8e8d707233681085dc561ebe26b44157d87c6879e05a0",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 59.525,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-24T03:30:44.219Z"
  }
}
```
