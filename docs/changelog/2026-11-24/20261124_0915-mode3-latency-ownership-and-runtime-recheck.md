# Mode 3 latency ownership and runtime recheck

At 2026-11-24 09:09 UTC, root dev-loop had no active build and one pending
Mode 3 game batch, generation 30. The pending set contained 16 paths:

- `src/network/network-weapons.cpp`
- `src/gui/ui-system.h`, `src/gui/ui-system.cpp`, `src/gui/ui-system-render.cpp`
- `src/engine/engine-tick-ui.h`, `src/engine/engine-tick-ui.cpp`,
  `src/engine/engine-tick.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`
- `src/debug/debug-visuals.h`, `src/debug/debug-visuals-labels.cpp`
- `src/gamemode/gamemode.h`, `src/gamemode/gamemode.cpp`,
  `src/gamemode/gamemode-runtime-resolver.cpp`
- `src/config/camera-config.h`, `src/config/camera-config.cpp`
- `src/terminal/debug-commands.cpp`

No active edit leases covered these paths at the time of the check. Feature
owners were asked to confirm path ownership and readiness. This batch was left
pending and unsealed because its owners had not confirmed readiness and it is
not a focused latency experiment.

## Runtime proof status

The root server instance was PID 28456 in run `20261124_081518`. Its journal
was current through 09:09 UTC and reported Juggernaut Horde on
`dust2cyberiav4`, 67 NPCs, zero players, and NPC module generation 587. The
registered client PID 35508 belonged to run `20261124_084513`, was marked stale,
and had no fresh journal event. This is not a connected, matched client/server
workload and provides no valid paired runtime or activation proof.

## Latency interpretation

The previous published game DLL generation 587 took 221.691 seconds according
to root dev-loop. That is not a measurement of the pending generation-30 batch.
Historical dependency-index data from 07:38 UTC showed 334 header-to-translation
unit edges across the headers in this batch, with overlapping dependents; that
index predates the pending edits and is only a fan-out warning. Exact current
invalidations and compile/link costs remain unknown until the owners seal a
batch and root dev-loop reports per-source results.

Header fan-out reduction is a near-term way to avoid unrelated translation
units recompiling after common-header edits. It cannot alone make a broad
monolithic DLL link sub-10 seconds. A smaller root-dev-loop-owned behavior DLL
candidate is still the longer-term route, gated on correct same-process
`ModuleHost` rebinding and state-preserving activation.

No C++ files were changed, no build was requested, no runtime was restarted,
and no resource capture was started in this recheck.

## Follow-up process/journal reconciliation

At 09:18–09:21 UTC, a native Win32 process snapshot found that client PID 9368
was alive with command line `--room PF48HHV`; this matched root server PID
28456's room code. PID 9368 wrote fresh client events to the journal under run
`20261124_084513`, while PID 28456 wrote server events under run
`20261124_081518`. Root dev-loop still reported client PID 35508 as stale,
showing that its client process registry did not match the live PID.

The server journal briefly reported 67 NPCs and one player at 09:18:26 UTC,
then 67 NPCs and zero players at 09:19:37 UTC. The same client journal's last
frame/FPS event was at 09:18:18 UTC: 9.27 FPS, p95 187.69 ms, and 14 of 14
frames over the 4.167 ms budget. This is evidence of a short connection window
and a severely delayed client, not a sustained healthy pair or matched
performance baseline. Server NPC module generation remained 587. The discrepancy
was sent to the hot-reload and resource-profiler owners; no additional process,
build, capture, or game command was started.

## Connected pair recovered; batch still owned

At 09:22–09:23 UTC, root dev-loop reconciled its live client records. Server
PID 28456 remained online in run `20261124_081518`, and client-02 PID 31640 was
online in run `20261124_084513`, connected to server-01 at `127.0.0.1:60088`.
The client journal recorded `network.client.snapshot.applied` at 09:23:16.964
UTC for server tick 130400. The server journal reported Juggernaut Horde on
`dust2cyberiav4`, 67 NPCs, one player, and NPC module generation 587 around
09:23:13–09:23:16 UTC. This is direct evidence of a connected pair and matching
NPC module generation; it is not evidence of a new DLL activation.

That pair's client performance remains poor: its 09:23:16 FPS window reported
11.78 FPS and 15/15 frames over 4.167 ms; a 09:23:17 window reported 2.76 FPS
with frame p95 as high as 593.6 ms. Dev-loop marked the instances'
`build_match=false`, so this run must not be represented as an acceptance pass
or as a matched latency baseline without checking the executable/DLL identities.

