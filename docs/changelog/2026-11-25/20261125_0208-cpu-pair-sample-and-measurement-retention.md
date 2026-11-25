# CPU profiling pass: matched pair sample and retention gap

## Result

This was a measurement pass, not a runtime optimization. No MiMITA source was changed and no build was requested by this task. The active CPU goal remains: choose one measured CPU change per CPU-goal build, compare the same pinned workload before and after, and reject any change that misses 240 FPS or 60 Hz/gameplay behavior.

## Direct measurements

Target run: `20261125_014732`, server PID `36452`, client PID `25552`, EXE build ID `2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b`. `run.json` says `parallel_agent_test` but has null agent/task/issue and null DLL identity. Do not issue commands to this run or call it a fully owned/pinned pair. Client journal did confirm `network.mode.active=jugger_horde` at 01:54:33Z; server's recent NPC phase report showed generation 632 and 67 NPC, but not in the same CPU sample window.

- PID-only sample `cpu-live-juggernaut-pidonly-20261125T0153Z.csv` was captured for 01:54:27.607–01:54:42.364Z: 12 logical CPUs, host CPU mean 84.86%, client 20.64% whole-host (247.6% of one logical CPU), server 12.54% host (150.4% of one logical CPU), pair 33.17% host. RAM used mean 91.97%, available mean 1.35 GB. Sampler averaged 0.026% host CPU. Client journal had 70 frame slices, 942/942 frames over 4.167 ms, and mean frame-slice average 17.08 ms. No server tick rows or CPU-callsite rows aligned to this sample.
- PID-only sample `cpu-live-juggernaut-pidonly-20261125T0202Z.csv` was captured for 02:04:09.926–02:04:24.803Z: host CPU mean 97.70%, client 19.50% host, server 11.37% host, pair 30.88% host. RAM used mean 92.87%, available mean 1.20 GB. Sampler averaged 0.026% host CPU. In the matching client journal interval, 507/507 frames exceeded 4.167 ms; frame-slice averages were 29.54 ms and slice p95 averaged 39.34 ms (maximum slice p95 76.19 ms). Server tick telemetry and CPU-callsite rows were absent from that exact interval.
- Client wall-time scopes in the second interval: `EngineTick::Simulate` averaged 14.14 ms/call; `Setup::ConfigPolling` 0.93 ms/call; `Audio::MusicConfigPoll` appeared once at 1.46 ms. These are elapsed scope times, not CPU attribution, and nested scopes must not be added. Relevant sites include `src/engine/engine-tick.cpp:111,221` and `src/network/multiplayer-tick.cpp:1328`. The one MusicConfigPoll observation is not evidence that audio explains the pair's CPU share.
- A separate all-process sampler was too intrusive: 8 rows across 12.3 seconds, process enumeration 1.47–1.93 seconds/row, sampler CPU estimated 6.39% of host. Exclude that sample from normal-load claims; use the PID-only method or improve its retention/collection path.
- At 02:06:35Z, this run had 0.665 GiB of event journals across 10 files (server 0.466 GiB, client 0.199 GiB). This is direct log volume, not proof that logging caused CPU or disk slowdown. C: free space changed between nearby reads (1.82 GiB and later 2.36 GiB); recheck immediately before any build and do not attribute that change to a specific cleanup.

## Attribution and limits

The direct result is that this live pair consumed about 31–33% of whole-machine CPU while the host was 85–98% busy, and client frame-time acceptance failed badly. This supports the user's report that MiMITA can materially contribute to PC load, but it is not a causal close-process A/B and the top competing process list was not available in the low-overhead samples.

No `performance.cpu-callsite-window` event was produced. `game.hotness.summary.probe=true` means the host transport probe is enabled; it does not prove the bounded CPU callsite probe ran. Scope timing, event counts, and NPC phase elapsed totals do not identify exact function CPU shares. No runtime CPU code change has yet been proven to lower usage.

The two PID-only CSVs were created under `.dev/perf-captures`; on recheck that directory and both files were absent. Cause is unknown. Aggregate values were appended to `issue-ed3ac775283d`, but raw per-sample retention failed. Future captures should be retained with the exact run's evidence and verified before citing the A/B.

## Next pass

1. Let the unrelated active root build finish; do not mix its five network behavior inputs into the CPU experiment. Attempt `game-dll-20261125T021123740140Z-5e8bdb`, generation 633, carries note `e99604034789` for client body-part holes/projectile damage logging, not a CPU optimization.
2. Obtain an owned run with explicit agent/task, exact EXE/DLL identity, connected client, authoritative active mode, and generation-aligned NPC module. Run the existing bounded `npc_cpu_probe` on client/server with a low-overhead PID-only host sample; retain the raw artifact.
3. Pick one source change only from the resulting CPU attribution. Queue its build note before editing, use root dev-loop, then repeat the same scenario and compare process/host CPU, frame distributions, server ticks, and behavior journals. If function attribution is missing, improve the probe before changing gameplay code.

## Follow-up correction — 02:14 UTC

Root dev-loop later reported the unrelated generation-633 request as failed at the disk gate, not active: 1,630 MB free after auto-prune versus the required 2,048 MB. Another owner status snapshot reported 1,548 MB; free space was changing, so these are separate point-in-time gate readings. The attempt had five network behavior inputs and note `e99604034789`, no CPU treatment, and no runtime A/B. Do not treat it as an active compile or CPU optimization result. The next CPU-goal build remains held until a callsite profile chooses one change and the build gate has sufficient disk headroom.
