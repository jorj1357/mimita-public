# Generation 574 build latency evidence

- Session ID: work-18d0f741b81a
- Date: 2026-11-23
- Scope: record the completed Mode 3 generation574 build, resource sidecar, source fanout, and live activation evidence.

## Finding

Attempt `game-dll-20261123T225702279702Z-0ed089` published generation574. The request started at22:57:01.080Z and client activation was journaled at23:18:29.843Z: 1,288.763s from request acceptance to client activation. Worker runtime was1,292.062s (22:57:02.280Z–23:18:34.342Z). The client’s first gameplay tick followed at23:18:30.340Z. This is far above the <=10s target.

The batch changed seven compiler inputs and compiled158 units; it does not satisfy the requested 15–20-file acceptance case. Changed-source preflight took156.500s, object scan2.725s, compilation1052.461s, LLD link28.943s, coherence0.665s, and publish0.470s. Link alone exceeded the entire target. Header invalidation caused143 dependent TUs from `camera.h`,8 from `ragdoll-mode.h`, and2 from `post-fx.h`, plus four changed `.cpp` units and the generated identity provider. The remaining485/643 units were skipped.

## Resource and runtime evidence

The automatic sidecar sampled521 times over22:57:05.246–23:18:33.212Z on12 logical CPUs. Host CPU averaged99.997%; RAM available averaged1.86GB and reached165MB minimum; pagefile-used averaged37.21GB. Client/server PIDs32832/22184 averaged13.305%/11.970% of whole-host CPU. Compiler/linker roles averaged9.012% host. Concurrent top observed process means included RobloxPlayerBeta13.624%, FL649.659%, and SNAPOS64 7.996% host. These samples establish concurrent resource pressure but do not quantify its causal effect on build duration.

The same build interval contains a server tick row verifying Juggernaut Horde on `dust2cyberiav4` with67 NPC/1 player. No same-window client FPS event was captured. Client journal proved generation574 activation, first gameplay tick, and later first snapshot. Server PID22184 had `game-g574.dll` loaded in a process-module inspection, but its journal was capped and had stopped updating; server activation event is unproven.

The resource sidecar used the older run-context association, attaching the root Bloodtest run instead of the isolated pair’s journals. The per-PID sampler fix was saved after this attempt began and still needs a root-daemon reload plus a later build sample to prove it is live.

A separate no-build resource sample from the resource-profiler, `.dev/perf-captures/postbuild-gen574-pids-20261123T2326Z.csv`, spans23:28:38.732–23:28:52.137Z. Host CPU remained99.991%; available RAM averaged1.96GB (minimum1.90GB). Five MiMITA processes accounted for about38.93% host CPU, including the Juggernaut server/client at10.942%/16.720%. Root Bloodtest journal confirmed0 NPC/0 players; Juggernaut journals were stale/capped and another pair's workload was unknown. Treat this only as post-build host/process context, not a matched workload or causal baseline. The sampler owner advised against restarting the root daemon while batch generation46 has six pending inputs that may be lost. No daemon restart/build was performed; a persistence-safe reload plan is still needed before proving the sampler update.

## Next experiment

First reduce avoidable header fanout (including moving suitable inline definitions out of shared headers). Then coordinate a game-only batch with15–20 changed `.cpp` files, fresh paired runtime journals, and the corrected per-PID sampler. Measure one root Mode3 request through client/server activation. Since the current monolithic LLD phase took28.943s, a coarse game-plugin DLL boundary or incremental-link experiment is required for the <=10s target; keep the current MinGW DLL path as fallback until runtime behavior and activation are proven.

## Failed retained-batch attempt

The next root attempt, `game-dll-20261123T234301570308Z-6c7a9c`, was requested23:43:00.717Z and failed23:46:14.757Z after193.092s in attempt time (194.040s request-to-failure). It did not publish a DLL; the seven paths remain pending. Changed-source preflight consumed157.351s. GCC failed at `src/network/multiplayer-projectiles.cpp:443`: `projectileTrailConfigFor` was used before declaration; its definition appears at line513. The compiler timing report for that TU attributed75.68s to parsing,35.90s to template instantiation,29.03s to preprocessing, and15.95s to overload resolution. The exact failure was sent to the active feature agent for owner confirmation; no source was changed by this session and no retry was requested.

Its automatic sidecar collected94 samples on12 logical CPUs. Host CPU averaged99.990% (range99.859–100.0%); available RAM averaged1.409GB and fell to49,664,000 bytes (~47.3MiB); pagefile use averaged36.26GB/45.55GB. The top-process observations attributed about31.5% host CPU to MiMITA PIDs32832,22184,10164, RobloxPlayerBeta16.755%, FL64 9.550%, and SNAPOS64 8.037%. `cc1plus.exe` appeared at1.496% host in18 observed rows only; the top list is partial and discovery-sampled. The resource artifact lacks a matched pre-build baseline, fresh same-run client/server workload rows, and host commit charge. It demonstrates concurrent severe resource pressure but not causal slowdown or total compiler CPU.

The core latency read remains: header fanout causes unnecessary recompiles, heavy translation units make even preflight expensive, and the monolithic LLD link's28.943s already exceeds the total10s target. Header/code hygiene is necessary but cannot alone meet the target; a separately linkable hot gameplay boundary or a successful incremental-link method is also required. Full issue evidence: `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.

Resource-profiler later reconciled the full process-role records for the failed attempt. Correct whole-host averages are MiMITA PIDs10164/22184/32832 combined30.498%, six `cc1plus` entries each about0.8–1.2%, `compiler_linker` role3.046%, RobloxPlayerBeta PID26216 16.755%, FL64 PID32712 9.550%, SNAPOS64 PID7384 8.037%. The earlier 31.5% MiMITA and1.496% compiler values were only an aggregation of entries that appeared in the top-process list. Exact isolated-run workload and pair generation remain unverified for that interval.

After the failure, the existing watcher PID5936 spawned root daemon PID26308 at23:54:39.984Z. Both report source-set hash `9bf07b600a23dfcb98f860a4eb65d1e05152a8944d0665675f54f7d370a4afa3`; root run ID is `20261123_235443`. The root daemon restored the pending batch, which had accumulated to14 game inputs; there was no active native build at the last status read. I did not request the daemon restart. The sampler source set is now loaded, but a future build sidecar must prove the per-PID runtime/journal mapper is operating correctly.

Old Juggernaut server PID22184 remained alive at inspection while client PID32832 was absent, so this was not a paired runtime. Resource-profiler reported a separate fresh TDM run `20261123_234937`; its journal-only tick/FPS evidence is not build-overlap CPU/RAM proof and does not meet the Juggernaut acceptance workload. No process was relaunched by this session.

Full evidence and caveats: `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.