The root batch advanced to generation 46 with 20 pending game paths, adding
`src/gui/gui-main.cpp`, `src/input/gamepad-input.h/.cpp`, and retaining
`src/network/multiplayer-tick.cpp`. The vehicle-arena-controller has active
leases on those four paths. No build is active. Its owner, the hot-reload owner,
and the resource profiler were sent the updated pair and batch state. The batch
remains unsealed until all owners confirm readiness; no profiler/build was
started in this follow-up.

## Mode 3 multi-file build measurements and repeated compile failure

Three root-dev-loop attempts for the shared game batch ran between 09:28 and
09:50 UTC. Their active build time sums to 710.5 seconds (11m50s); the full
first-start to last-failure span was about 22 minutes because attempts were
separated by source changes and retries. None published a replacement DLL; the
live/published game generation remained 587.

| Attempt | Inputs / result | Measured elapsed work |
|---|---|---|
| `game-dll-20261124T092811883920Z-29d505` | source generation 46, 20 game paths; failed before C++ compilation because `config/hot-reload/packet-ledger.json` was stale | 38.198 s total; packet codegen gate 5.555 s; no compile/link/publish |
| `game-dll-20261124T093339904951Z-946024` | generation 46; compile failed in `engine-tick-ui-game-hud.cpp`, `engine-tick-ui.cpp`, and `engine-tick.cpp` | 479.390 s total; compile phase 458.173 s; 92 TUs compiled and 3 failed before fail-fast; latest progress sample before failure showed 379 skipped of 645 examined |
| `game-dll-20261124T094655760219Z-f5a080` | source generation 47, 21 game inputs (the previous 20 plus `src/combat/explosion-fx.cpp`); same three UI TUs failed | 192.897 s total; changed-source preflight 37.790 s, object-cache scan 2.564 s, compile phase 108.176 s; 17 TUs compiled and 3 failed |

The failures are an API consistency problem before link: `engine-tick-ui.cpp`
passes four arguments to `uiBeginFrame` while `src/gui/ui-system.h` declares
three; `engine-tick-ui-game-hud.cpp` refers to undeclared `UISys` and missing
`DebugVis::projectToHudScreen`; `engine-tick.cpp` also refers to undeclared
`UISys`. The generation 47 retry repeated these errors, so it did not provide a
new latency or activation result. The three failed source files are owned by
the shared UI contribution; this agent did not edit them. The latest root state
again shows generation 47 pending with 21 game paths. The compiler batch should
not be repeated until that interface mismatch has a confirmed owner fix.

The dependency records quantify the header fan-out risk. Attempt 2 logged 265
object invalidations: `debug-visuals.h` 130, `ui-system.h` spellings 75,
`gamemode.h` 31, `camera-config.h` 6, `engine-tick-ui.h` 3,
`gamepad-input.h` 2, and 14 missing/invalid dependency files. Attempt 3 still
logged 174 invalidations: `debug-visuals.h` 77, `ui-system.h` spellings 53,
`gamemode.h` 25, `camera-config.h` 4, `gamepad-input.h` 3, nine missing/invalid
dependency files, and three missing objects. These six edited headers can
invalidate far more than the edited `.cpp` files. Attempt 3's object-cache scan
itself was 2.564 s (dependency-index prepass 1.129 s, unique-dependency stat
prewarm 0.538 s, freshness checks 0.827 s); header fan-out and compilation,
not this particular scan, dominated its elapsed build time.

Attempt 3 also shows why cache work alone is insufficient: its changed-source
preflight compiled just `src/combat/explosion-fx.cpp` and took 37.790 s under
this host load. The later compile phase took another 108.176 s and failed. The
run's resource sidecar recorded 111 samples over 194.961 s, 12 logical CPUs,
mean host CPU 99.988% (peak 100%), mean RAM use 92.30%, mean available memory
about 1.124 GiB with a minimum of 516,096 bytes, and mean pagefile use about
40.62 GiB. Presence-conditioned top process means included RobloxPlayerBeta
15.44% host, FL64 10.21%, MiMITA client PID 31640 7.42%, server PID 28456
5.43%, cpptools 4.11%, and Python build daemon 2.64%. These observations show
severe simultaneous resource pressure, but do not prove that it caused a given
compiler's slowdown. The sidecar could not match server/client journals because
they were registered under different run IDs; the root snapshot reported
Juggernaut Horde on `dust2cyberiav4`, but exact in-window roster, frame, and tick
alignment remains unknown.

