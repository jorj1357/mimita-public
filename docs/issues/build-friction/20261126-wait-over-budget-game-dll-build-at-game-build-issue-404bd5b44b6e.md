# issue-404bd5b44b6e: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-build-not-stalled)

### 2026-11-26T17:20:43.775Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed pinned wait game-dll-build:2026-11-26T16:49:17.979Z and its build attempt from run 20261126_164443.

VERDICT: confirmed real over-budget build, not a stalled build and not a wait-policy bug. The build completed successfully. No code fix started or claimed.

PINNED IDENTITY
- wait_id game-dll-build:2026-11-26T16:49:17.979Z; stage game_build; budget 60.0s; elapsed 60.023s; over-budget recorded 2026-11-26T16:50:44.855Z (dev-loop.wait.over_budget, issue created once).
- build_id 2026-11-26T16:48:56.265247Z; request 2026-11-26T16:49:17.806Z-36f0bfdb:mode3:game; source_generation 7.
- build attempt game-dll-20261126T164918058647Z-26c738; generation 696; revision 4d9ba81e...15c03; code_hash dfbd8b94...e3c35e3ef.
- Journal: C:\mimita-v9\logs\2026-11-26\20261126_164443\dev-loop\events.jsonl
- Timing: .dev\build-timings\game-dll-20261126T164918058647Z-26c738.json and .resource.json

WHAT HAPPENED (direct observation)
- started 16:49:18.058Z, completed 16:51:39.101Z; worker_total 136.859s, total 139.968s.
- compiled 63 / skipped 582 of 644 sources; jobs 10; object_scan 1.485s; compile 131.703s; link 2.329s; publish 0.407s.
- compile_sum_ms 1,219,786 over 63 TUs (~9.26x effective parallelism of the 10-job pool); compile_max_ms 59,295 (single TU took 59.3s).

FIRST CAUSE OF THE LATE TERMINAL
The wait is genuine compute time. The Mode 3 sealed batch changed shared header src/network/server.h plus src/network/vehicle-networking.cpp, src/duel/duel-queue.cpp, src/vehicles/vehicle-system.cpp (and cold devscripts/dev-launch-modes.json), forcing 63 translation units to recompile. On 10 jobs with per-file compile 9-59s, the 60s budget elapsed while TUs were still queued (later TUs show queue_wait_ms up to ~99s); the build reached a terminal result ~79s after the budget and then published generation 696.

RESOURCE PRESSURE (direct observation; window 2026-11-26T16:49:18-16:51:39Z, 107 one-second samples)
- Host 12 logical / 8 physical CPUs, ~15.65 GiB RAM.
- build-attempt mean_host_cpu 93.245%, peak 100%; minimum_available_memory_bytes 5,046,272 (~4.8 MiB).
- Sampler process_discovery_wall_ms_total 47,105ms (sampler stalled ~47s under load).
- Concurrent live game from the same run: server PID 32992 (MiMITA server, mode vehicle_freeplay, players=1, npcs=0, ~62 ticks / 12.9ms window), client PID 26700 (MiMITA client, FPS 444, p95 3.06ms, max 6.05ms). The DLL build shared the host with a running server+client pair.
- Inference (not direct): near-memory-exhaustion + 10 compile workers + a live game pair is the dominant reason the compile wall stretched to 131.7s. Unknown: the exact per-PID CPU split of the compile jobs (the resource summary reports host aggregate, not per-worker CPU).

RECURRENCE (same root-cause class; do not create more duplicates)
Within this run the identical "Wait over budget: game-dll-build at game_build" trigger fired again: issue-4fb7a24c54b0 (wait 16:56:24.029, gen 698), issue-45733b82bbbc (wait 17:07:53.638, gen 702; compile 79.343s, total 101.39s), issue-24863e1f595e (wait 17:13:10.456, gen 703; only 5 TUs but compile 92.001s, total 131.781s). 130+ identical build-friction issues already exist. The issue's stated risk ("a noisy wait policy could create duplicate friction issues") is already realized. Related umbrella owners: issue-e044a931c54b (waiting is the enemy; resource-arbitration proposal by build-wait-reviewer 2026-11-20), issue-7d3dcfd7433b (measure every dev-loop wait stage), issue-84c584957e65 (late reviewer-wake dispatch).

REVIEWER WAKE (acceptance item)
Exactly one wake was queued for this wait occurrence (wake-61c635eea286; .dev/agent-wake/events.jsonl): queued 16:50:20.774Z on codex; codex claimed/launched 16:50:44.717/44.826Z then exited; wake.retry to opencode 16:57:38.602Z; opencode claimed/launched 17:09:06.520/06.723Z, ~18m46s after queue. "Queued exactly once" is met; the late fallback reproduces issue-84c584957e65.

ACCEPTANCE CHECK
- wait_id / stage / elapsed / build_id / request_id / source_generation present: yes.
- reviewer wake queued exactly once: yes.
- Evidence-quality gaps (non-blocking): evidence.reason is a truncated fragment of the last per-file progress record (not a meaningful reason), evidence.generation is null although the build produced generation 696, and run_id 20261126_164443 is absent. Suggested (build/dev-loop tooling owner): record run_id and the real generation on the wait, and replace the truncated progress tail with a one-line phase summary.

NEXT STEP / OWNER
Owner unowned; claim via next --claim if taking it. Smallest useful next check: one controlled repro of the same 63-TU change with the game pair stopped vs running, recording before/after compile wall against the 60s budget, then feed the result to issue-e044a931c54b. Unknowns: per-PID compile CPU share; whether jobs>10 under ~4.8 MiB free helps or hurts.
