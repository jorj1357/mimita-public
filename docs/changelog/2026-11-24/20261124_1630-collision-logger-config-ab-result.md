# Collision logging CPU A/B result

## Change and method

- Ran a reversible configuration-only A/B on the existing processes; no source, executable, build, or process priority was changed.
- A kept `collision.capsule-world.solve` at `important`; B changed only that category to `off`. NPC movement and performance categories remained enabled.
- Interleaved three 12-second windows per condition: A1/B1/A2/B2/A3/B3, sampled at one-second cadence on a 12-logical-CPU host.
- Restored `config/debuglogger.json` byte-for-byte; its SHA-256 is `2161a60e71309ce0b0e34c58b478e87a7e62f8ef0b557d5e2e2838a4ef0e5152`.

## Findings

- The filter change took effect on root server PID25840: 12 matching collision events per A window and zero per B window.
- Mean whole-host CPU was 13.24% vs 13.08% for PID25840; 18.16% vs 18.07% for client PID23360; 7.85% vs 7.52% for child server PID33092. All three MiMITA processes together were 39.24% vs 38.67%; host CPU remained about 99% in both conditions.
- These small differences do not establish an improvement. Root-server tick averages were about 2.39ms in A and 3.04ms in B; each condition had a >100ms stall, with maxima 118ms and 728ms respectively. Disabling this log category did not improve the observed tick tail.
- The process roster missed PID33092. PID23360 spawned that MiMITA server for Bloodtest/badhouse/1 NPC; root server PID25840 was a separate Bloodtest/slaughterhouse1/2-NPC server. PID33092's journal was capped and stale before the A/B, so its workload and client frame results are unknown. This was not a fully journal-matched pair test.
- The host was saturated. Other observed high processes included pythonw, cpptools, and OBS. Available memory varied widely, so this run supports no memory conclusion.
- Existing 1/64 callsite estimates from a separate low-NPC Bloodtest sample put `updateOneNpc` and `doCollisions` at small shares; they do not rank costs for the 67-NPC target workload.

## Interpretation and next experiments

- Direct observation: disabling this collision log removed its events but did not produce a consistent CPU or tick improvement.
- Hypothesis still open: the large elapsed-time gap between collision/sweep timing and inner solver timing may be blocking, lock contention, or descheduling rather than collision CPU. The A/B alone cannot distinguish these.
- Next falsifiable passes: (1) compare thread CPU time with wall time around sweep/slide and the log write; (2) separately time in-process mutex, named mutex, write, and flush waits, then test a bounded nonblocking optional-event path; (3) repair process-to-run journal attribution and measure all MiMITA PIDs with mode, NPC/player count, module generation, frame, and tick evidence on one connected target run.
- Do not count this as a runtime optimization. Any candidate code change needs a matched before/after run and must preserve 60 Hz server behavior and the client frame-time target.

## Artifacts

- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-A1.csv`
- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-B1.csv`
- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-A2.csv`
- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-B2.csv`
- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-A3.csv`
- `.dev/perf-captures/cpu-collision-log-ab-20261124T161846Z-B3.csv`
- Root server journal: `logs/2026-11-24/20261124_121332/server-01/events.jsonl`.
- Client journal: `logs/2026-11-24/20261124_150432/client-01/events.jsonl` (capped/stale before this sample).
- Full issue discussion: `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`.
