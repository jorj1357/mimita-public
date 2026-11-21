# issue-ed3ac775283d: Measure and reduce MiMITA CPU use during idle and normal play

## Human report (verbatim, immutable)

> how to make it so the mimita is like 1 percent or less cpu usage why is using so much?


## Agent suggestion

### 2026-11-21T23:11:37.631Z — cpp-latency-pressure-opt

Fresh bounded CPU sample on 2026-11-21 (10 s, psutil, 12 logical CPUs, concurrent game workload): system CPU 90.8%. Three mimita.exe processes used 91.6%, 239.8%, and 114.4% of one logical core (about 7.63%, 19.99%, and 9.53% of whole-machine capacity); together about 3.72 cores / 37.15% of machine capacity. The root dev-loop daemon used 52.5% of one core (4.38% machine); dashboard Python process used 55.6% of one core (4.64% machine). Process snapshot showed dev-loop client+server for Payload and an extra Bloodtest server, not Juggernaut; this is not the requested AI-controlled Juggernaut workload. The sample shows >90% aggregate CPU pressure, but it cannot assign the remaining load to a culprit. Next: attribute daemon/dashboard hot threads (source watcher currently rescans full directories after broad notifications; GUI refresh is another candidate), capture quiet/changed periods and notification counts, record per-game frame/AI-tick workload, then run one pinned Juggernaut test when no conflicting game-runtime work is active. Do not stop current processes.

## Agent discussion and suggestions

### 2026-11-21T23:19:59.232Z — cpp-latency-pressure-opt

Message: `msg-7fc0a9212771` (evidence)

Subject: Dev-loop watcher change: preserve your active test

I changed devscripts/dev-loop.py to consume exact Windows file-change paths rather than recursively snapshotting the full watched tree on each notification; the temporary-directory create/modify/close probe passed. The existing daemon needs a controlled restart before runtime/CPU verification. The process inventory shows a Bloodtest server PID 15340 launched under the active client, while your issue/task is active. I will preserve all game processes and avoid starting Juggernaut or restarting the shared daemon until I know whether this exact run is still being used. Please reply with the run/PID you own and whether restart with the process pair preserved is safe; no action on your process is requested.

## Agent suggestion

### 2026-11-21T23:26:43.425Z — cpp-latency-pressure-opt

Correction to my 2026-11-21 CPU evidence message: 91.6 + 239.8 + 114.4 = 445.8% of one logical CPU, i.e. about 4.46 logical-core equivalents, not 3.72. Normalized across 12 logical CPUs, the combined share remains 37.15% of machine capacity; raw per-process and system CPU values are unchanged.

## Agent suggestion

### 2026-11-21T23:31:15.984Z — cpp-latency-pressure-followup

8-second follow-up sample on the current non-Juggernaut Payload runtime: host aggregate CPU was 67.6%; dev-loop daemon used ~42.6% of one logical core (thread deltas ~32.2% and ~10.4%), dashboard ~7.6%, root game client and two servers remain busy. The process/thread CPU API cannot attribute Python stacks or host remainder. Need a controlled profiler run on the same runtime/load and compare daemon run-loop vs watcher call durations; do not treat this as Juggernaut evidence. This supports profiling the daemon and active game separately; <=1% whole-machine CPU is only a reasonable idle/background target, not during active rendering/simulation.

## Agent suggestion

### 2026-11-21T23:40:40.586Z — cpp-latency-pressure-profile

Correction to the previous CPU interpretation: py-spy identified TID 29684 as Thread-1 (watch) and TID 31296 as mimita-dev-control; the earlier 10.4% thread sample was the control thread, not the watcher. A 15s py-spy sample of the old daemon shows repeated state() / journal JSON decoding from the main run loop's print_status call (dev-loop.py:5516) and save_state path (:5519), plus duplicate instance-journal parsing in _write_manifest (:2000-2010). The watcher path was sampled rarely during this idle interval. Code now reuses a saved state snapshot and manifest instance rows; live impact awaits a controlled daemon restart with a preserved/relaunched pair.
