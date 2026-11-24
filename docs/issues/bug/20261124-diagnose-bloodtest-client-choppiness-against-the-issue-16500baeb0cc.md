# issue-16500baeb0cc: Diagnose Bloodtest client choppiness against the 240 FPS target

## Human report (verbatim, immutable)

> can u see why perorfmance is so chppy in the bloodtest mode ... need bloodttst perfoamnance to be 240fps and find why its choppy like maybe swap? idk ... i think its swap too that has like high ms consistently and config polling


## Review (needs-evidence)

### 2026-11-24T22:18:10.255Z — bloodtest-quality-triage

Reviewer: bloodtest-quality-triage

OBSERVED — direct Windows process/host sample at 2026-11-24 22:14:23.339Z–22:14:32.015Z (8.676 s), 12 logical CPUs. Four mimita.exe processes consumed about 77.4% whole-host CPU: PID26492 FCS client 481.5% of one logical CPU / 40.13% host; PID27372 FCS server 213.9% / 17.83% host; PID29052 Vehicle Arena client 191.6% / 15.97% host; PID37720 Vehicle Arena server 41.4% / 3.45% host. End-of-sample physical RAM was 212,552 KiB free of 16,409,100 KiB (~1.3% free). Their run IDs and ownership: PIDs26492/27372 are dev-loop run20261124_221014, Fun Counter Strike on dust2cyberiav4, one NPC configured; PIDs29052/37720 are isolated run20261124_221254_ball-proof-3, Vehicle Arena, agent vehicle-ball-proof, runtime ball-proof-3, generation626. Additional top host CPU: dev-loop dashboard PID16864 5.07% host, Task Manager PID30272 3.76%, dev-loop daemon PID19808 2.06%. The root dev-loop shortly afterward reported published generation626 and no active build. The FCS client journal logs/2026-11-24/20261124_221014/client-01/events.jsonl records recent 79.8–99.4 FPS windows, all frames above 4.167ms, p50 9.57–10.89ms, and one 100.64ms max. The vehicle test server journal begins logs/2026-11-24/20261124_221254_ball-proof-3/server-01/events.jsonl.

INFERENCE — MiMITA was a major CPU consumer during this measured window and physical RAM availability was critically low. Four processes were not all unowned: they form two active, identifiable test pairs; one is a headless server, so it has no normal taskbar window. The vehicle runtime's run.started event has agent=vehicle-ball-proof but issue="", so its proof/issue link is incomplete. This sample is not Bloodtest; the earlier Bloodtest run20261124_212828 exited before this process sample. It does not establish that swap caused the Bloodtest FPS loss.

UNKNOWN — pagefile/commit pressure, hard-fault rate, GPU and display utilization, CPU source-function attribution for this interval, and the human/agent justification or current need for each runtime. At 22:10 the dev-loop briefly showed launching/no process, while the run manifest later identified an online FCS pair; this is a status/manifest visibility discrepancy to reconcile, not grounds to stop either run.

NEXT DISCRIMINATING CHECK — Use one synchronized 60s sample while Bloodtest is active, with the resource collector joined by PID/run_id to client/server journals and process manifests. Add CPU per-thread/function or ETW attribution and pagefile/hard-fault counters; compare same workload with one test pair at a time. Explain every PID, role, owner, issue, generation, and stop/keep reason. Preserve active runs until their owners or runtime registry confirm they are no longer needed.
