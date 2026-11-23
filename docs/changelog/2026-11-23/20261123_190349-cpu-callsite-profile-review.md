// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# CPU callsite profile review

# Task

- Task ID: work-712b7581487f
- Session ID: work-712b7581487f
- Owner: resource-efficiency-goal
- Focus: CPU callsite attribution, build-resource sampling, matched runtime profiling
- Summary: Reviewed the bounded callsite CPU probe and prior matched generation-560 server/client sample. updateOneNpc accounted for about 26% of server process cycle time, approximately 2.7% of whole-host CPU in that capture; client FPS was 155–174. Reviewed gen569 build resource capture failure and sparse rescue data. No source optimization or new runtime probe was performed because the current Bloodtest run is owned by another active test.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-23T19:03:49+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 0abd6231aea08bed8580bff0c11369c01fc40bde
- Final commit: 0abd6231aea08bed8580bff0c11369c01fc40bde

# Pre-existing changes

- The generated Git snapshot below contains extensive concurrent agent work on this branch. I did not modify or claim any of those files.
- This session made no game or build-source edits. It added coordination context/messages and this changelog only.

# Requested behavior

> “can u furhter look into like the exact files and steup things that we can do to make cpu usage go down? like along witih percentage of resources used, what funciton used it, what file, what is mkaing it use so much cpu, why, what proves it, etc — like function A line 123 in file A is contiubign 97% of cpu usage and then we changed it in test 2 and it used 3% cpu usage stuff like that”

# Specification alignment

- Read `docs/specs/performance/performance.md`: performance decisions use measured frame-time and simulation costs, not CPU utilization alone; gameplay remains fixed at 60 Hz; the client acceptance window requires every rendered frame at or below 4.167 ms, with p50/p95/p99/p99.99/max and over-budget counts.
- Read `docs/specs/performance/perf-report-overlay.md`, `docs/specs/performance/culling-and-work-avoidance.md`, and `docs/skills/efficiency-checker-v1.md`: profile measured work at its owner and preserve frame/tick acceptance.
- Read `docs/specs/dev-loop/build-time-observability.md`: matched build pressure needs time-aligned host/process evidence; missing samples are unknown, not zero.
- No specification or runtime behavior was changed.

# Exact implementation changes

