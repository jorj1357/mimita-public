# Runtime process and CPU evidence follow-up

## Changes

- Appended current process, CPU, memory, client frame-slice, server NPC-budget, and stale-run-manifest observations to plan issue `issue-dc2f873290a7`.
- Sent the measured sample and journal paths to the owner of CPU profiling issue `issue-ed3ac775283d` for correlation with their WPR work.

## Findings

- Existing multi-instance code defines one instance as one executable process. It already supports one server plus multiple client processes; isolated test runs can add `client-02`. One process hosting several independent clients would require separate client-global ownership and is not currently supported.
- In five seconds, current run `20261122_214309` (Juggernaut Horde, dust2cyberiav4, EXE build `2026-11-22T21:32:48.195707Z`, DLL generation 526) had client PID 26428 at 242.5% of one logical core and server PID 33984 at 153.6%; together about four logical-core equivalents. RSS was about 456/335 MiB. Host readings were 67.8-83.0% across 12 logical CPUs and memory was 83% used with 2.65 GiB available. This is a short, mixed-workload sample, not a whole-machine attribution.
- The client journal recorded 7-9 frames per performance slice, 24.3-33.2 ms average and all frames above the 4.167 ms target. `Network::ReceivePoll` and `EngineTick::Simulate` were the largest measured scopes; timing scopes do not equal thread CPU attribution.
- The server reported 67 NPCs and NPC phase maxima of 37.7-39.9 ms against a 10 ms share, with 37-42 over-budget ticks per roughly 1.018-second window. This is direct evidence of server NPC budget pressure for this workload.
- Two old test manifests still showed `online` while their recorded PIDs had exited. Their state must be reconciled before trusting process counts.
- No source cause is proven. Recent NPC/server changes and the 999 FPS cap setting are leads only; no matched profile links them causally. WPR was not started because profiling ownership was already active and the machine had limited free-memory headroom.

## Next step

- Wait for the existing CPU profiling task to obtain a safe, low-loss per-thread capture. Then implement the first measured improvement: queue independent scenarios through a capped set of warm pinned runtimes before attempting in-process client multiplexing.