The next latency work is therefore gated on fixing the UI compile errors and
reconciling the pending mixed-owner batch. Once it is safe to build, measure a
single small `.cpp` change separately from a 15–20 input batch under the same
verified live workload. To get close to 10 seconds, reduce common-header
fan-out and the 37.790-second direct compile cost, then isolate frequently
edited behavior behind a small replaceable DLL boundary so unrelated game
translation units do not rebuild. Neither a candidate DLL nor a successful
same-process activation was produced in these attempts. No source, build mode,
dev-loop authority, or runtime process was changed by this investigation.

## Compiler front-end breakdown for one source edit

Attempt `game-dll-20261124T094655760219Z-f5a080` also captured GCC's
`-ftime-report` for the directly changed `src/combat/explosion-fx.cpp`: 230
lines, 10.9 KB, and 10 project includes. It compiled in 37.793 s with 0.883 ms
queue wait. GCC assigned 23.12 s (68%, 327 MB in its GGC report) to parsing,
5.47 s (16%) to language-deferred work, and 5.25 s (15%) to optimization and
generation. Nested report entries include preprocessing 8.86 s, template
instantiation 6.11 s, inline function-body parsing 4.32 s, global parser
3.71 s, and overload resolution 3.26 s. These are compiler-reported phase
timings for one TU under this build's host pressure, not whole-host CPU shares
or proof that pressure caused the compile duration. The result narrows future
TU optimization to include/template and front-end cost; an A/B is still needed
before changing includes, PCH use, or source boundaries.

## HUD compile blocker correction and current readiness

At 2026-11-24 10:21 UTC, the shared HUD source contained this unresolved call at `src/engine/engine-tick-ui-game-hud.cpp:194`:

- Before: `DebugVis::projectToHudScreen(camera, weapons.mPhysicalAimPoint, cx, cy)`
- After: `DebugVis::projectToScreen(camera, weapons.mPhysicalAimPoint, cx, cy)`

The replacement reuses the declared `DebugVis::projectToScreen(const Camera&, glm::vec3, float&, float&)` API in `src/debug/debug-visuals.h:187`; it has the same argument contract, projects into the renderer's HUD-facing screen coordinates, and the same translation unit already uses that function for objective text. This avoids adding a duplicate projection helper. Build note `b3d00b36d675` was queued before the source edit. Static search now finds no `projectToHudScreen` declaration, definition, or call. This change has not been compiled or runtime-proven. `git diff --check` still reports the pre-existing extra blank lines at the end of the in-progress HUD file; I left that unrelated formatting untouched.

The current root status after the correction is source/batch generation 53, with 21 game paths pending and no active build. The last attempt remains a failed generation-587 DLL build after 192.897 s; its three UI compile errors were from the earlier snapshot, and neither link nor publish ran. The root run `20261124_084513` currently reports server PID 28456, no client PID, Juggernaut Horde on `dust2cyberiav4`, configured NPC count 1, and `builds_match=false`; the journal's stored server PID differs from the root registry. It is not a valid connected 67-NPC acceptance run. I requested readiness/intent confirmation from `vehicle-arena-controller` and coordinated the next resource sidecar with `resource-profiler` and `build-coordinator`. No build or runtime command was started.

The file-edit lease release was initially given a SHA-256 value where the coordinator expected its Git blob hash, producing conflict `conflict-c1dde7354156`. No concurrent content change was observed. The coordinator decision `decision-b4ccee465b05` records the format mistake and confirms retaining the working source.

## Recovered matched generation 587 runtime baseline

Resource-profiler follow-up recovered a matched no-build baseline for run `20261124_084513`: server PID 28456 and client PID 31640, Juggernaut Horde on `dust2cyberiav4`, 67 NPCs and 1 player, with host DLL and NPC module both at generation 587. The bounded callsite probe ran 09:30:44–09:30:59 UTC and produced 15 server / 12 client windows. It estimated `updateOneNpc` at a median 2.19% of whole-host CPU and nested `doCollisions` at 0.78%; client render/simulate estimates were too sparse/noisy to rank. In the same interval the server's mean tick was 32.73 ms and all 930 sampled client frames exceeded 4.167 ms. The run was stopped afterward and the next gen588 build began later, so this is a useful pre-build runtime baseline, not evidence of build causality or a before/after optimization win. Detailed resource findings are appended to `issue-ed3ac775283d`.

