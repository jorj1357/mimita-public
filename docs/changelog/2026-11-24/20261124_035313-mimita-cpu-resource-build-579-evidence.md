// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# MiMITA CPU resource build 579 evidence

# Task

- Task ID: session-1dcdd72054a9
- Session ID: session-1dcdd72054a9
- Owner: resource-profiler
- Focus: CPU attribution, build pressure, NPC tick cost, profiler evidence gaps
- Summary: Captured the full gen579 build resource sample, traced two NPC hot-path timing candidates, and coordinated separate A/B follow-ups; no product source was changed.
- Status: Evidence pass complete; optimizations and matched client/server proof remain open
- Date, time, timezone: 2026-11-24T03:53:11+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Final commit: 186eebd23ce5f8f3f10377c47277ed58bd3b4d83

# Pre-existing changes

- Exact status output: captured by `devscripts/changelog.py`; the shared checkout already contained many dirty files and concurrent agent edits.
- Files not created or modified by this session: all existing product/config/spec files and all unrelated dirty paths in the captured state. This session changed no product source or runtime configuration.
- Session-attributed writes: append-only evidence entries in `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` and `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`, plus this changelog. Do not revert either shared issue file wholesale.

# Requested behavior

Continue the user's persistent goal to identify MiMITA resource costs at process/function/source level, test one optimization at a time, preserve gameplay and performance requirements, and record results for later agents. This pass was evidence collection and coordination; it did not claim a CPU reduction.

# Specification alignment

- Current specification paths: `docs/specs/performance/performance.md`; `docs/specs/performance/culling-and-work-avoidance.md`; `docs/workflows/measurement-hypothesis-driven-improvement.md`; `docs/specs/dev-loop/build-time-observability.md`; `docs/specs/dev-loop/change-to-evidence-latency.md`; `docs/specs/dev-loop/agent-coordination.md`; `docs/specs/dev-loop/agent-lanes-build-authority.md`; `docs/specs/dev-loop/agent-proof-and-verification.md`; `docs/specs/dev-loop/agent-self-improvement.md`; `docs/specs/debug-logging/canonical-jsonl.md`; `docs/workflows/issue-intake-and-triage.md`; `docs/operations/task-completion/task-completion.md`; `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Focused skill paths reviewed: `docs/skills/efficiency-checker-v1.md`, `docs/skills/documentation-checker-v1.md`.
- Exact requirements: profile actual process CPU rather than infer from host totals; distinguish elapsed time from CPU time; use matched workloads; optimize one measured owner at a time; retain 60 Hz fixed simulation and the per-frame 240 FPS client gate; link build, runtime, and human evidence separately.
- Why the change follows the specification: the full build sidecar and precise journal/source correlations were appended to existing resource and latency issues with explicit limitations. No source change was made without a clean, matched A/B window.
- Conflicts or decisions: host DLL generation 579 is active, but the current server reports NPC module generation 577. The registered client PID is stale and the server has zero players. This run cannot prove a matched client/server workload or 240 FPS.

# Exact implementation changes

No product-source, config, or profiler-code changes were made.

- Existing issue additions at CPU issue lines 2737, 2745, 2777, 2793, 2803, and 2813 include collision-log correlation, NPC config-poll timings, the complete gen579 resource summary, runtime identity correction, pagefile totals, and a text-formatting correction. Before: those records for this run were absent. After: observed data, source paths, evidence caveats, and next A/B proposals are appended. The coordination discussion is append-only; preserve existing entries.
- Build latency issue addition at line 3622 begins `Correction to my earlier gen579 latency note: the summary JSON's cache_invalidations array contained only32 rows...`. Before: the summary-only count of 32 was reported as complete. After: the full JSONL event count of 156 is recorded and grouped by normalized header.
- Reason: preserve the latest attempt's process-resource evidence and correct an incomplete summary-JSON invalidation count using the full append-only timing stream.
- Unrelated behavior preserved: no source, config, runtime process, or build authority was changed by this session.

