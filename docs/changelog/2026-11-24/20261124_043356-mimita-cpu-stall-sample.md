// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# MiMITA CPU Stall Sample

# Task

- Task ID: session-e639d686b901
- Session ID: session-e639d686b901
- Owner: resource-profiler
- Focus: performance
- Summary: Captured a bounded mixed-runtime resource sample and identified a 3.42-second wall-time update for one NPC; recorded runtime, build-overlap, and CPU-attribution caveats.
- Status: in-progress
- Date, time, timezone: 2026-11-24T04:33:53+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

# Pre-existing changes

- Exact status output: The shared checkout contains extensive concurrent edits, shown in the generated status inventory below. This activation added only the CPU issue discussion, one CSV capture, and this changelog; it edited no game source.
- Files created or modified by this session: `.dev/perf-captures/cpu-mixed-runtimes-20261124T0418Z.csv`; `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d`; this changelog.

# Requested behavior

- Continue the durable CPU/resource investigation: collect bounded host and process evidence, identify costs at function/source location where measurements permit, test optimizations against matched workload, and preserve 240-FPS client / 60-Hz server behavior. This activation does not complete that goal.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`, `docs/specs/dev-loop/build-time-observability.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/workflows/issue-intake-and-triage.md`, `docs/operations/task-completion/task-completion.md`.
- Exact requirements: Resource observations must include UTC, duration, logical CPU count, per-process CPU in one-core and whole-host units, workload identity, concurrent build state, top competing processes, and artifact links. Wall time and CPU attribution are different claims; missing client/runtime proof stays unknown.
- Why the change follows the specification: The bounded sample was retained with its exact process/workload and build-overlap caveats and appended to the existing CPU issue. No causal claim or gameplay optimization was made from mixed-generation evidence.
- Conflicts or decisions: The shared run is not a connected client/server pair, and NPC module generation lags host DLL generation. The sample is useful server pressure evidence but cannot serve as the matched A/B baseline.

# Exact implementation changes

## File: `.dev/perf-captures/cpu-mixed-runtimes-20261124T0418Z.csv`

- Lines/functions/headings: 14 one-second host/process samples, 2026-11-24T04:17:57.943Z–04:18:11.933Z.
- Old content or behavior: No prior file; a bounded read-only capture was created.
- New content or behavior: Host CPU mean 100%; RAM used mean 90.8%, minimum available 1354 MiB; target server PID33892 mean 14.63% whole-host / 175.56% one logical CPU; PID6064 mean1.84% host/22.08% one core; PID29656 mean1.86% host/22.32% one core. Top competitors: RobloxPlayerBeta PID30340 13.88% host, FL64 PID27936 10.45%, SnapOS64 PID7384 8.62%, pythonw PID16864 5.47%, dev-loop pythonw PID34700 3.24%. Sampler overhead mean2.54% one core/0.21% host.
- Reason: Quantify current server-side pressure without claiming the unverified foreground client was connected.
- Why unrelated behavior is preserved: The sampler does not alter process priority, game settings, build inputs, or runtime state.

## File: `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d`

- Lines/functions/headings: Append-only evidence discussion begins at line 2859.
- Old content or behavior: No prior text removed or edited.
- New content or behavior: The full append records the sample artifact and interval, whole-host and one-core normalized process CPU, memory, sampler overhead, workload identity, gen579→580 transition, related build attempt, 4.522-second server tick, and 3.422-second worst-NPC wall-time update. It labels CPU-attribution and pair-proof gaps.
- Reason: Keep measured findings and caveats available to CPU and DLL-latency owners.
- Why unrelated behavior is preserved: Existing issue history remains append-only.

## File: `src/network/server-npcs.cpp` and `src/npc/npc.cpp` (read-only attribution)

- Lines/functions/headings: `simulateSharedNpcs` around 2737–2745 and 3421; `NpcSystem::updateOneWithTarget` at1240; `NpcSystem::updateOneNpc` at1576.
- Old content or behavior: No edits.
- New content or behavior: The journal's slow tick reports 3,544.45ms total; `npc_ai_ms` is 3,512.60ms and worst actor200062 took3,422.51ms. The timer around `updateOneWithTarget` includes the shared `updateOneNpc` work. This is elapsed wall time, not measured CPU milliseconds.
- Reason: Select a precise target for the next bounded function-cycle probe.
- Why unrelated behavior is preserved: Source inspection only.

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
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md |
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
| `docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md |
| `docs/operations/devscripts/script-inventory.md` |  |  |  | git checkout -- docs/operations/devscripts/script-inventory.md |
| `docs/specs/dev-loop/multi-instance.md` |  |  |  | git checkout -- docs/specs/dev-loop/multi-instance.md |
| `docs/specs/gamemodes/bodycam.md` |  |  |  | git checkout -- docs/specs/gamemodes/bodycam.md |
| `docs/specs/graphics/graphics-post-processing.md` |  |  |  | git checkout -- docs/specs/graphics/graphics-post-processing.md |
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
| `docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md' |
| `docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md' |
| `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md' |
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
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-24T03:30:52.372066Z", "latest_successful_build_id": "2026-11-24T03:30:52.372066Z:3ae36c9314a776a66c842b97356f14e128171e1442d28e49cd18a110b6030db6", "latest_successful_source_generation": 20}, "game_current": {"build_mode": "debug", "code_hash": "c52bacb0ee377fefcbd527307e30933dade1af55883f88450281b10aa873b6fb", "dll": "build\\game\\generation-580\\game-g580.dll", "generation": 580, "revision": "a7d41dfc93935fe7a0e2fa9c481f3fc9bd9679dd7e02ca6a96efcc4735b3f12b", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-580\\game-g580.dll", "generation": 580, "published_utc": "2026-11-24T04:18:01.000Z", "revision": "a7d41dfc93935fe7a0e2fa9c481f3fc9bd9679dd7e02ca6a96efcc4735b3f12b", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 59.525, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-24T03:30:44.219Z"}}
- Generation: 580
- Artifact: build\game\generation-580\game-g580.dll
- Journal path: C:\mimita-v9\logs\2026-11-24\20261124_022440_01\client-02\events.jsonl
- Session ID: session-e639d686b901

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
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
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
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/operations/devscripts/script-inventory.md
 M docs/specs/dev-loop/multi-instance.md
 M docs/specs/gamemodes/bodycam.md
 M docs/specs/graphics/graphics-post-processing.md
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
?? docs/issues/bug/20261124-coalesce-delayed-damage-confirmations-and-presen-issue-33dd666c3008.md
?? docs/issues/bug/20261124-gen576-fcs-client-never-observes-active-phase-wh-issue-d2a8614db704.md
?? docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-compiling-issue-c4b82c2ed632.md
?? docs/issues/build-friction/20261124-wait-over-budget-exe-build-at-linking-issue-627ca4b9f577.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-456061c38a8c.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-5bca92fcb543.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md
?? docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md
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
 config/hot-reload/packet-ledger.json               | 236 ++++-----
 config/maps/dust2cyberiav4.json                    | 110 +++-
 config/movement/movement-slowdm.json               |  12 +-
 config/movement/movement-source.json               |   2 +-
 config/postfx.json                                 |   5 +
 config/tips.json                                   |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  27 +
 devscripts/dev_loop/cli.py                         |  28 ++
 devscripts/dev_loop/parallel_test_runs.py          |  10 +-
 docs/ROUTER.md                                     |   4 +
 docs/features/npc-behavior.md                      |  85 ++++
 docs/issues/README.md                              |  51 +-
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  40 ++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  18 +
 ...ness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |   8 +
 ...et-exe-build-at-compiling-issue-00b133ae7e13.md |  37 ++
 ...et-exe-build-at-compiling-issue-693e63059a7c.md |  38 ++
 ...budget-exe-build-at-setup-issue-723326f11472.md |  39 ++
 ...ild-queue-at-build-pendin-issue-af958c3e10eb.md |  36 ++
 ...e-dll-build-at-game-build-issue-191782d7d9c5.md |  37 ++
 ...e-dll-build-at-game-build-issue-41a872c08642.md |  45 ++
 ...e-dll-build-at-game-build-issue-84ec2bf27598.md |  38 ++
 ...e-dll-build-at-game-build-issue-f66e6e82656b.md |  49 ++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  75 +++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  34 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  80 +++
 ...opment-and-networking-con-issue-cf9cd77eb056.md | 177 +++++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  33 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 555 +++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 268 ++++++++++
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  48 ++
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |  12 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   8 +
 docs/operations/devscripts/script-inventory.md     |   2 +
 docs/specs/dev-loop/multi-instance.md              |   6 +
 docs/specs/gamemodes/bodycam.md                    |  28 +-
 docs/specs/graphics/graphics-post-processing.md    |  49 +-
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
 81 files changed, 2588 insertions(+), 204 deletions(-)
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
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-84ec2bf27598.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f66e6e82656b.md
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
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/operations/devscripts/script-inventory.md
M	docs/specs/dev-loop/multi-instance.md
M	docs/specs/gamemodes/bodycam.md
M	docs/specs/graphics/graphics-post-processing.md
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
    "code_hash": "c52bacb0ee377fefcbd527307e30933dade1af55883f88450281b10aa873b6fb",
    "dll": "build\\game\\generation-580\\game-g580.dll",
    "generation": 580,
    "revision": "a7d41dfc93935fe7a0e2fa9c481f3fc9bd9679dd7e02ca6a96efcc4735b3f12b",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-580\\game-g580.dll",
    "generation": 580,
    "published_utc": "2026-11-24T04:18:01.000Z",
    "revision": "a7d41dfc93935fe7a0e2fa9c481f3fc9bd9679dd7e02ca6a96efcc4735b3f12b",
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

## Runtime and measurement details

- In-window server journal: run `20261124_022440_01`, PID33892, Juggernaut Horde on dust2cyberiav4, 67 NPCs and 0 players. Nine server tick windows averaged44.81ms `total_avg_ms`; the window containing the worst tick recorded 4522.547ms maximum, with npc_ms=4428.907 and gamemode_ms=92.047. This is below the 60-Hz target and is not a connected-pair acceptance sample.
- Matching `npc.tick-cost`: 2026-11-24T04:18:05.431Z, tick150562, `simulateSharedNpcs` at `src/network/server-npcs.cpp:3421`; total3544.4496ms; `npc_ai_ms=3512.6032`; `worst_ai_actor=200062`; `worst_ai_ms=3422.5101`; logger mutex wait/hold both0.0ms; difficulty poll0.1569ms and collision-config poll2.582ms.
- The sample spans build attempt `game-dll-20261124T041553895510Z-0e8a6d`: it began04:15:53.895Z and published gen580 at04:17:58.550Z, 0.607s after the sample began. Host hotness moved g579→g580 during the sample while NPC module generation remained577. This is neither a stable no-build runtime profile nor compiler attribution. The exact build timing is recorded in `.dev/build-timings/game-dll-20261124T041553895510Z-0e8a6d.json`.
- The existing callsite probe wraps `NpcSystem::updateOneNpc` and reports sampled cycles scaled 1/64 against same-window process CPU time in `src/perf/perf-frame.cpp:294-345`. The reported per-site CPU milliseconds are estimates and can overlap. There was no `performance.cpu-callsite-window` in this capture, so the 3.422-second actor update is not yet split into CPU time by inner function or line.
- No client rendered-frame proof was available; PID2388 had only a baseline sample row and no valid CPU delta. Other server processes were separate Bloodtest processes. Do not combine these into a clean client/server pair total.

## Next step

The exact high wall-time target is one actor's NPC update. On a fresh connected pair after owner release and matching DLL/module generations, run the existing bounded callsite CPU probe plus low-overhead host/process sampling, and include server tick and client frame journals in the same interval. Then change only the measured dominant owner and compare before/after. Keep a separate no-build idle pair for the 1%/500MB stretch target.

## Validation and review

- Read the routed performance, build authority, coordination, issue-intake, and task-completion documents.
- The 15-second native sampler completed with14 rows; no build was requested and no gameplay source changed. The observation straddled build publication and a generation transition; this caveat is retained above and in the CPU issue.
- No tests were requested or run. No behavior change is claimed. Human gameplay acceptance and valid 240-FPS client proof remain outstanding.