- No C++ or Python product source was changed.
- Appended the user's exact profile goal to CPU issue `issue-ed3ac775283d` as brain context `ctx-a181ee09baf4`; appended the follow-up request for exact function/file percentages as `ctx-8a8ffbd46a31`.
- Sent the gen569 rescue capture and build-sampler review to the responsible latency agent, attached to build issue `issue-136d4553ff92`; sent probe-scope semantics and historical matched profile data to that agent, attached to CPU issue `issue-ed3ac775283d`.
- The sampler owner has since corrected the unreachable host-only fallback in the current source. The completed gen569 sidecar remains the earlier zero-sample failure; daemon reload and a subsequent build are still required to prove the fallback.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/changelog/2026-11-23/20261123_190349-cpu-callsite-profile-review.md` | No file | One session record with source map, measured evidence, limits, and next steps | Preserve this review for future agents | Remove only this session record if explicitly requested |

# Contract / ABI changes

- None.

# State / schema changes

- None in the game. Coordination brain gained two append-only user context records and issue-linked review messages.

# Diagnostics

- Read-only review of `src/perf/npc-cpu-probe.h`, `src/perf/perf-frame.cpp`, `src/terminal/debug-commands.cpp`, `src/npc/npc.cpp`, `src/physics/movement/physics-collision-dispatch.cpp`, and `src/engine/engine-tick.cpp`.
- The bounded probe emits `performance.cpu-callsite-window` in `src/perf/perf-frame.cpp:320-350`; its percentage math is at :293-314.
- The source line in the event is the probe placement (`__LINE__`), not an instruction line inside the function. Sampling is one in 64 calls; nested scopes can overlap and the event labels its shares as estimates.

# Validation

- Read-only source/artifact review and current dev-loop status.
- No tests, build, WPR session, runtime command, source optimization, or live probe was run in this review.
- WPR/WPA/xperf are installed. The retained ETLs and symbolized HTML are historical artifacts; no current trace was started.
- Build status and artifact identity are preserved in the automatic snapshot below.

# Measured evidence

## Historical matched callsite probe

- Run `20261123_113457_mode3-latency-run`; server PID 24692, client PID 23980; generation 560; Juggernaut Horde, 67 NPCs/1 player; server journal confirmed the NPC module on generation 560.
- The bounded `perf_callsite_cpu on 15` emitted 15 one-second windows from 14:00:07.757Z to 14:00:21.759Z.
- At the time of the run, `NpcSystem::updateOneNpc` was at `src/npc/npc.cpp:1549`; the callsite estimate averaged 25.97% of server process cycles (range 17.01–35.50%). The same-window server used 10.36% whole-host CPU, so the function estimate was approximately 2.69% whole-host CPU. This is an estimate of a broad function scope, not its inner instructions.
- The paired client used 18.29% whole-host CPU; its telemetry was 155–174 FPS and did not meet the 240 FPS target.
- Current source has the same function at `src/npc/npc.cpp:1575`, with the current probe marker at :1578-1579.

## Historical 10 ms WPR function samples

- ETL `.dev/perf-captures/mimita-cpu-10ms.etl`, 36.433 s, 10 ms period. Offline symbolization matched generation 546. Runtime evidence aligned the workload to Juggernaut Horde / dust2cyberiav4 / 67 NPCs / 1 player.
- ETW module CPU was client PID 3600 at 18.72% whole-host and server PID 8804 at 10.39%; combined 29.11%.
- Among 867 exclusive client game-DLL sample hits, the largest named rows were UIVertex allocator construction 38/867 (4.4%), textured-particle vertex construction 28/867 (3.2%), audio volume factor 18/867 (2.1%), and resampler 28/867 across adjacent lines (3.2%).
- Among 492 exclusive server game-DLL sample hits, glm vec4 construction was 13/492 (2.6%), and `AabbTree::overlaps` at `src/physics/movement/collision-aabb-tree.h:118` was 10/492 (2.0%).
- These are shares of sampled exclusive game-DLL instruction hits, not CPU milliseconds or shares of total process CPU. No single function explains the pair's CPU use; these samples do not justify a code change by themselves. Audio did not dominate this trace.

## Build resource capture and latency

- Build attempt `game-dll-20261123T182740034015Z-98b8bc` published generation 569 in 644.432 s. It compiled 185 translation units and skipped 458; 177 cache invalidations cited header freshness: 143 from `src/camera.h`, 32 from `src/gamemode/gamemode.h`, and 2 from camera-config headers. The longest TU was `src/mapeditor/map-editor.cpp` at 93.358 s.
- Timing summary: changed-source preflight 78.213 s; compile wall 536.225 s; object scan 4.095 s; link 5.945 s; publish 0.576 s.
- The automatic sidecar had zero samples because the daemon Python reported `ModuleNotFoundError: No module named 'psutil'`.
- Manual rescue artifact `.dev/perf-captures/game-dll-20261123T182740034015Z-98b8bc-rescue-20261123T183521Z.summary.json`: 15 samples, 18:35:21.609Z–18:38:03.338Z; host 12 logical CPUs, mean 78.647% CPU, max 84.7%, minimum available RAM 390,656,000 bytes. MiMITA was absent; compiler averaged 8.358% whole-host and unclassified other processes 51.366%. Actual sample intervals were 7.5–13.2 s, not the intended 1 s.
- This is evidence of host pressure during part of a build and severe memory scarcity at a sample point. It does not prove that MiMITA caused the build to take 644 s.

# Regression review

- No behavior regression was introduced or confirmed by this review.
- The prior gen560 `updateOneNpc` figure is a baseline only; no optimization A/B or before/after improvement was performed.

# Human acceptance

- No human gameplay, visual, or hardware acceptance was performed.
- 60 Hz server acceptance remains unverified for this review. The historical client sample failed the 240 FPS frame target.

# Agent handoff context

- CPU issue: `issue-ed3ac775283d`. Build latency/resource issue: `issue-136d4553ff92`.
- Current shared run `20261123_184017` is owned by the Bloodtest bodycam runtime task (server PID 32088/client PID 25772); its launch identity reports Bloodtest with 0 NPCs. It is not a suitable NPC CPU workload. I did not sample, command, stop, or restart it.
- Next: after that owner releases the pair, verify live mode, NPC/player counts, server/NPC module generation, PIDs, and frame/tick workload from fresh journals. Take a matched short host/process sample and run the bounded server/client callsite probe. If `updateOneNpc` remains material, add narrower probes around its expensive subcalls or use a matched stack trace; then optimize one owner and repeat the same workload.
- For an exact source-line stack profile, use a CPU stack ETL with matching generation PDBs and symbolize it offline. Previous WPR start failed with `0xc5585011`; do not claim that elevated launch alone has solved the required profiling permission.

# How to reverse

- No product source change to revert. The changelog and brain records are append-only evidence.

# Related records

- CPU issue: `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`
- Build issue: `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`
- Historical WPR symbolization: `docs/changelog/2026-11-23/20261123_0954-offline-wpr-symbol-resolution.md`
- Historical callsite sample: `docs/changelog/2026-11-23/20261123_1404-runtime-scoped-cpu-probe.md`

## Captured git state

```text
M .gitignore
 M config/accounts/default.json
 M config/analytics.json
 M config/behavior-profiles.json
 M config/gamemodes/bloodtest.json
 M config/gamemodes/bombtag.json
 M config/gamemodes/connected_world.json
 M config/gamemodes/counterstrike.json
 M config/gamemodes/duel.json
 M config/gamemodes/elimination.json
 M config/gamemodes/ffa.json
 M config/gamemodes/funcounterstrike.json
 M config/gamemodes/jugger_horde.json
 M config/gamemodes/juggernaut.json
 M config/gamemodes/juggernaut_duel.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/gamemodes/npc_waves.json
 M config/gamemodes/payload.json
 M config/gamemodes/retrograd.json
 M config/gamemodes/rocket_duel.json
 M config/gamemodes/sandbox.json
 M config/gamemodes/ssbm.json
 M config/gamemodes/survive_disasters.json
 M config/gamemodes/tdm.json
 M config/gamemodes/vehicle_arena.json
 M config/gamemodes/vs_jason.json
 M config/gamemodes/zombie_tower.json
 M config/hot-reload/packet-ledger.json
 M config/movement/movement-source.json
 M config/npc-difficulty.json
 M config/weapons/rocket_launcher/config.json
 M devscripts/agent-wake.py
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/cpu-wpr-capture.ps1
 M devscripts/dev-loop.py
 M devscripts/dev_loop/build.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/dev_loop/runtimes.py
 M devscripts/game_build_worker.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
 M docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md
 M docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md
 M docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md
 M docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
 M docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md
 M docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/improvement/20261121-track-proven-behavior-with-version-specific-regr-issue-37a27bfdc575.md
 M docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
 M docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/specs/gamemodes/bloodtest.md
 M docs/specs/gamemodes/bodycam.md
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/gamemodes/gamemodes.md
 M docs/specs/gamemodes/teamdeathmatch.md
 M docs/specs/npc-behavior/npc-behavior.md
 M docs/specs/weapons/weapons.md
 M src/camera.cpp
 M src/camera.h
 M src/config/camera-config.cpp
 M src/config/camera-config.h
 M src/devtools/terminal.cpp
 M src/engine/engine-tick-camera.cpp
 M src/engine/engine-tick-setup.cpp
 M src/engine/engine-tick.cpp
 M src/gamemode/gamemode-runtime-resolver.cpp
 M src/gamemode/gamemode.cpp
 M src/gamemode/gamemode.h
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/gui/ui-tooltip.cpp
 M src/map/map-geometry-module.cpp
 M src/mapeditor/map-editor-module.cpp
 M src/navigation/navigation-policy-module.cpp
 M src/network/network-policy-module.cpp
 M src/network/server-gamemode.cpp
 M src/network/server-gamemode.h
 M src/network/server-npcs.cpp
 M src/network/server-packet-chat.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc-perception-module.cpp
 M src/npc/npc.cpp
 M src/npc/npc.h
 M src/perf/perf-frame.cpp
 M src/physics/movement/actor-contact-policy-module.cpp
 M src/physics/movement/physics-collision-dispatch.cpp
 M src/render/actor-rendering-module.cpp
 M src/render/post-fx.cpp
 M src/render/post-fx.h
 M src/render/world-rendering-module.cpp
 M src/terminal/debug-commands.cpp
 M src/world/chunk-collision-module.cpp
 M src/worldgen/infinite-worldgen-module.cpp
