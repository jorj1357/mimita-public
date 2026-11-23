# Live MiMITA CPU WPR capture and source attribution

- Confirmed `.dev/perf-captures/mimita-live-cpu.etl` is a valid 31.674-second WPR CPU capture with zero lost buffers/events.
- Matched the trace to run `20261123_055730_01`, generation 546, live Juggernaut Horde with one player and 67 NPCs; the GUI launch setting of one NPC did not reflect the actual runtime workload.
- Resolved sampled game-DLL function stacks through embedded DWARF. Client DLL-exclusive samples show audio resampling/mixing as the clearest lead; server samples are distributed across NPC and collision functions.
- Recorded process/module CPU, source locations, capture caveats, and next matched low-overhead A/B evidence steps in the CPU investigation issue.
- No source code, build, or runtime behavior was changed in this investigation.
