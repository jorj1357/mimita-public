# Mode 3 Latency — Module Generation Refresh Source Trace

## Current measured state

At 2026-11-24 12:50:40 UTC, root dev-loop reported no active build and no pending batch. Source generation was 77 and the published game DLL was generation 594. The root status was `command_queued` on run `20261124_122748`, Bloodtest/slaughterhouse1, server PID 25840 and client PID 34756. The runtime registry's server journal still belongs to run `20261124_121332`, so this is not a clean registry-matched pair. The client journal reports Bloodtest; the server journal reports one NPC and one player. Server host DLL generation is 594 while its NPC module generation remains 593.

Available RAM was 225,841,152 bytes out of 16,802,918,400 bytes. The CPU-reduction agent recommended at least 6 GiB free before adding a 67-NPC Juggernaut run. No new pair, build, or CPU sample was started.

## Source-level generation mismatch

`ModuleHost::loadLocalModules` in `src/hot-reload/module-host.cpp:785-884` resolves each local module export from `HostRuntime::activeGameModule()`, initializes it, stores `entry.api` and `base->generation`, then sets `mLocalModules=true`. The engine and server loops call `ModuleHost::poll()`, but `src/hot-reload/module-host.cpp:1157` returns immediately when `mLocalModules` is true. Local APIs therefore do not refresh when the host swaps its game DLL.

`HostRuntime` shuts down the old game API and commits the candidate at `src/host/host-runtime.cpp:2709-2715`. `HostLoader::commit` retains the old `HMODULE` in `retiredModules_` before replacing the active generation (`src/host/host-loader.h:193-207`). This explains how a callback from the previous DLL can remain callable while the host reports a newer generation; it is a source-derived explanation consistent with the live g594/g593 journal mismatch.

The module ABI already supports `initialize`, `shutdown`, `exportState`, and `importState` (`src/modules/api/module-abi.h:152-162`). The NPC behavior module currently exports zero state and accepts only a zero-byte state import (`src/npc/npc-behavior-module.cpp:937-945`). A safe local-module rebind can therefore be prototyped by resolving the active DLL's API at a quiescent generation boundary, initializing and importing into the candidate before replacing the old API, retaining the old API on any failure, then shutting down the old callback and updating the module generation. This remains a proposal; it has not been implemented or runtime-proven, and other local modules may have different state requirements.

## Separate-module path and authority boundary

`config/modules/modules.json` is absent in the current checkout. The current `build/source-manifest.json` classifies `npc-behavior-module.cpp`, `npc-perception-module.cpp`, and their implementation files as active `owner=game`, so these translation units are part of the whole-game DLL. `devscripts/dev-loop.py:495-515` still contains catalog-based discovery of module-owned source paths, but it resolves no module sources without the catalog. This matches the hardcoded local-export path in `ModuleHost`.

The build-authority specification's separate ModuleHost worker rule is in its historical Mode 2 section; current Mode 3 requires the root dev-loop to own builds. Therefore restoring the old catalog alone or letting a runtime ModuleHost launch its module worker would not satisfy current authority. A speed prototype needs a root-dev-loop-owned module build target and activation route, or a deliberate authority-spec change, while retaining the monolithic game DLL as fallback. This is the strongest architecture candidate found so far for avoiding a 644-object link on behavior-only changes, but it has not been implemented or measured.

## Latency implication

Fixing local API refresh is required for live NPC behavior proof, but it does not make the monolithic build faster. The latest one-source attempt `game-dll-20261124T122208532874Z-63061a` took 99.428 seconds with zero cache invalidations: changed-source compilation took 60.521 seconds and LLD took 22.383 seconds across 644 object inputs. Reaching <=10 seconds requires a separate latency change, such as a root-dev-loop-owned coarse hot module that compiles and links far fewer inputs; keep the existing game DLL path as fallback until the candidate is proven.

## Coordination and validation

The exact source trace and proposed rebind safety conditions were sent to the active `hot-reload-latency-root` owner on issue `issue-136d4553ff92`. No source files were changed. `git status` showed no modifications in the inspected module-host, host-loader, module-ABI, or NPC behavior module sources. Documentation-only validation: checked the new file contents and `git diff --check` after writing.

At 12:56:08 UTC, root status still showed no build or pending batch, game DLL generation 594/source generation 77, Bloodtest/slaughterhouse1 on run `20261124_122748` (server PID 25840, client PID 34756). Available RAM was 542,445,568 bytes. No CPU sample, build, or new pair was started; this remains below the resource-profiler's 6 GiB gate.