# Changed files and reverse steps

Session-attributed effect: append evidence to the two issue discussions and create this changelog. The captured changed-path table below includes shared work by other agents and is not a list of this session's edits. Reverse only this session's appended discussion messages if specifically requested; never revert entire issue files or any other shared checkout path.

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/changelog/2026-11-24/20261124_035313-mimita-cpu-resource-build-579-evidence.md` | no file | new session evidence record | required final record | remove only this file if specifically requested |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` | existing issue history | gen579 full invalidation-count correction appended at line 3622 | correct summary-only count with complete JSONL | append-only shared history; do not revert whole file |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` | existing issue history | this pass's resource/collision/config evidence appended at lines 2737-2813 | record measured host/process data and caveats | append-only shared history; do not revert whole file |
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
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-bf610d228066.md' |
| `docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261124-wait-over-budget-game-dll-build-at-game-build-issue-ecdc697b2415.md' |
| `docs/specs/dev-loop/live-evidence-observer.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/live-evidence-observer.md' |
| `docs/specs/simulation/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/simulation/' |

# Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: not applicable.
- Reverse: no code or ABI change.

# State / schema changes

- Domain: none.
- Old schema: not applicable.
- New schema: not applicable.
- Migration: none.
- Reverse: not applicable.

# Deleted paths

- Path: none.
- Before: not applicable.
- Why: no paths deleted.
- Reverse: not applicable.

# Diagnostics

- Owner/category: existing root dev-loop build-resource sampler; runtime owner `simulateSharedNpcs` / NPC collision diagnostics.
- Input: host/process snapshots and canonical `server-01/events.jsonl` records for run `20261124_022440_01`.
- Decision: correlate build phase and process CPU with server workload; separately identify selected NPC collision and tick outliers.
- Output: `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.resource.jsonl`, its `.resource.json` summary, the build timing `.jsonl`/`.json`, and issue discussion entries.
- Failure or rejection reason: sampler explicitly reports `runtime_alignment.status=unavailable` and no matched pre-build baseline, commit-charge limit, or client frame/tick data. No performance change was tested.
- Rate limiting: sampler target 1s, process discovery 5s; observed interval averaged 1.57s and peaked 4.63s. NPC fall diagnostic is rate-limited to once per actor per 30 ticks.

# Validation

- Focused skill paths and results: efficiency checker reviewed; candidates and uncertainty recorded. Documentation checker applied to the changelog and routes; no source recommendation was implemented.
- Tests and exact commands: read-only Python aggregation of the attempt resource/timing JSONL; PowerShell `python devscripts/dev-loop.py --status`; journal queries with `rg`; run-manifest inspection. No tests were added or run because no code changed.
- Build status: unrelated root attempt `game-dll-20261124T033440193460Z-3a5393` succeeded and published generation 579 at 2026-11-24T03:43:34.316Z. It took 534.174s total; the full timing stream lists 158 compiled, 485 skipped, and 156 header invalidations. This session did not request the build.
- Runtime or hot-reload evidence: server PID 33892 later emitted hotness generation 579, Juggernaut Horde, dust2cyberiav4, 67 NPCs/0 players. Adjacent NPC phase rows still report module_generation 577. `Get-Process` found server PID 33892 but not registered client PID 2388. No connected pair, player behavior, or client frame evidence is proven.
- Output files: `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.resource.jsonl`; `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.resource.json`; `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.jsonl`; `.dev/build-timings/game-dll-20261124T033440193460Z-3a5393.json`; `logs/2026-11-24/20261124_022440_01/server-01/events*.jsonl`.

# Measured evidence