?? config/dev-superadmin.example.json
?? config/movement/movement-slowdm.json
?? devscripts/dev_loop/batch_targets.py
?? devscripts/dev_loop/build_resource_sampler.py
?? devscripts/quality/npc-match-report.py
?? devscripts/quality/test_build_resource_sampler.py
?? devscripts/quality/test_game_build_cache_marker_fast_path.py
?? devscripts/quality/test_mode3_batch_net_delta.py
?? devscripts/quality/test_runtime_command_routing.py
?? docs/changelog/2026-11-23/20261123-1721-postbuild-mitita-resource-window.md
?? docs/changelog/2026-11-23/20261123_0805-cpu-phase-evidence-source-mapping.md
?? docs/changelog/2026-11-23/20261123_0816-build-15file-latency-and-resource-coupling.md
?? docs/changelog/2026-11-23/20261123_0835-wpr-sampled-function-attribution.md
?? docs/changelog/2026-11-23/20261123_0846-wpr-workload-runtime-alignment.md
?? docs/changelog/2026-11-23/20261123_0847-issue-6413477d361d-independent-review.md
?? docs/changelog/2026-11-23/20261123_0902-mode3-pch-off-and-generation554.md
?? docs/changelog/2026-11-23/20261123_0910-cpu-wpr-evidence-crosslink.md
?? docs/changelog/2026-11-23/20261123_0920-structured-log-quota-pressure-evidence.md
?? docs/changelog/2026-11-23/20261123_092247-independent-review-of-game-dll-progress-json-publication-failure.md
?? docs/changelog/2026-11-23/20261123_094103-source-only-object-marker-fast-path.md
?? docs/changelog/2026-11-23/20261123_0947-marker-validation-microbenchmark.md
?? docs/changelog/2026-11-23/20261123_0954-offline-wpr-symbol-resolution.md
?? docs/changelog/2026-11-23/20261123_1000-live-resource-sample-and-profiler-threading.md
?? docs/changelog/2026-11-23/20261123_1012-exe-build-queue-wait-review.md
?? docs/changelog/2026-11-23/20261123_1035-coordination-unread-and-closure-audit.md
?? docs/changelog/2026-11-23/20261123_1035-resource-sample-pre-build-post-build-comparison.md
?? docs/changelog/2026-11-23/20261123_104153-independent-review-launch-client-reconciliation-failures.md
?? docs/changelog/2026-11-23/20261123_1042-mode3-cache-marker-live-activation-and-blink-compatibility.md
?? docs/changelog/2026-11-23/20261123_1105-resource-profile-thread-and-tick-overload.md
?? docs/changelog/2026-11-23/20261123_112000-issue-7d013b4a6c77-independent-review.md
?? docs/changelog/2026-11-23/20261123_1153-issue-25b14f436a3e-independent-review.md
?? docs/changelog/2026-11-23/20261123_1200-corrected-matched-cpu-evidence.md
?? docs/changelog/2026-11-23/20261123_1239-independent-review-issue-9a9807fa0128.md
?? docs/changelog/2026-11-23/20261123_130354-issue-b0ba8698acd7-independent-review.md
?? docs/changelog/2026-11-23/20261123_1325-mode3-pair-under-pressure-generation560-latency.md
?? docs/changelog/2026-11-23/20261123_1404-runtime-scoped-cpu-probe.md
?? docs/changelog/2026-11-23/20261123_1434-resource-capture-and-logging-ab.md
?? docs/changelog/2026-11-23/20261123_1435-mode3-reverted-target-filter.md
?? docs/changelog/2026-11-23/20261123_1545-issue-safe-close-reopen-controls.md
?? docs/changelog/2026-11-23/20261123_1556-issue-3dad54c7224d-bounded-review.md
?? docs/changelog/2026-11-23/20261123_1605-local-dev-superadmin-unlock.md
?? docs/changelog/2026-11-23/20261123_1620-cpu-auth-coordination.md
?? docs/changelog/2026-11-23/20261123_1630-issue-266c2a856cfc-bounded-wait-review.md
?? docs/changelog/2026-11-23/20261123_1640-issue-8a2e0ac7236b-build-wait-review.md
?? docs/changelog/2026-11-23/20261123_1645-generation564-build-fanout-and-activation-crash.md
?? docs/changelog/2026-11-23/20261123_171116-bloodtest-tdm-and-bodycam-specification.md
?? docs/changelog/2026-11-23/20261123_1723-mode3-late-join-review-and-build-resource-coupling.md
?? docs/changelog/2026-11-23/20261123_1724-generation566-incremental-build-evidence.md
?? docs/changelog/2026-11-23/20261123_172909-bloodtest-projectile-weapon-specs.md
?? docs/changelog/2026-11-23/20261123_1735-issue-3d02ab0de320-build-wait-review.md
?? docs/changelog/2026-11-23/20261123_174610-bloodtest-camera-tuning-plan.md
?? docs/changelog/2026-11-23/20261123_175500-late-join-team-identity.md
?? docs/changelog/2026-11-23/20261123_1808-build-resource-sampler-and-gap-surfacing.md
?? docs/changelog/2026-11-23/20261123_1835-npc-elite-measurement-and-ct-defuse.md
?? docs/changelog/2026-11-23/20261123_185448-cpu-callsite-profile-review.md
?? docs/coordination/decisions/20261123_080111-mode3-live-dll-test-decision-24dfc5502e86.md
?? docs/coordination/decisions/20261123_082556-mode3-live-dll-test-decision-c01373b5bc38.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md
?? docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-65993acdc8b9.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-266c2a856cfc.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-3c2a8fd12fe9.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-3d02ab0de320.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8a2e0ac7236b.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-8fdc520a07c5.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-991c632f1ca0.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-a17177a48737.md
?? docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-ec790f30f5ad.md
?? docs/issues/capability/20261123-provider-neutral-screenshot-comparison-against-g-issue-41f95565b65e.md
?? docs/issues/improvement/20261123-require-safe-issue-closure-and-reopening-for-hum-issue-11efafcb7122.md
?? docs/issues/launch-friction/20261120-launch-failure-unknown-at-client-reconciliation-issue-651bb3982693.md
?? docs/specs/weapons/melee/bat.md
?? docs/specs/weapons/melee/momentum-two-handed.md
?? docs/specs/weapons/projectile/aa12.md
?? docs/specs/weapons/projectile/ak47.md
?? docs/specs/weapons/projectile/desert-eagle.md
?? docs/specs/weapons/projectile/famas.md
?? docs/specs/weapons/projectile/glock-17.md
?? docs/specs/weapons/projectile/judgement.md
?? docs/specs/weapons/projectile/m4a1.md
?? docs/specs/weapons/projectile/projectile-firearms.md
?? docs/specs/weapons/projectile/sawed-off-double-barrel.md
?? docs/specs/weapons/projectile/uzi.md
?? src/perf/npc-cpu-probe.h
```

## Captured diff stat

```text
.gitignore                                         |    1 +
 config/accounts/default.json                       |    2 +-
 config/analytics.json                              |    2 +-
 config/behavior-profiles.json                      |   37 +-
 config/gamemodes/bloodtest.json                    |   20 +-
 config/gamemodes/bombtag.json                      |    1 +
 config/gamemodes/connected_world.json              |    1 +
 config/gamemodes/counterstrike.json                |    1 +
 config/gamemodes/duel.json                         |    1 +
 config/gamemodes/elimination.json                  |    1 +
 config/gamemodes/ffa.json                          |    1 +
 config/gamemodes/funcounterstrike.json             |    1 +
 config/gamemodes/jugger_horde.json                 |    1 +
 config/gamemodes/juggernaut.json                   |    1 +
 config/gamemodes/juggernaut_duel.json              |    1 +
 .../gamemodes/masterlist/gamemode-masterlist.json  |   23 +
 config/gamemodes/npc_waves.json                    |    1 +
 config/gamemodes/payload.json                      |    1 +
 config/gamemodes/retrograd.json                    |    1 +
 config/gamemodes/rocket_duel.json                  |    1 +
 config/gamemodes/sandbox.json                      |    1 +
 config/gamemodes/ssbm.json                         |    1 +
 config/gamemodes/survive_disasters.json            |    1 +
 config/gamemodes/tdm.json                          |    1 +
 config/gamemodes/vehicle_arena.json                |    1 +
 config/gamemodes/vs_jason.json                     |    1 +
 config/gamemodes/zombie_tower.json                 |    1 +
 config/hot-reload/packet-ledger.json               |    6 +-
 config/movement/movement-source.json               |   11 +-
 config/npc-difficulty.json                         |    2 +-
 config/weapons/rocket_launcher/config.json         |    4 +-
 devscripts/agent-wake.py                           |   13 +
 devscripts/agent_coord.py                          |  174 ++
 devscripts/blender-ai                              |    0
 devscripts/cpu-wpr-capture.ps1                     |   16 +-
 devscripts/dev-loop.py                             |  196 +-
 devscripts/dev_loop/build.py                       |   67 +-
 devscripts/dev_loop/cli.py                         |    2 +-
 devscripts/dev_loop/game_build.py                  |   62 +-
 devscripts/dev_loop/parallel_test_runs.py          |    7 +-
 devscripts/dev_loop/runtimes.py                    |   25 +
 devscripts/game_build_worker.py                    |  197 +-
 docs/issues/README.md                              |   73 +-
 ...objective-behavior-regres-issue-be1adb51b6ac.md |   84 +
 ...re-journal-masks-first-st-issue-79e68f63fb01.md |    6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |   22 +
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |   30 +
 ...grades-under-high-cpu-or--issue-2a5c5da53565.md |   10 +
 ...s-compile-groundstate-has-issue-b7216286bdcc.md |   33 +
 ...s-compile-groundstate-has-issue-b91438ac2972.md |   35 +
 ...s-out-after-rebuilding-64-issue-1bf97a27238f.md |   42 +
 ...s-out-after-rebuilding-64-issue-a6890271d414.md |   12 +
 ...ion-fails-replacing-share-issue-61b5edc937d9.md |   42 +
 ...budget-exe-build-at-setup-issue-7d013b4a6c77.md |   38 +
 ...budget-exe-build-at-setup-issue-dff7e8c0f758.md |   50 +
 ...ild-queue-at-build-pendin-issue-3dad54c7224d.md |   38 +
 ...ild-queue-at-build-pendin-issue-e9f3b2727a9a.md |   44 +
 ...e-dll-build-at-game-build-issue-25b14f436a3e.md |   34 +
 ...e-dll-build-at-game-build-issue-40dfaa2a803c.md |   39 +
 ...e-dll-build-at-game-build-issue-6413477d361d.md |   34 +
 ...e-dll-build-at-game-build-issue-9a9807fa0128.md |   37 +
 ...e-dll-build-at-game-build-issue-b0ba8698acd7.md |   49 +
 ...e-dll-build-at-game-build-issue-bb273ac1386c.md |   39 +
 ...e-dll-build-at-game-build-issue-dd54a79216ad.md |   39 +
 ...move-avoidable-build-and--issue-e044a931c54b.md |   36 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   48 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |   46 +
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  125 +
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |   43 +
 ...nd-keep-log-retention-bou-issue-3f776ce60511.md |   47 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  670 ++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 2441 ++++++++++++++------
 ...ith-version-specific-regr-issue-37a27bfdc575.md |   14 +
 ...-stores-raw-truncated-wor-issue-6e08d89b55e3.md |   16 +
 .../fcs-objective-speed-defuse-REG-2026-11-22.md   |  134 ++
 docs/specs/dev-loop/agent-coordination.md          |    9 +
 docs/specs/dev-loop/agent-self-improvement.md      |   21 +
 docs/specs/dev-loop/build-time-observability.md    |   10 +
 docs/specs/gamemodes/bloodtest.md                  |  206 +-
 docs/specs/gamemodes/bodycam.md                    |  195 +-
 docs/specs/gamemodes/funcounterstrike.md           |   41 +-
 docs/specs/gamemodes/gamemodes.md                  |   22 +
 docs/specs/gamemodes/teamdeathmatch.md             |   21 +
 docs/specs/npc-behavior/npc-behavior.md            |   61 +
 docs/specs/weapons/weapons.md                      |   12 +
 src/camera.cpp                                     |   61 +-
 src/camera.h                                       |   11 +
 src/config/camera-config.cpp                       |   11 +
 src/config/camera-config.h                         |   14 +
 src/devtools/terminal.cpp                          |   16 +-
 src/engine/engine-tick-camera.cpp                  |   52 +-
 src/engine/engine-tick-setup.cpp                   |    8 +-
 src/engine/engine-tick.cpp                         |   10 +-
 src/gamemode/gamemode-runtime-resolver.cpp         |   15 +
 src/gamemode/gamemode.cpp                          |   43 +-
 src/gamemode/gamemode.h                            |   20 +-
 src/network/server-gamemode.cpp                    |  389 +++-
 src/network/server-gamemode.h                      |    3 +
 src/network/server-npcs.cpp                        |   14 +
 src/network/server-packet-chat.cpp                 |  125 +-
 src/npc/npc.cpp                                    |  245 +-
 src/npc/npc.h                                      |   21 +-
 src/perf/perf-frame.cpp                            |  270 +++
 .../movement/physics-collision-dispatch.cpp        |    3 +
 src/render/post-fx.cpp                             |    9 +
 src/render/post-fx.h                               |   11 +
 src/terminal/debug-commands.cpp                    |   74 +
 107 files changed, 6369 insertions(+), 992 deletions(-)