## Latest build/runtime gate recheck

At 2026-11-24 13:05:42Z, root dev-loop was idle with no pending batch and the published build still identified as generation 594. Its effective shared runtime remained Bloodtest on slaughterhouse1 with one NPC and client PID 34756; this is not the required Juggernaut Horde pair, and the runtime registry still did not provide a clean matching server/client identity. Windows reported 0.59 GiB available physical memory on a 12-logical-CPU host. This was a point-in-time memory check only: CPU load, process-role CPU, frame/tick interval, and sample duration were not measured. No build, process change, or new runtime was started. The corresponding low-memory / workload-identity gate was appended to issue-136d4553ff92.

## Compiler timing and the current link floor

The generation 594 build record for `src/network/server-projectiles.cpp` includes a parsed GCC `-ftime-report`. The translation unit took 60.521 seconds wall time. The report’s phase rows total about 45.61 seconds: parsing 32.10 seconds (70% of reported compiler phase time), deferred parsing 4.16 seconds, and optimization/code generation 9.21 seconds. The report does not account for the entire measured translation-unit wall time, so those subphase numbers are not additive to 60.521 seconds.

This source currently uses the shared GCC PCH: it is absent from `PCH_EXCLUDED_SOURCES`, the compile path keeps `-include src/pch.h`, and the current `src/pch.h.gch` file is 121,165,108 bytes. The existing PCH-off policy was retained for 15 other sources after they compiled faster without the shared PCH, but this TU has no matched PCH-on/off result. After the resource gate clears, a root-dev-loop-owned comparison on this unchanged TU is a focused next test. This may reduce compile time, but cannot by itself meet the <=10-second target: the same attempt spent 22.383 seconds in LLD alone across 644 object inputs.

At 2026-11-24 13:18:12Z, the root remained idle on Bloodtest/slaughterhouse1 with one NPC and client PID 34756; no server PID was registered. Free physical memory was 0.30 GiB on the 12-logical-CPU host. No build, CPU sample, or runtime control was started. The PCH comparison and a Juggernaut latency run remain gated on usable memory and clean runtime identity.

## Host memory point snapshot

At 2026-11-24 13:25:03Z, a one-time Windows process inventory took 0.49 seconds and reported 1.46 GiB free RAM on a 12-core, 15.65-GiB host. Memory Compression had a 2,812-MiB working set; ChatGPT, OpenCode, C++ language services, Firefox, Defender, Python, Settings, and Code were also among the largest listed working sets. The registered MiMITA process was client PID 34756; a second `mimita` PID 25840 existed but its role was not verified. Those two processes totaled 95 MiB working set / 627 MiB private bytes; they are not a confirmed pair. Adjacent root status showed Bloodtest/slaughterhouse1 with one NPC and client 34756, no registered server, no active build, and no pending batch. This is a memory inventory only: it cannot establish which processes are reclaimable or explain build CPU time. No process was stopped. Free RAM remains below the profiler's 6-GiB recommendation for starting a 67-NPC target run.

## Bounded no-build process and client-frame sample

A passive 12.44-second capture ran from 2026-11-24 13:31:21.494Z to 13:31:32.938Z against fixed PIDs, using Windows `GetSystemTimes`, `GetProcessTimes`, `GlobalMemoryStatusEx`, and `GetProcessMemoryInfo`; it did not enumerate all processes per row. Raw rows and method details are in `.dev/perf-captures/mode3-bloodtest-host-pid-baseline-20261124T133121Z.json`. Across 12 samples, host CPU averaged 94.83% (84.77–100%), and available RAM averaged 1,586.8 MiB (minimum 1,388.9 MiB). Registered client PID 34756 averaged 208.13% of one logical CPU, or 17.34% of 12-core host capacity; PID 25840 averaged 142.71% of one logical CPU / 11.89% host, but its role is unverified. Together the two MiMITA processes used 29.24% host CPU; do not treat them as a proven client/server pair.

The run was `20261124_122748`, Bloodtest/slaughterhouse1. Client journal rows overlapping the sample contained 56 frame slices / 2,086 frames, with 1,162 (55.7%) over the logger's 4 ms budget, weighted mean 5.208 ms, and maximum frame / slice p95 59.585 ms. The client journal reported one player and zero client NPCs; adjacent root status reported one configured NPC, client PID 34756, and no registered server PID. Root status after the capture remained idle with no pending build. This is current resource-pressure evidence for a light Bloodtest client context, not the required Juggernaut pair, not a build-overlap sample, and not evidence that this runtime load caused earlier compilation latency.
