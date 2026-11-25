# MiMITA CPU resource goal: current pressure and renderer log lead

## Goal and method

The active goal remains reducing MiMITA CPU/resource use through repeated, evidence-selected runtime changes while preserving gameplay, client frame-time, and server tick requirements. The coordination brain already holds the user rule that each CPU-goal game-DLL build contains one CPU treatment, with matched before/after runtime proof and a keep/revise/revert decision. The user instruction is linked once as `ctx-91c6fa4bda74` to `issue-ed3ac775283d`.

The reusable scientific loop already exists in `docs/specs/dev-loop/agent-self-improvement.md:58-83` and `docs/specs/dev-loop/agent-coordination.md:220-253`: falsifiable hypotheses, one variable at a time, predeclared metrics, negative results, and up to three ranked next ideas. No spec change was needed in this session.

## Current runtime and resource evidence

- Root dev-loop reports no active build. No WPR or compiler process was found.
- Three current isolated runs map to six live MiMITA processes: Juggernaut run `20261125_014732` (server PID 36452, client PID 25552) and Bloodtest runs `20261125_020100` (13336/36632) and `20261125_022449` (6244/38720). A fourth older run `20261124_212828` was listed online but had no corresponding process in the six-process inventory; treat that registry entry as stale until independently reconciled.
- The latest rate sample remains the 15.09-second `.dev/perf-captures/cpu-orphan-six-win32-2026-11-25T0333Z.csv`: six MiMITA processes averaged 69.38% of whole-host CPU on 12 logical CPUs. Do not treat this as a fresh 04:xx CPU rate.
- Point-in-time available RAM snapshots during this follow-up varied from about 384 MB to 941 MB of 16,025 MB. No new per-process CPU rate was collected. Root build owner reports a 2 GiB headroom gate for build work; no build was requested.
- The target Juggernaut server journal remained at 67 NPC / 1 player. At 2026-11-25T04:15:10.902Z it recorded a 153.337 ms server tick, with 142.119 ms in NPC work; the phase-budget row deferred NPC work. The active host DLL is generation 635 while the NPC phase reports module generation 632, so this is not a same-generation NPC optimization baseline.

## New logging and storage lead

Run `20261125_014732` has 15 client JSONL segments totaling 1,424,573,053 bytes and 20 server segments totaling 1,940,745,309 bytes: 3,365,318,362 bytes combined. From launch at 2026-11-25T01:47:32.772Z to the latest inspected row at about 04:15:10.9Z, this averages roughly 380 KB/s across the pair. The `actor.replica.trace` event appears in the live client journal.

`src/engine/engine-tick-render.cpp:687-709` defines `traceReplica`; it emits `actor.replica.trace` at Important severity once per second per visible remote actor, constructs position/interpolation JSON, then calls `StructuredLogger::writeEvent`. Current `config/debuglogger.json` sets the rendering category to Important. Git history attributes this code to commit `d06fc464` at 2026-11-24T21:34:40Z, after the user's Nov 22 report of high CPU and low FPS. Therefore it cannot explain the onset, but it is a plausible ongoing logging, serialization, and disk-I/O contributor. No per-event CPU share or current event-rate sample proves a CPU reduction opportunity yet.

The owner signal was sent to `melee-parity`; no ownership reply had arrived by this changelog. The finding was appended to the existing CPU issue and cross-linked to log-retention issue `issue-3f776ce60511`. No source/config changes were made.

## Next discriminating step

After the run owners release extra workloads, memory recovers, and the renderer owner confirms the path, compare one target-only logging treatment against a matched baseline. Measure client process CPU per logical CPU and whole-host share, journal bytes and `actor.replica.trace` rate, every-frame client timing, and server ticks under the same 67-NPC mode. Keep the diagnostic accessible for explicit verbose capture. Do not build while memory is below the existing gate; do not claim the logging path caused the Nov 22 onset.

## Session outcome

No gameplay code changed, no build or runtime command was started, and no behavioral proof is claimed. Shared issue updates record the resource snapshot, run/generation caveats, and renderer logging hypothesis. This is an evidence-gathering pass, not a resource reduction result.