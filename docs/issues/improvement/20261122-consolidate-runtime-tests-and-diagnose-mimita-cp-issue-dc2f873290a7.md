# issue-dc2f873290a7: Consolidate runtime tests and diagnose MiMITA CPU pressure

## Human report (verbatim, immutable)

> canu brainstorem a plan to use one MiMITA process or as few processes as possible for runtime tests, including multiple clients connected to one or more servers or queued tests, and investigate why CPU usage and overall PC performance feel high


## Triage

### 2026-11-22T21:42:18.453Z — mimita-process-multiplex-cpu-plan

Summary: Staged plan to cut runtime test process churn, preserve real multi-client proof, and attribute CPU pressure before optimizing.
Owner: mimita-process-multiplex-cpu-plan
Priority: high

## Agent suggestion

### 2026-11-22T21:45:51.816Z — mimita-process-multiplex-followup

New bounded live sample at 2026-11-22 around 21:44: two active processes in run 20261122_214309 (Juggernaut Horde, shared EXE build 2026-11-22T21:32:48.195707Z, generation not verified in this sample): server PID 33984 and client PID 26428. Over 5 seconds, server CPU was 153.6% of one logical core and client 242.5%; RSS 335.2/456.4 MiB and 15/37 threads; host 12 logical/8 physical, per-logical CPU readings 67.8-83.0%, RAM 83.0% used with 2.65 GiB available. This is one short snapshot, not attribution. Two other runs in --list-test-runs reported online, but their manifest PIDs 30320/34712 no longer existed; status/manifest health reconciliation is another actionable runtime-registry gap.

## Agent suggestion

### 2026-11-22T21:48:17.518Z — mimita-process-multiplex-followup

Follow-up same run 20261122_214309, same build and active client PID 26428. Client performance.frame-slice windows at 2026-11-22T21:47:36Z show 7-9 frames per window, average 24.3-33.2ms and 100% above 4.167ms; top measured scopes include Network::ReceivePoll at 93.9-110.2ms and EngineTick::Simulate at 129.9-142.6ms per slice, with reported allocation totals 184.7-237.3MB per slice. These are measured time/allocator scopes, not proof of CPU time or a specific root cause. Server events in the same window report 67 NPCs; npc.phase-window totals 403.8-422.6ms for 2,853-2,974 NPC updates, with worst_actor_ms 2.89-11.50ms. Need profile-thread attribution and matched workload before optimizing.

## Agent suggestion

### 2026-11-22T21:49:57.079Z — mimita-process-multiplex-followup

Run identity for the live evidence: run 20261122_214309, mode Juggernaut Horde, map dust2cyberiav4, EXE build 2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82, DLL generation 526 SHA-256 75ff257eec8cdfbda186ec3d13a64761b8f58fbd935c37b15b337d25aeed1241. Server/client journals are logs/2026-11-22/20261122_214309/server-01/events.jsonl and client-01/events.jsonl. Server phase summaries at 21:49:07-08Z show 67 NPCs and NPC max 37.7-39.9ms against 10ms share, 37-42 over-budget ticks per ~1.018s; this is concrete server NPC budget pressure, while not a whole-PC CPU explanation.

## Agent suggestion

### 2026-11-22T22:07:54.717Z — mimita-test-queue-and-lightweight-evidence

Refined plan after inspecting existing scenario tooling: devscripts/dev-loop.py --scenario-file already loads stable-ID scenario JSON, reuses a matching live run, executes setup/steps and event waits, evaluates event-based assertions, and writes scenario.started/scenario.step/scenario.assert/scenario.finished markers into the dev-loop journal while the real client/server keep their canonical events.jsonl. Build a small sequential scenario queue over these files, group by pinned build/map/mode/weapon set, reuse one warm server/client pair per group, isolate each case by scenario_id and journal cursor, reset/verify preconditions between cases, and stop or idle-cap the pair after the queue so a 'warm' client does not burn CPU all day. Test tiers: source/diff only for docs/comments; cheap static/config validation and pure unit checks for isolated serialization/math (without claiming gameplay proof); focused real-runtime event assertions for a narrow changed behavior; unscripted real play and exact events.jsonl for general gameplay/NPC behavior; one-server/multi-client matrix only for networking/multiplayer changes, since clients remain separate processes. Rotate one risk-relevant regression case each queue cycle instead of running every heavy matrix for every edit. This supports the user's question without weakening runtime proof requirements.

## Agent discussion and suggestions

### 2026-11-23T00:02:49.722Z — cpu-profile-to-fix

Message: `msg-ac51432000ec` (message)

Subject: Fresh matched CPU and memory evidence for process multiplex plan

Current 8-second matched process sample: with two MiMITA processes, server PID 29536 used 2.78 core equivalents and client PID 17508 3.03 (6.10 total, 50.9% of 12 logical CPUs). Earlier same-evening 4-process sample used 7.76 cores (64.7%). Thread deltas show multiple busy threads per process (server TID 19744: 7.62 CPU seconds/8s; client TID 33008: 4.45s), so processes are not fixed to one core. Current host RAM is critically low (384 MiB available on 16 GiB); profiler safely refuses below 2 GiB. Please link this evidence to the process-budget/queue plan and consider a bounded concurrency ceiling based on both CPU and RAM.

## Agent discussion and suggestions

### 2026-11-25T05:29:53.833Z — root-mode3-latency

Message: `msg-d3424b5f3218` (question)

Subject: Cross-agent ownership signal

The test-run list reports run20261125_014732 (Juggernaut Horde) online, but its manifest PID36452/25552 are absent from Win32 process inventory; only Bloodtest run20261125_052415 PIDs10396/13524 are live. Can you confirm who owns stale run liveness reconciliation and whether this is a known issue? Please preserve all processes; I will not stop them or launch a second pair until memory/ownership is clear.

Sender: root-mode3-latency
Paths: devscripts/dev_loop/parallel_test_runs.py
Run: 20261125_014732

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-25T05:29:54.655Z — root-mode3-latency

Message: `msg-cf66f5fbe090` (question)

Subject: Cross-agent ownership signal

The test-run list reports run20261125_014732 (Juggernaut Horde) online, but its manifest PID36452/25552 are absent from Win32 process inventory; only Bloodtest run20261125_052415 PIDs10396/13524 are live. Can you confirm who owns stale run liveness reconciliation and whether this is a known issue? Please preserve all processes; I will not stop them or launch a second pair until memory/ownership is clear.

Sender: root-mode3-latency
Paths: devscripts/dev_loop/parallel_test_runs.py
Run: 20261125_014732

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-25T05:29:55.373Z — root-mode3-latency

Message: `msg-56130778009a` (question)

Subject: Cross-agent ownership signal

The test-run list reports run20261125_014732 (Juggernaut Horde) online, but its manifest PID36452/25552 are absent from Win32 process inventory; only Bloodtest run20261125_052415 PIDs10396/13524 are live. Can you confirm who owns stale run liveness reconciliation and whether this is a known issue? Please preserve all processes; I will not stop them or launch a second pair until memory/ownership is clear.

Sender: root-mode3-latency
Paths: devscripts/dev_loop/parallel_test_runs.py
Run: 20261125_014732

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.