The CPU owner proposed the static-wall-contact scan in `NpcSystem::updateOneNpc` (commit `186eebd23`) as a separate falsifiable A/B candidate; it remains a hypothesis. It is not included in the current 21-path batch and must stay separate until that batch is compiled and its shared-runtime owner releases the next test window. As of the latest root status, no connected client is registered, the current run is not build-matched, and the gen53 game batch remains pending. No retry, runtime command, or capture was started in this work step.


## Completed generation 590 build and measured pressure

Root attempt `game-dll-20261124T103652183287Z-745ef3` started at 2026-11-24T10:36:52.184Z and completed successfully at 10:53:22.048Z. It published `build/game/generation-590/game-g590.dll`. Total attempt time was 989.864 seconds (16m29.864s); the successful artifact was not activated in a live runtime during this observation.

The requested batch had previously been reported as 21 pending paths, but this attempt's source-manifest diff records only three changed `.cpp` files and one changed header: `src/engine/engine-tick-ui-game-hud.cpp`, `src/engine/engine-tick.cpp`, `src/gui/ui-system.cpp`, and `src/gui/ui-system.h`. It therefore does not prove the 15–20 changed-translation-unit target. Reconcile the batch path ledger with its captured source manifest before using it as a multi-file benchmark.

The build examined 645 units: 208 compiled and 437 skipped. The raw attempt JSONL recorded 204 cache-invalidation events, each matching one completed dependent translation unit. Causes after path normalization were `ui-system.h` 96, `debug-visuals.h` 70, `gamemode.h` 23, `gamepad-input.h` 3, `camera-config.h` 3, missing/invalid dependency files 8, and one missing object. The remaining four compilations were the three changed `.cpp` files and generated `module-identity-provider.cpp`. These counts make transitive header fan-out the clearest build-work multiplier in this attempt; they do not establish that each header was edited in this attempt.

| Stage | Wall time |
|---|---:|
| Changed-source preflight | 40.554 s |
| Object-cache scan | 2.913 s |
| Compile | 910.194 s |
| LLD link | 13.293 s |
| Coherence validation | 1.465 s |
| Publish | 0.800 s |

The object-cache scan's sub-times were 1.157 s dependency-index prepass, 0.430 s unique-dependency stat prewarm, and 1.272 s object freshness checks. The complete attempt also logged 1.465 s coherence validation, 0.271 s toolchain setup, 0.031 s identity/cache setup, 0.016 s identity-provider source generation, and 0.013 s vendor compile. Its named stage durations sum to 969.550 s; the worker reports 969.791 s, while end-to-end attempt duration is 989.864 s, leaving 20.073 s outside worker wall time to account for in a future timing reconciliation.

The resource sidecar captured 643 samples from 10:36:55.944Z to 10:53:22.025Z at a nominal one-second interval. Host CPU mean was 99.913% (minimum 92.436%, maximum 100%); memory use averaged 95.112% (peak 100%); available RAM averaged 702.2 MiB with a recorded minimum of 0; pagefile use averaged 37.586 GiB. `cc1plus.exe` appeared at a mean 4.74% whole-host CPU across 381/643 samples; `RobloxPlayerBeta.exe` averaged 16.09% across 641/643. Process means are presence-conditioned and are not a causal attribution of build slowdown.

Resource alignment remains unavailable (`fresh_server_and_client_journal_evidence_not_matched`). There was no matched pre-build baseline or same-run server/client journal join; root status showed no live client and configured NPC count 1. Thus the attempt proves a long compile under severe host pressure, not how many seconds pressure added or live Juggernaut behavior. The shared CPU issue `issue-ed3ac775283d` and build issue `issue-136d4553ff92` now contain these observations. Brain gap key: `build-resource-sampling:game-dll-20261124T103652183287Z-745ef3`.

Next discriminating work: retain the automatic sidecar, align exact process IDs and live journals, and measure one batch containing 15–20 genuinely changed `.cpp` inputs only after pending owners report readiness. Prioritize stable narrow interfaces for the highest invalidation families (`ui-system.h`, `debug-visuals.h`, then `gamemode.h`) and preserve object-cache correctness. Since current LLD link alone took 13.293 s, reaching a 10 s end-to-end budget also requires a measured link reuse/incremental-link or smaller independently reloadable code boundary; cache-scan optimization alone cannot meet it. The current run had no live pair, so generation 590 activation and behavior remain unproven.

## Generations 591–593: compile time is hidden inside preflight

