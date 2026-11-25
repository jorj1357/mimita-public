# Secret-safe authorization for the bounded CPU probe

## Evidence and proposed route

The existing run-scoped command transport persists command strings in the queue and `dev-loop.command.request` event (`devscripts/dev_loop/parallel_test_runs.py:392-400,1024-1028`). The previous admin-based probe route would therefore persist a password. No credential or profiler command was sent.

Source inspection shows `src/network/server-packet-chat.cpp` already has `devLoopbackSuperadminEnabled(sock)` at lines 54-70, checking server role, nonempty run metadata, loopback binding, and the enabled `dev_loop_localhost` config. The existing `npc_cpu_probe` parser is bounded to 1-30 seconds, but currently runs after `serverAdminAccessEnabled(player)` at line 860. A narrow measurement-enablement change could allow only `npc_cpu_probe` commands through when either the normal admin grant is present or this existing dev-loop check passes. All other server commands would retain the current admin gate; no password or admin-session grant would be needed.

This is a proposal, not an implementation. Before editing, confirm whether the peer endpoint also needs an explicit loopback check, and coordinate ownership of `server-packet-chat.cpp`. The CPU profiler owner has not replied; their activity remains waiting for an authorized target run. Root dev-loop reports no active build, but available RAM was only 671 MB at 2026-11-25T04:37Z, below the 2 GiB build gate. No C++ edit/build is safe yet.

## Experiment sequence

After ownership and memory gates clear:

1. Implement and build only the bounded probe authorization path; prove non-dev and non-loopback requests remain denied and the probe auto-stops within its configured bound.
2. On an owner-approved same-generation workload, exercise the probe through the normal run-scoped command queue without any password. Capture its acknowledgment and CPU-callsite rows.
3. Select one CPU treatment from the attributed profile, then use a separate matched before/after runtime pass for CPU, memory, client every-frame timing, 60 Hz server ticks, and gameplay checks.

The current Juggernaut run remains host DLL generation 635 / NPC module generation 632 with two other Bloodtest runs in the process inventory; this is not yet an NPC optimization baseline. The run-owner question and authorization proposal are recorded in `issue-ed3ac775283d`; the latency issue is cross-referenced in `issue-136d4553ff92`.

## Session outcome

No source, configuration, runtime, or build state was changed. The CPU optimization goal remains active; this is a measurement-access design step, not a resource reduction result.