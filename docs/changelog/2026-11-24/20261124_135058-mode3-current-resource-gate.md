# Mode 3 latency work: current resource gate

## Purpose

Record the current build/runtime state and why the next heavy latency trial is not safe to start yet.

## This entry does not

- Claim that host load caused the earlier slow builds.
- Prove the <=10-second multi-file Juggernaut target.
- Change source, build policy, or the active game processes.

## Observed at 2026-11-24T13:49Z

- Root dev-loop status reported no active build and no pending batch. The latest published game DLL was generation 594; the EXE build ID was `2026-11-24T07:48:35.684015Z:52c3ec0ab05736cff06e7105a6966f96920790dde025b8d21ecdb49e24a0b122`.
- The shared runtime was Bloodtest on slaughterhouse1, with server PID 25840 and client PID 34756. This is not the target Juggernaut Horde workload.
- One host counter read reported 100% CPU and the OS memory query reported 0.48 GiB free. This is a single snapshot, not a duration sample; it provides no process attribution and does not establish build-time causality.
- The latest prior bounded same-room sample is recorded in the CPU issue. It showed a Bloodtest client/server pair with different run IDs and host DLL generation 594 versus NPC module generation 593. That is useful pressure context, not target acceptance evidence.

## Decision and next step

No new client/server process, build, or runtime command was started. The current machine has insufficient memory headroom for a 67-NPC Juggernaut trial, and the DLL-boundary owner has been asked for the ModuleHost generation-refresh status. Once the host has recovered and that owner confirms a safe window, run the next measured target trial through the root dev-loop, keeping the 15–20-file edit set, process/resource sample, DLL activation, and runtime behavior proof on one matched run.

## Evidence references

- Root status command: `python devscripts/dev-loop.py --status --json --no-coord`.
- Current latency issue: `issue-136d4553ff92`.
- CPU/resource issue and prior bounded sample: `issue-ed3ac775283d`.
- Prior same-room sample artifact: `.dev/perf-captures/mode3-bloodtest-host-pid-baseline-20261124T133121Z.json`.
- This observation was appended to the latency issue discussion with the active ModuleHost owner.

## Follow-up at 2026-11-24T13:57Z

- A second root-status read still showed no active build or pending batch, generation 594, and the same Bloodtest server/client PIDs 25840/34756. A one-shot host counter read was 94.1% CPU; available RAM was 0.55 GiB. This is another point snapshot without process attribution, not a duration sample or build-causality result.
- A broad `agent-coord sync`/board/status invocation remained active for more than 100 seconds and was terminated. A follow-up process inventory found no remaining `agent-coord.py` process. Treat that tooling work as a possible host-load confounder; it is not evidence that MiMITA alone used the observed CPU.
- The DLL-boundary owner had not replied by the latest inbox check. The resource-profiler was told to keep its separate CPU A/B on hold until adequate headroom and a coordinated window are available. The pair remains preserved; no build, restart, probe, or new game process was started.

## Follow-up at 2026-11-24T14:04Z

- A fresh root status/counter read beginning at 14:04:54.851Z showed no active build or pending batch; game generation 594; Bloodtest/slaughterhouse1; server PID 25840 and client PID 34756. One host CPU counter read was 68.7%; available RAM was 2.80 GiB. This is a point snapshot without process attribution, not the target Juggernaut workload.
- The coordination brain surfaced the stable gap `build-resource-sampling:game-dll-20261124T122208532874Z-63061a`. Its sidecar records 92 samples over 99.423 seconds, mean host CPU 99.853%, peak 100%, minimum available memory 312,864,768 bytes, and no sampler errors. It could not match the live server/client journals and lacks a matched prebuild baseline, host commit charge/limit, and journal-aligned mode/workload/frame/tick metrics.
- I appended the gap and its next evidence step to the latency issue and sent it to the resource-profiler owner. It is missing measurement coverage, not proof that game load caused the long build. The target run remains held below the CPU agent's proposed ~6 GiB headroom; no build, runtime control, or new game process was started.

## Sampler alignment follow-up at 2026-11-24T14:21Z

- The resource-profiler owner replayed attempt 63061a after its code change: it now identifies same-run server PID 25840/client PID 12848, while reporting server mode `bloodtest` and client mode unknown. Their 14/14 focused sampler tests and `py_compile` passed. This is tooling evidence; the root daemon has not loaded the change yet.
- Raw client journal evidence explains the unknown: its `network.mode.start` row is timestamped 12:13:59.745Z, more than eight minutes before the build sidecar began at 12:22:08.533Z. The sampler's freshness window therefore correctly treats client mode as stale. The server continues to provide fresh authoritative mode and workload/tick events.
- The owner is extending the output to separate same-run process/frame alignment from mode proof: expose mode from the fresh server journal and keep client-mode equality explicitly unverified. I am holding daemon restart until they send READY because their sampler and test-file leases remain active.
- At 14:21:39.831Z, the current no-build root status still showed Bloodtest/slaughterhouse1, generation 594, server PID 25840 and client PID 34756. A single host snapshot was 87.6% CPU and 2.21 GiB free RAM. This remains below the proposed headroom for a 67-NPC target run; it is not process-attributed evidence. No build or runtime control was started.

## Build stage and daemon reload follow-up at 2026-11-24T14:31Z

- Read-only attempt timeline for `game-dll-20261124T122208532874Z-63061a` shows 99.428s total for one changed source (`src/network/server-projectiles.cpp`), 2 translation units compiled and 643 skipped. The UI's 60.600s `changed_source_preflight` phase contains the changed source's 60.521s compile; the nominal `compile` phase was only 0.864s for `module-identity-provider.cpp`. LLD was 22.383s over 644 objects / 492,383,306 bytes; object scan was 0.055s. This clarifies that cache scanning is not the dominant cost in this attempt; the changed TU and full DLL link are. The issue now records that the phase label obscures the actual compile.
- After the sampler owner released its leases and reported 15/15 focused tests plus `py_compile`, I performed the documented daemon stop/ensure reload. New daemon PID 19372 replaced PID 36728; server PID 25840 stayed alive. The former client PID 34756 was absent after reload, and root `--list-instances` contained only the server. Root status is `published_stale`, `builds_match=false`, Bloodtest/slaughterhouse1, 1 NPC, no client. Source inspection confirms the shutdown finalizer at `dev-loop.py:6123-6125` terminates daemon-owned clients while leaving the independent server running; this path caused the old client to disappear. The restart procedure's preservation expectation is unmet for clients. No replacement client, build, or new runtime has been started.
- Available RAM was 2.39 GiB at 14:31:06.390Z, below the proposed ~6 GiB headroom for the 67-NPC pressure trial. The Juggernaut edit-to-live trial remains held until the client/run identity and host/NPC generation are repaired and resource headroom improves.
- A read-only source trace confirmed `dev-loop.py:6123-6125` calls `self.stop_processes()` during daemon shutdown, terminating daemon-owned clients while leaving the independent server alive; the stop/ensure reload therefore explains why client PID 34756 disappeared. At 14:44:59.269Z, the root remained server-only (PID 25840, Bloodtest/slaughterhouse1, 1 NPC) with 1.871 GiB of 16.025 GiB visible RAM free; CPU was not measured in this point read. No client reattach or build was started.
