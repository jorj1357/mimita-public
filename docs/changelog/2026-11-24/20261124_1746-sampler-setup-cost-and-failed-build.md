# Automatic sampler cost and failed DLL attempt

**Purpose:** Record the terminal resource and compile evidence for the long root DLL attempt, including the cost and gaps in its automatic sampler.

**Audience:** MiMITA resource investigators and Mode 3 build owners.

**Does not:** Identify a runtime MiMITA hot function, prove that host pressure caused the build failure, or authorize a sampler implementation change.

## Terminal attempt

Root attempt `game-dll-20261124T171210403561Z-968a7e` ended at 2026-11-24 17:36:53.955 UTC after 1,487.063 seconds. Its authoritative `build.failed` event says `failed_phase=compile`, with two failing files: `src/vehicles/vehicle-profile.cpp` and `src/vehicles/vehicle-system.cpp`. Both use `VehicleJumpProfile::impulse`, which the current type does not provide; the profile loader also references missing `heldForce`, `heldWindowSeconds`, and `minimumHoldSeconds`. The dev-loop status headline instead reported a changed-source preflight failure in `vehicle-system.cpp`, so the status summary and attempt result disagree. Generation 596 was not published; the previous generation 595 remained active.

## Resource sidecar

The sidecar completed with 250 samples over 1,487.047 seconds on 12 logical CPUs. Host CPU averaged 99.781% and peaked at 100%; available physical RAM reached a 28.2 MB minimum. The build itself failed under this pressure, but that coincidence does not prove pressure caused the compile errors.

The sampler reported 62.750 seconds of synchronous setup wall time versus 4.484 seconds of setup CPU, 208.797 seconds of sampling-thread CPU, 1,029.112 seconds cumulative process-discovery wall time, and 86.903 seconds cumulative runtime-context discovery wall time. Requested 1-second sampling averaged about 5.95 seconds between the 250 rows. The sampler's process discovery runs synchronously inside `BuildResourceSampler.start()` at `devscripts/dev_loop/build_resource_sampler.py:1101`, before the sampling thread starts at line 1112. This confirms startup discovery can delay the build boundary; the cumulative discovery totals are not proof that the build's main thread waited for that entire duration. The sampling thread's CPU time is about 14.0% of one logical CPU, or 1.17% of the 12-core host across this attempt.

The short follow-up attempt `game-dll-20261124T173829158368Z-8723b2` ended at 17:39:05.669 UTC after a changed-source preflight failure in `src/vehicles/vehicle-system.cpp`. Its 16 resource rows covered 36.609 seconds: host CPU mean 99.606%, available RAM minimum 985.7 MB, sampler setup 2.953 seconds, sampling-thread CPU 8.125 seconds, process-discovery wall 18.469 seconds, and runtime-context wall 0.729 seconds. Requested 1-second sampling averaged 2.168 seconds between rows, with an 8.734-second maximum gap. Its runtime alignment had same-run server/client PIDs, but the client mode was unverified.

## Runtime context and next check

The long attempt's same-run journals reported Bloodtest/slaughterhouse1, one NPC and one player, server PID 32620 and client PID 29884. The client frame sample at 17:36:57.302 reported 161.7 FPS, p95 13.70 ms, and 85 of 162 frames over the 4.167 ms budget. Server mode was confirmed as Bloodtest; independent client-mode evidence was missing. This is not the Juggernaut 67-NPC acceptance scenario, and the journal tail contained no bounded callsite CPU window.

Before using this sampler for precise comparisons, coordinate ownership and measure one narrow observer-cost change: keep host and known-PID CPU deltas frequent, but move full process inventory and runtime-journal discovery out of synchronous build startup. Compare setup wall/CPU, sample cadence, process coverage, and build wall time under matched warm inputs. No sampler code was changed in this pass. A separate root build was active on run `20261124_174426` at the last status check; no profiling command was issued during it.

## Evidence

- `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.jsonl`
- `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.resource.jsonl`
- `.dev/build-timings/game-dll-20261124T171210403561Z-968a7e.resource.json`
- `.dev/build-timings/game-dll-20261124T173829158368Z-8723b2.jsonl`
- `.dev/build-timings/game-dll-20261124T173829158368Z-8723b2.resource.json`
- `logs/2026-11-24/20261124_170535/server-01/events.jsonl`
- `logs/2026-11-24/20261124_170535/client-01/events.jsonl`
- CPU issue `issue-ed3ac775283d`; DLL latency issue `issue-136d4553ff92`
