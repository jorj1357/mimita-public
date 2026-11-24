# CPU profile evidence and sampler handoff

- Session ID: work-5dd66f34bec1
- Owner: resource-efficiency-goal
- Focus: Continue matched runtime CPU attribution and coordinate with the build-latency owner
- Status: partial; no MiMITA source change or optimization made
- Date: 2026-11-23

## Current runtime and sampler state

Root dev-loop status was `lifecycle=stopped`, with generation571 published and root server PID10164 alive on Bloodtest, zero NPCs and zero players; no root client was registered. The separate isolated Juggernaut pair PIDs22184/32832 remained alive. A prior 15-second sample for that pair found combined CPU at 51.02% of whole-host capacity, but its nearest matching journal rows were after the sample, so its exact in-window workload and generation remain unknown. This agent did not stop or restart that pair.

The build-latency owner reported a corrected Windows native process-metrics fallback smoke: six samples over 2.6 seconds, client about30.2% and server19.8% of whole-host CPU, with4.68GiB RAM available. The result establishes that the native fallback can observe process CPU in a no-build smoke window. Exact UTC interval, artifact path, PIDs, and journal-aligned mode/workload were not present in the handoff, so it is not treated as the matched CPU profile or as function attribution. The latency owner is checking the safe reload path so future root builds import the sampler change.

## Function attribution limits and next evidence

The existing bounded probe samples 1/64 thread-cycle scopes and reports estimates. The matched gen570 profile ranked `updateOneNpc` above collision dispatch, but broad/overlapping scopes do not identify exact source-line CPU shares. Nearby elapsed phase counters also overlap and are not CPU-time partitions. The next useful measurement is a journal-aligned host/process sample plus non-overlapping, opt-in NPC thread-cycle buckets (or valid CPU.light call stacks), with sample counts and confidence limits. Preserve the current runtime pair and wait for the latency owner to confirm reload completion and release the sample window before collection.

No game source, sampler source, build, or runtime was changed by this session. The CPU evidence and coordination request were appended to issue `issue-ed3ac775283d` and linked to the latency owner; the sampler owner should record the smoke artifact details in the next handoff. No optimization is justified until function-level evidence is valid and repeatable.