```

## Captured changed paths

```text
M	.gitignore
M	config/accounts/default.json
M	config/analytics.json
M	config/behavior-profiles.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/bombtag.json
M	config/gamemodes/connected_world.json
M	config/gamemodes/counterstrike.json
M	config/gamemodes/duel.json
M	config/gamemodes/elimination.json
M	config/gamemodes/ffa.json
M	config/gamemodes/funcounterstrike.json
M	config/gamemodes/jugger_horde.json
M	config/gamemodes/juggernaut.json
M	config/gamemodes/juggernaut_duel.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/gamemodes/npc_waves.json
M	config/gamemodes/payload.json
M	config/gamemodes/retrograd.json
M	config/gamemodes/rocket_duel.json
M	config/gamemodes/sandbox.json
M	config/gamemodes/ssbm.json
M	config/gamemodes/survive_disasters.json
M	config/gamemodes/tdm.json
M	config/gamemodes/vehicle_arena.json
M	config/gamemodes/vs_jason.json
M	config/gamemodes/zombie_tower.json
M	config/hot-reload/packet-ledger.json
M	config/movement/movement-source.json
M	config/npc-difficulty.json
M	config/weapons/rocket_launcher/config.json
M	devscripts/agent-wake.py
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/cpu-wpr-capture.ps1
M	devscripts/dev-loop.py
M	devscripts/dev_loop/build.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/dev_loop/runtimes.py
M	devscripts/game_build_worker.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-isolated-test-run-failure-journal-masks-first-st-issue-79e68f63fb01.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
M	docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b7216286bdcc.md
M	docs/issues/build-friction/20261120-game-dll-build-405-fails-compile-groundstate-has-issue-b91438ac2972.md
M	docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-1bf97a27238f.md
M	docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md
M	docs/issues/build-friction/20261120-game-dll-build-publication-fails-replacing-share-issue-61b5edc937d9.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-7d013b4a6c77.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-at-setup-issue-dff7e8c0f758.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-3dad54c7224d.md
M	docs/issues/build-friction/20261120-wait-over-budget-exe-build-queue-at-build-pendin-issue-e9f3b2727a9a.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-25b14f436a3e.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-40dfaa2a803c.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-6413477d361d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-9a9807fa0128.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-b0ba8698acd7.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-bb273ac1386c.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-dd54a79216ad.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/improvement/20261121-track-proven-behavior-with-version-specific-regr-issue-37a27bfdc575.md
M	docs/issues/tech-debt/20261122-wait-issue-reason-field-stores-raw-truncated-wor-issue-6e08d89b55e3.md
M	docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/specs/gamemodes/bloodtest.md
M	docs/specs/gamemodes/bodycam.md
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/gamemodes/gamemodes.md
M	docs/specs/gamemodes/teamdeathmatch.md
M	docs/specs/npc-behavior/npc-behavior.md
M	docs/specs/weapons/weapons.md
M	src/camera.cpp
M	src/camera.h
M	src/config/camera-config.cpp
M	src/config/camera-config.h
M	src/devtools/terminal.cpp
M	src/engine/engine-tick-camera.cpp
M	src/engine/engine-tick-setup.cpp
M	src/engine/engine-tick.cpp
M	src/gamemode/gamemode-runtime-resolver.cpp
M	src/gamemode/gamemode.cpp
M	src/gamemode/gamemode.h
M	src/network/server-gamemode.cpp
M	src/network/server-gamemode.h
M	src/network/server-npcs.cpp
M	src/network/server-packet-chat.cpp
M	src/npc/npc.cpp
M	src/npc/npc.h
M	src/perf/perf-frame.cpp
M	src/physics/movement/physics-collision-dispatch.cpp
M	src/render/post-fx.cpp
M	src/render/post-fx.h
M	src/terminal/debug-commands.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-23T18:39:16.832027Z",
    "latest_successful_build_id": "2026-11-23T18:39:16.832027Z:6ac084a7c89b83dd9bbe2ed3689e1554456de15b420c53f2770fe536db5b991c",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "7a161a57bbb4fb286eb8579e9be22f6f1a7f32fb5da4d4996d9cf0a8c04a501e",
    "dll": "build\\game\\generation-569\\game-g569.dll",
    "generation": 569,
    "revision": "10a9e09076006cc69b31302befba6e49c86f131b1e0e0fa1fddfe137a21bef21",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-569\\game-g569.dll",
    "generation": 569,
    "published_utc": "2026-11-23T18:38:24.419Z",
    "revision": "10a9e09076006cc69b31302befba6e49c86f131b1e0e0fa1fddfe137a21bef21",
    "status": "ready"
  },
  "progress": {
    "compiled": 5,
    "elapsed_s": 35.383,
    "phase": "build_complete",
    "result": "success",
    "skipped": 82,
    "updated_utc": "2026-11-23T18:39:13.965Z"
  }
}
```
