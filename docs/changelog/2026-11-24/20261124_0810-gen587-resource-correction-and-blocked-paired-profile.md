# 2026-11-24 — Gen587 build-pressure and blocked CPU profile

Session: `resource-profiler`, work session `work-c70f374b4637`. Issue: `issue-ed3ac775283d`.

## Work completed

- Rechecked root dev-loop state and the active Juggernaut run after the game-DLL build completed.
- Parsed the automatic resource sidecar for attempt `game-dll-20261124T075315478413Z-04a3b7`.
- Cross-checked server and client journals for mode, player/NPC count, module generation, tick timing, and frame timing.
- Appended evidence and explicit limits to [the CPU resource issue](../../issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md).

## Observed evidence

The 221.684-second build changed only `src/vehicles/vehicle-system.cpp`; preflight was 101.132s, compile 4.199s, link 53.739s, and publish 0.554s. Its 176 resource samples show host CPU averaging 99.994% on 12 logical CPUs and memory use averaging 97.1%, with only 17,457,152 bytes available at the minimum. Server PID 380 averaged 8.372% whole-host CPU / 100.5% of one logical CPU; client PID 34868 averaged 17.726% whole-host / 212.7% of one logical CPU. Two other MiMITA PIDs were also active. These are build-window process metrics, not proof that one matched Juggernaut pair caused host saturation or build delay.

The resource sidecar explicitly has `runtime_alignment.status=unavailable`. The server journal reports Juggernaut, 67 NPC, 0 connected players; the client has no authoritative in-sample mode and reports a different NPC-view map. After the build, server tick windows averaged 40.878ms with p95 77.894ms and max 109.531ms; the server did not meet the 60Hz timing requirement. NPC phase logs show 550.317ms total wall time, including physics 312.051ms and nested collision 229.047ms; this is not CPU time by function. A client FPS record during the build had 93.92 FPS and 86/94 frames above 4.167ms. The current server host DLL is generation 587 while NPC phase logs still say module_generation 586, so the bounded NPC CPU probe was not run.

## Decision / next step

No runtime optimization was selected from these data: they do not provide a clean matched pair or per-function CPU attribution. The next valid experiment needs an authoritative same-run connected server/client, host and NPC-module generation aligned, then a no-build host/process baseline and the existing bounded server callsite probe. Compare any subsequent single-owner change against matching all-frame client and fixed-tick evidence.

## Scope

No source or runtime setting was changed; no new build or manual CPU capture was started. No tests were run.



## Follow-up: child server resolves the apparent disconnect, but not the pairing

A live check of run `20261124_081518` found that the registered root server PID28456 (Juggernaut, dust2cyberiav4, 67 NPC, 0 players) is not serving client PID26360. The client’s authoritative mode event at08:22:43 is Bloodtest/slaughterhouse1; its actual server is child PID22208, parented to the client, with one NPC and one active player. Server events for PID22208 are mixed into the client journal rather than a separate registered server journal.

The child server reports module_generation587; its 61-tick window averaged2.477ms, p95 10.216ms, max13.707ms, with no >100ms tick. This is one window only. The client FPS event at08:21:01 reported178.36 FPS, p95 8.036ms, and151/179 frames over4.167ms, so the client still fails the frame target. The child is a possible sample target after its owner confirms; I have not captured it or sent profiler/admin commands. A fresh module_generation587 does not prove safe live hot-reload refresh.

This correction and next check were appended to issue-ed3ac775283d. The next sample must use the actual child PID plus client PID, record the effective Bloodtest workload, and preserve the journal-registration limitation.

## Follow-up: authoritative mode field correction

At08:39Z I read the sampler diff and received confirmation from `hot-reload-latency-root` that its explicit game-mode event allowlist and configuration-mode regression test are their in-progress changes. The current code ignores the later `dev_timer_tips.config_loaded` row when deriving `actual_mode`; `network.mode.active` remains authoritative. The sampler owner reports 12 focused tests, Python compilation, and scoped diff checks passed, but the root daemon has not reloaded the change. I appended the correction to issue-ed3ac775283d so prior sidecar `client_actual_mode=repeat` is not treated as a real mode mismatch. I did not edit sampler/test code, start a build, or capture/restart any runtime.
