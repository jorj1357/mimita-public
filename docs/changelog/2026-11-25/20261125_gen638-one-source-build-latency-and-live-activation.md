# Generation 638 one-source build latency and live activation

## Observed build

Attempt `game-dll-20261125T053302586316Z-919e8d` published game DLL generation 638 from source generation 7. Its only changed source path was `src/network/multiplayer-projectiles.cpp`. The attempt summary reports 74.750 seconds total; its resource summary reports 74.734 seconds.

The worker timing record separates the main costs:

| Stage | Wall time |
|---|---:|
| Changed-source preflight / edited TU compile | 52.844 s / 52.701 s TU |
| Object-cache scan | 0.075 s |
| Remaining compile phase | 1.172 s, including the generated module identity provider |
| LLD link | 11.156 s (11.078 s linker subphase) |
| Coherence validation | 0.453 s |
| Publish | 0.469 s |

Only the edited TU and generated identity provider compiled; there were no header-dependent invalidations. The optional GCC timing report for the edited TU attributes 31.91 seconds (66%) to parsing, 6.76 seconds to deferred language work, and 9.69 seconds to optimization/code generation. This is compiler phase wall timing, not function-level attribution.

## Matched resource and runtime evidence

The automatic sidecar contains 50 approximately one-second samples from 05:33:06.366Z to 05:34:17.312Z on a 12-logical-CPU host. Mean host CPU was 93.79%, RAM use 72.82%, and available RAM averaged 4.25 GiB with a 3.83 GiB minimum. Two Bloodtest server/client pairs were present. Across the sample, their process means were:

| Run / role | PID | CPU, one logical CPU | CPU, whole host | Mean working set |
|---|---:|---:|---:|---:|
| `20261125_052415` client | 13524 | 239.0% | 19.92% | 735 MiB |
| `20261125_052415` server | 10396 | 127.8% | 10.65% | 155 MiB |
| `20261125_052838` client | 8040 | 128.4% | 10.70% | 572 MiB |
| `20261125_052838` server | 14688 | 60.7% | 5.06% | 133 MiB |

The sidecar identifies the server workload as Bloodtest with two NPCs and one player, but independent client-mode confirmation is missing. No Juggernaut workload was running, and there is no pre-build baseline. These measurements show heavy host CPU contention during the build; they do not establish that the game pair caused the compile latency.

Run `20261125_052838` journals prove generation 638 activation in the same processes: server PID 14688 at 05:34:17.393Z and client PID 8040 at 05:34:17.619Z. The sidecar completed at 05:34:17.331Z, so the client activated 0.288 seconds later. Server hotness rows report generation 638, two NPCs, and one player. This proves activation timing after publication for Bloodtest, not the requested Juggernaut behavior or <=10-second edit-to-live target.

## What this changes about the next step

This one-source build took about 75 seconds despite zero header fanout and a 75 ms object-cache scan. Header hygiene remains useful for multi-file edits, but it cannot explain or remove this attempt's 52.7-second edited-TU compile or 11.1-second monolithic link. For this sample, the edited-TU front end and the full-DLL link must be addressed before a 15–20-file, <=10-second target is plausible. A behavior-layer DLL or function-level patch path needs its own isolated prototype and measured build/activation data; this record does not claim either approach is already effective.

The build and CPU issues received the attempt, resource gap, process identities, mode caveat, and activation evidence. No source, compiler configuration, runtime, or build authority was changed in this investigation.

## Current module boundary audit

The existing NPC behavior module API is not an independent DLL today. `devscripts/game_build_worker.py` lists `src/npc/npc-behavior-module.cpp` in `FOLDED_MODULE_SYMBOLS`; its export is compiled into the monolithic game DLL. `ModuleHost::startup()` calls `loadLocalModules()`, which resolves the module API with `GetProcAddress()` from the active game DLL. The local path sets `mLocalModules=true`, and `ModuleHost::poll()` returns immediately for local modules. Startup does not fall back to the external catalog path. In the current checkout, `config/modules/modules.json` and `devscripts/module-build-worker.py` are absent even though legacy code still refers to both.

This confirms that there is no active separate module build/reload path to turn on with a config change. Reintroducing one must route build requests through the root dev-loop; reviving the old in-process worker launch would conflict with the current single-builder rule. The existing deferred hot-layer design also says coarse DLLs reduce link scope but do not remove the cost of compiling every changed C++ file; 50 files at one second requires a non-C++ compile-on-save behavior layer or equivalent. This is architecture evidence, not proof that a separate DLL or JIT will meet the current 15–20-file/10-second target.

The next bounded prototype should isolate one NPC decision behavior behind a stable, versioned boundary, preserve live state across activation, and measure edited-source compile, component link, activation, and first visible behavior in a pinned Juggernaut run. Header fanout reduction remains complementary for source that stays in the monolithic game DLL.