I re-read the raw attempt JSONL and source for three later attempts. This provides a direct answer to why unchanged translation units compile: in generation 591, eight changed `.cpp` files plus two changed headers caused 70 distinct dependency invalidations. All 70 invalidated paths appear among the 79 completed compiler units; the other nine are eight directly changed `.cpp` files and the generated module identity provider. The 70 dependents trace to 47 `gamemode.h` paths and 23 `ui-system-internal.h` paths. The summary JSON exposes 32 invalidation records while its raw JSONL contains 70 unique records; that count/list discrepancy needs reconciliation in the build report.

| Attempt | Changed inputs | Total | Changed-source preflight | Object scan | Compile phase | Link | Outside worker | Host CPU mean | Minimum free RAM |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `game-dll-20261124T111512931313Z-6d43ee` / gen591 | 8 `.cpp`, 2 headers; 79 TUs compiled | 410.254s | 58.719s | 5.421s | 314.875s | 7.757s | 21.026s | 99.953% | 589,824 B |
| `game-dll-20261124T112400384612Z-eb878a` / gen592 | 1 `.cpp`; 2 TUs compiled; 0 invalidations | 31.943s | 15.157s | 5.265s | 0.640s | 4.649s | 10.252s | 98.106% | 1,843,503,104 B |
| `game-dll-20261124T113118610851Z-05382c` / gen593 | same 1 `.cpp`; 2 TUs compiled; 0 invalidations | 181.029s | 74.564s | 5.256s | 4.453s | 51.680s | 47.173s | 99.997% | 47,779,840 B |

The gen592/gamepad input compile took 15.111s and gen593 took 74.481s. Both are counted inside `changed_source_preflight`, not the separately named `compile` phase. This is intentional in the current worker: `devscripts/game_build_worker.py:2194-2204` enters the preflight phase and calls `run_compile_batch(to_build, ...)`, then marks those outputs handled; the later `compile` phase compiles the generated identity provider and any remaining dependent work. Therefore “preflight” is not scan-only and understates the user-visible source compile unless the UI shows per-translation-unit timers or labels this as changed-source compilation.

The successive single-file attempts have zero cache invalidations yet total time varies 5.7x (31.943s to 181.029s). The slower attempt also has link time 11.1x longer (4.649s to 51.680s), and minimum free RAM falls from 1.72 GiB to 45.6 MiB. The sidecars show both hosts near 100% CPU and do not have exact same-run workload alignment, so this is strong pressure correlation, not a controlled causal estimate. Both attempts leave substantial end-to-end time outside worker timing (10.252s and 47.173s). These gaps and linker/resource attribution need stage owners; “one changed file” does not currently mean “one fast edit-to-live.”

No 15–20-file batch or live gen593 activation was proven in this observation. Current root status still names the shared Bloodtest run with 0 NPC, and only 0.73 GiB physical RAM was free at the latest snapshot. I did not start another game pair or build. The latency issue `issue-136d4553ff92` now has the raw comparison and source-location finding.

## Generation 593 manual runtime join correction

Resource-profiler later joined the attempt to fresh journals and exact PIDs, correcting the earlier “runtime alignment unavailable” limitation from the sidecar alone. Attempt `game-dll-20261124T113118610851Z-05382c` overlapped run `20261124_112426`, Bloodtest on `mimita-duels-map-v3`, server PID 5804 and clients 24384/34276. During 11:31:22.374–11:34:19.602 UTC, all three runtime processes were present; server journal showed 0 NPC / 2 players. The host averaged 99.997% CPU and 95.7% memory use with a 45.57 MiB minimum available. The three MiMITA processes averaged 35.02% whole-host CPU and about 1,249.7 MiB combined working set. Both clients missed the 4.167 ms frame budget on about 84–85% of sampled frames (weighted averages 12.36 ms and 13.77 ms). This is an exact build-overlap process/runtime join, but not the target Juggernaut workload or a no-build matched baseline.

The same run's journals reported host DLL generation 592 advancing to 593 near build completion. In a later no-build sample on the same PIDs, 11:46:43.209–11:47:02.211 UTC, host CPU remained 99.98%, the three MiMITA processes averaged 60.05% whole-host CPU, and their combined working set averaged 421.5 MiB. The server had changed to 1 NPC; the journal reported host DLL g593 while NPC behavior module remained g591. Client frame-slice rows were absent in this sample. This shows near-total host saturation persisted after the compiler exited, but the changed NPC count and missing client frame metrics prevent a causal build/no-build comparison. It also means host DLL activation alone does not prove NPC behavior activation.

