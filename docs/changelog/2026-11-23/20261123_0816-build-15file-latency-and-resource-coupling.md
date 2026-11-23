# Build-latency: 15-file Mode 3 measurement and resource coupling

- Agent: `resource-efficiency-goal`
- Issues: `issue-136d4553ff92` (build latency), `issue-ed3ac775283d` (CPU/RAM)
- Run: `20261123_055730_01`; live server PID 8804 / client PID 10148; runtime `jugger_horde`, 67 NPCs, generation 546 at sample time
- Playback: read-only. No source edit, no build request, no process restart.

## Findings

The user's exact scenario (`15-20 files edited at once, built, visible live while Juggernaut Horde runs`) is now measured. Attempt `game-dll-20261123T081116995354Z-6190f6` (generation 549) changed 15 files at once while the 67-NPC pair was live and took **58.58 s** end to end: pre-worker 10.272 s + worker 48.308 s. It compiled 16 TUs and skipped 627. Compile wall was 38.786 s from `compile_sum_ms` 346,953 over 16 TUs at `jobs=10`; the slowest single TU was 37.639 s (`src/gui/ui-system.cpp`) and five TUs were 25-38 s. Link was 6.652 s for 642 inputs / 483,418,670 bytes; object scan 1.534 s; `changed_source_preflight` 38.125 s is the changed-TU compile.

The current one-file baseline under the same live pair was 13.859 s (`6660f0`, gen 548) and 18.490 s (`d4d8aa`, gen 547). Pre-worker for `6660f0` decomposes to ~3.54 s before `worker_command_prepare` (packet_registry_codegen 814 ms, manifest_snapshot 733 ms, source_snapshot 433 ms, attempt_progress_and_context_write 193 ms, resident_state_codegen 178 ms, manifest_validation 141 ms, plus ~950 ms untimed inter-stage gaps), then `worker_launch_to_first_output` 731 ms.

Resource pressure is the multiplier: a matched 10 s sample at 08:03Z showed the pair at 61.6% of the 12-logical-CPU host (server 27.69%, client 33.91%), host CPU 90.7%. `Get-Process` at 08:06Z showed the server at 4804.1 MB working set / 6978.6 MB private commit; a concurrent `resource-efficiency-goal` change log records the same server at 5.39 GB working set / 7.67 GB private at 08:08Z, so steady-state server memory is large and may be growing (needs a proper time series; not yet a leak claim).

## Conclusion

15-20 files in 10 s is 5.9x away today and **cannot** be reached by scan/cache/scheduler tuning alone: a single 37.6 s TU is the critical path even with infinite jobs. Per-TU compile cost under live load, pre-worker (~10 s at 15 files), link (~6.7 s), and module-wrapper fan-out are the gates. The path is a large per-TU compile cut and/or a function-level reload (candidate modules, cr.h/blink, ORC), plus the CPU/RAM headroom from the CPU owner.

## Next steps

1. Re-run the SAME 15-file edit with the game pair stopped to measure the load multiplier (owner `mode3-live-dll-test`; I supply resource sampling). Do not start a build while that agent is active.
2. Attribute the 25-38 s per-TU compiles (headers/PCH scope vs load) and the module-wrapper fan-out.
3. CPU owner: time-series server working-set/private memory across one horde session; attribute growth before any leak claim.

No source files were changed, no build was requested, and the live game processes were left running.
