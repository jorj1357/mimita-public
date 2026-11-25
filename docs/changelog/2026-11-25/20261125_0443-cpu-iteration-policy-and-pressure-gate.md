# CPU treatment loop and current pressure gate

## User direction

The user requested that every CPU-focused build test a code treatment, compare matched before/after runtime resource use, and feed results into the next falsifiable experiment. This applies to the active CPU-reduction goal and does not make an unmeasured build count as an optimization.

## Coordination record

Added this experiment-loop requirement to `issue-ed3ac775283d` and asked `cpu-optimization-pass-loop` to coordinate its current file ownership, profiler gate, and eligible runtime. The required pass shape is one CPU treatment, a predicted effect, a matched baseline, a treatment build/artifact, same-workload post-activation sampling, frame/tick/gameplay checks, and a result with the next hypothesis. A telemetry-enabling-only change must be labeled as such; it is not a CPU reduction.

## Current read-only snapshot

At 2026-11-25T04:43:33.831Z, Windows reported 12 logical processors, 570.1 MiB available of 16,024.5 MiB RAM. Six `mimita` PIDs were alive: 6244 (31.5 MiB working set), 13336 (39.7), 25552 (120.5), 36452 (2,133.0), 36632 (122.7), and 38720 (113.6). Their working-set sum was 2,561.0 MiB; this can double-count shared pages and is not private-memory total. Instantaneous CPU rate and process roles/modes were not measured.

Dev-loop status reported no running game pair and no active build, while those six processes remained outside its registered runtime. Therefore there is no valid matched workload or activation proof for a code A/B. Free RAM is below the 2 GiB build gate. `src/network/server-packet-chat.cpp` still checks generic admin access before the bounded `npc_cpu_probe` handler, so the probe path is not yet verified usable without the credential-bearing `adminadmin` command route.

## Outcome

No game source, configuration, build, runtime, or profiler was changed or started. Next step is to reconcile the unregistered processes and establish one authorized, same-generation client/server workload with sufficient memory headroom; then run one CPU-focused treatment at a time. Until then, any change would lack a trustworthy before/after comparison and could worsen the already constrained machine.