- Before values: no same-run A/B baseline. This is observation, not an optimization result.
- After values: full build capture 2026-11-24T03:34:40.193Z–03:43:34.360Z, 534.167s and 340 samples; 12 logical CPUs; host CPU mean 99.977%, peak 100%; RAM use mean 90.42%, peak 100%, available RAM mean1.522GB/minimum135,168 bytes; pagefile used mean38.717GB/peak41.831GB of45.516GB. Tracked four MiMITA processes mean 11.50% of host capacity combined, compiler/linker 15.63%, tracked other applications 37.64%. Tracked MiMITA PID averages: server33892 5.35%, client-labeled2388 2.76%, and two additional unassigned processes6064 1.68% and29656 1.71%. Sidecar does not validate these as a connected pair. Sampler thread used 10,109ms CPU over 534s (~0.16% host average); actual sample interval averaged 1.57s. Pagefile use alone does not prove active disk paging.
- Timestamps: server tick-window at 2026-11-24T03:39:26.782Z (while build continued) reported 15 sampled ticks, total avg 64.188ms, max 186.603ms, 2/15 above 100ms; adjacent stall event reported 156.066ms. This is a concurrent-load observation, not an isolated causal comparison.
- Tick/frame/network measurements: server journal verified Juggernaut Horde / dust2cyberiav4 / 67 NPC / 0 players and generation 577 during the sample; later host hotness reports generation579 but NPC phase remains577. Client FPS and connected-user behavior are unavailable.
- Collision candidate: 875 matching actor+tick pairs; fall-event-to-substage gap median26.806ms, p95 236.858ms, p99 2409.048ms, max4588.769ms; total collision diagnostic median26.903ms. Selected outliers only; not CPU share.
- Config polling candidate: among 2391 `npc.tick-cost` events emitted only for ticks above50ms, `npc_difficulty_config_poll_ms` median0.2078ms, p95 2.6332ms, p99 12.2452ms, max54.2418ms. Not a representative all-tick distribution.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: neither elapsed-time candidate is proven to cause the user's reported CPU regression. The collision diagnostic commit predates the reported onset; selected elapsed-time gaps can include blocking or preemption.
- Related regression paths: none confirmed.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not proven; server had zero players.
- Still unverified: current CPU attribution for a connected pair; active client frame-time distribution; 60 Hz acceptance; any before/after optimization; NPC-module generation refresh.

# Agent handoff context