At the latest root check the registered shared runtime was still Bloodtest with 0 NPC and two clients; free physical memory was 0.87 GiB. I did not add a second pair. A valid target run still needs a pinned, connected Juggernaut server/client, fresh 67-NPC/1-player journal proof, and matching host DLL/NPC module generation before any NPC behavior claim.

## Generation 594 single-source timing and current activation

Attempt `game-dll-20261124T122208532874Z-63061a` published game DLL generation 594 from source generation 77. Its manifest listed only `src/network/server-projectiles.cpp` as changed; the worker checked 644 units, compiled two (that translation unit and the generated module identity provider), skipped 643, and recorded zero cache invalidations. End-to-end time was 99.428 seconds. The `changed_source_preflight` phase took 60.600 seconds and included 60.521 seconds compiling `server-projectiles.cpp`; LLD took 22.466 seconds overall (22.383 seconds in the linker) across 644 inputs / 492,383,306 input bytes. Object-cache scan was 0.055 seconds, publish 0.847 seconds, and coherence validation 0.703 seconds. This attempt shows that header fanout did not explain this single-file delay; changed-source compile plus monolithic link consumed about 83 seconds. It does not establish why the TU or link was slow.

The automatic resource sidecar captured 92 samples over the 99.423-second attempt: mean host CPU 99.853%, peak 100%, minimum available RAM 312,864,768 bytes. It had no matched prebuild baseline and could not align runtime mode, server/client, frame, or tick journals; host commit charge/limit was unavailable. This is pressure correlation, not proof that pressure caused the compiler or linker time. Raw artifacts are `.dev/build-timings/game-dll-20261124T122208532874Z-63061a.json` and its `.resource.jsonl` sidecar.

At the 12:31 UTC root check, run `20261124_121332` was still Bloodtest on `slaughterhouse1`, server PID 25840 and client PID 12848. The live server journal reported host game DLL generation 594, one NPC, two players, while `npc.phase-window` still reported NPC module generation 593. Free physical memory at the 12:28 UTC snapshot was 1,399,037,952 bytes. No Juggernaut 67-NPC/1-player run or matching NPC module activation was proven; no new test pair or build was started.

## Matched pre-build baseline and current low-memory gate

Resource-profiler provided a pre-build sample on run `20261124_121332`, server PID 25840 and client PID 12848, Bloodtest/slaughterhouse1, one NPC/one player, generation 593 with matching NPC module. The sample ran 12:19:48.372–12:19:58.292 UTC, ending 130 seconds before the generation 594 build began. Host CPU averaged 99.99%; RAM use was 96.9%, with 498 MiB mean and 321 MiB minimum available. The pair averaged 23.14% whole-host CPU. Client mean frame time was 6.07 ms, with 69.83% above 4.167 ms; server ticks were derived at 61.7/s, mean 2.64 ms and maximum 31.72 ms. This is a Bloodtest pre-build baseline, not a Juggernaut acceptance sample.

The resource-profiler's manual build-window join found the same Bloodtest server/client and `cc1plus`, but the sidecar's automatic runtime alignment remained unavailable. Runtime processes averaged 21.12% host CPU and `cc1plus` averaged 3.58% across only 20 visible samples during the build; this limited observation does not show all compiler CPU or establish that host pressure caused the 99-second build. The agent recommends at least 6 GiB free RAM before launching an additional 67-NPC test pair.

At 12:36:42 UTC, a new point-in-time Windows memory query reported 410,095,616 bytes free of 16,802,918,400 bytes visible RAM. No CPU interval was taken. Root control status was `command_queued`, run registry `20261124_122748` listed client PID 34756 while server PID 25840 still wrote to the earlier `20261124_121332` journal; connected workload/mode therefore remain unverified. No build or new runtime was started. This RAM snapshot is below the 6 GiB test-launch gate.

## Journal identity correction after the 12:36 status

Read-only parsing of the newer rows confirms client PID 34756 emitted `network.mode.active` for Bloodtest/slaughterhouse1 at 12:36:42.742 UTC in `logs/2026-11-24/20261124_122748/client-01/events.jsonl`. Server PID 25840 continued writing to the prior run journal; at 12:37:45.725 UTC its `game.hotness.summary` reports generation 594, one NPC and one player, with adjacent `npc.phase-window` reporting module generation 593. This supports that the client was in Bloodtest and the server had a player, but the root registry still splits the client and server across run IDs 122748 and 121332, so the pair is journal-corroborated rather than registry-matched. No CPU sample was taken; the only free-RAM measurement remains 410,095,616 bytes at 12:36:42 UTC, below the recommended 6 GiB gate.
