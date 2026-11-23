# CPU.light sample attribution and source-resolution limit

- Agent: `cpulight-function-map`
- Issue: `issue-ed3ac775283d`
- Capture: `.dev/perf-captures/mimita-cpu-light-10ms.etl`
- Run: `20261123_055730_01`; runtime build identity `2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7d9d`; game DLL generation 546.

## Capture and resource findings

WPR CPU.light sampled at a 10 ms period for 10.5714 seconds, UTC 2026-11-23 07:15:55.042–07:16:05.614. The host has 12 logical CPUs. xperf reports zero lost buffers/events. The ETL is 90,177,536 bytes. Normalizing sample weights by all 12 logical CPUs over the measured duration:

| Process/module | Whole-machine CPU | Approx. core equivalents | Share within MiMITA process |
|---|---:|---:|---:|
| Client PID 3600 (`mimita.exe`) | 19.94% | 2.39 | — |
| Client `game-g546.dll` | 4.88% | 0.59 | 24.5% |
| Server PID 8804 (`mimita.exe`) | 12.02% | 1.44 | — |
| Server `game-g546.dll` | 3.02% | 0.36 | 25.1% |
| Both MiMITA processes | 31.96% | 3.84 | 100% |
| Both processes, samples in `ntoskrnl.exe` | 12.78% | 1.53 | 39.9% of combined MiMITA CPU |

Top other sampled processes included `pythonw.exe` PID 16864 (5.99% host), `MsMpEng.exe` PID 6696 (3.48%), `pythonw.exe` PID 23824 (1.39%), `Taskmgr.exe` PID 23012 (0.87%), and WPR PIDs 3568/3192 (0.25%/0.15%). The process attribution says which process context was running at each sample; it does not prove which caller caused kernel or driver work. The later dev-loop status showed build idle and `dust2cyberiav4` / `jugger_horde` / one NPC, but exact build overlap and exact mode/NPC state during the trace were not established.

## Function and line attribution

This analysis did not recover exact MiMITA functions or source lines. `xperf -a profile -detail` grouped this binary by module, and symbol output names `game-g546.dll` as `Unknown` for both PIDs. The CPU.light profile did not collect System Stacks. The generated profile therefore cannot support claims such as “function X consumed Y%” or a file/line attribution.

Next check: obtain CPU Usage (Sampled) instruction-pointer rows from WPA for this ETL and symbolize them against the immutable generation-546 DLL's usable debug information. If CPU.light cannot export those rows, use a bounded stack-enabled capture with a symbolization route confirmed before capture. Tie mode, actor count, and build phase to the same UTC window, then report sampled leaf CPU milliseconds and whole-host/process shares separately. No hot code should be changed on module-only evidence.

## Artifacts and validation

- ETL: `.dev/perf-captures/mimita-cpu-light-10ms.etl`
- Reports: `mimita-cpu-light-10ms-stats.txt`, `mimita-cpu-light-10ms-frequency.txt`, `mimita-cpu-light-10ms-profile.txt`, `mimita-cpu-light-10ms-profile-symbols.txt`, `mimita-cpu-light-10ms-detail.txt`
- Journals: `logs/2026-11-23/20261123_055730_01/client-01/events.jsonl`, `logs/2026-11-23/20261123_055730_01/server-01/events.jsonl`
- No source edits, builds, tests, or process restarts were made. The active MiMITA client/server were left running.