- Owner: resource-profiler
- Focus: CPU attribution, build pressure, NPC tick cost, profiler evidence gaps
- Files to check: `src/physics/movement/physics-collision-glb-main.cpp:615-654`; `src/debug/structured-log.cpp:1542-1669`; `src/network/server-npcs.cpp:2102-2114`; `src/npc/npc-difficulty-config.cpp:312-328`.
- What another agent should not redo: the full 340-row resource aggregation, 156-row cache invalidation count from the full timing JSONL, the rotated collision event pair analysis, or the existing issues' appended evidence.
- Next exact step: after the bodycam/build owners release the root batch and a healthy client/server pair is journal-verified, run the existing bounded function-cycle probe and process sampler on that exact pair. Then test one collision logging behavior change (`writeEvent` versus `tryWriteEvent`) alone, followed by a separate config-polling A/B. Require matched workload, active module generation, server tick distribution, and client per-frame results before retaining an optimization.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83 / 186eebd23ce5f8f3f10377c47277ed58bd3b4d83
- Git status: captured below
- Diff stat: captured below
- Build identity: authoritative attempt is `game-dll-20261124T033440193460Z-3a5393`, source generation21, generation579, revision `be49c3e1ca52efbc3368aecdf082a58b308223015f796ae5835343c0a08aa53c`, published 2026-11-24T03:43:34.316Z. The auto-captured latest EXE build/sourcegen20 and progress timestamp03:30:44 refer to an older EXE build, not this DLL attempt.
- Generation: 579
- Artifact: build\game\generation-579\game-g579.dll
- Journal path: actual CPU runtime was `C:\mimita-v9\logs\2026-11-24\20261124_022440_01\server-01\events.jsonl`. The auto-generated path `C:\mimita-v9\logs\2026-11-24\20261124_035143\client-02\events.jsonl` belongs to a separate failed `parallel_agent_test` Bloodtest run; do not use it as CPU-sample evidence.
- Session ID: session-1dcdd72054a9

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: no feature dossier changed. Related issue records: `issue-ed3ac775283d` (resource reduction) and `issue-136d4553ff92` (build and hot-reload latency).

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
 config/hot-reload/packet-ledger.json               | 232 +++++-----
 config/maps/dust2cyberiav4.json                    | 110 ++++-
 config/movement/movement-slowdm.json               |  12 +-
 config/movement/movement-source.json               |   2 +-
 config/postfx.json                                 |   5 +
 config/tips.json                                   |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  27 ++
 devscripts/dev_loop/cli.py                         |  28 ++
 devscripts/dev_loop/parallel_test_runs.py          |   6 +-
 docs/ROUTER.md                                     |   4 +
 docs/features/npc-behavior.md                      |  85 ++++
 docs/issues/README.md                              |  46 +-
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  40 ++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  18 +
 ...ness-uses-a-45s-wall-wait-issue-fa63aa4e218f.md |   8 +
 ...et-exe-build-at-compiling-issue-00b133ae7e13.md |  37 ++
 ...et-exe-build-at-compiling-issue-693e63059a7c.md |  38 ++
 ...budget-exe-build-at-setup-issue-723326f11472.md |  39 ++
 ...ild-queue-at-build-pendin-issue-af958c3e10eb.md |  36 ++
 ...e-dll-build-at-game-build-issue-191782d7d9c5.md |  37 ++
 ...e-dll-build-at-game-build-issue-84ec2bf27598.md |  38 ++
 ...e-dll-build-at-game-build-issue-f66e6e82656b.md |  49 +++
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |  48 ++
 ...e-dll-build-at-game-build-issue-b1744402b0a5.md |  46 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  34 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  70 +++
 ...opment-and-networking-con-issue-cf9cd77eb056.md | 177 ++++++++
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |  33 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 481 +++++++++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 188 ++++++++
 ...dates-for-findings-and-cr-issue-70cbf4d13c02.md |  48 ++
 ...-while-server-starts-unde-issue-c8ac61cf07fc.md |  12 +
 docs/operations/devscripts/script-inventory.md     |   2 +
 docs/specs/dev-loop/multi-instance.md              |   6 +
 docs/specs/gamemodes/bodycam.md                    |  19 +-
 docs/specs/graphics/graphics-post-processing.md    |  49 ++-
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
 src/gui/ui-system.cpp                              |   5 +-
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
 src/network/server-packets.cpp                     |   4 +
 src/network/server-players.cpp                     |  28 +-
 src/network/server.h                               |   1 +
 src/physics/movement/movement-conversion.cpp       |   4 +
 src/physics/movement/movement-step.cpp             |  83 +++-
 src/physics/movement/movement-step.h               |   8 +
 src/physics/movement/movement-types.h              |  27 ++
 src/ragdoll/ragdoll-mode.cpp                       |  35 +-
 src/render/post-fx.cpp                             |   1 +
 src/render/post-fx.h                               |   6 +
 src/sim/simulate-tick.cpp                          |  38 ++
 78 files changed, 2310 insertions(+), 198 deletions(-)
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
    "code_hash": "45926450d0262aa23bc1324a6f285525d2d4c969edbec7d9ec1d46b2d6d0ba65",
    "dll": "build\\game\\generation-579\\game-g579.dll",
    "generation": 579,
    "revision": "be49c3e1ca52efbc3368aecdf082a58b308223015f796ae5835343c0a08aa53c",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-579\\game-g579.dll",
    "generation": 579,
    "published_utc": "2026-11-24T03:43:34.316Z",
    "revision": "be49c3e1ca52efbc3368aecdf082a58b308223015f796ae5835343c0a08aa53c",
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
