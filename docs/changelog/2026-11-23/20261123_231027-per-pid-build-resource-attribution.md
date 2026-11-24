# Per-PID build resource attribution

**Date:** 2026-11-23 23:10 UTC  
**Issues:** `issue-ed3ac775283d`, `issue-136d4553ff92`

## Finding

Generation 573's resource sidecar sampled host/process pressure, but its runtime context described root run `20261123_192224` (Bloodtest) while sampled PIDs 22184 and 32832 belonged to isolated run `20261123_195307` (Juggernaut Horde). The 22:11:54–22:12:08 UTC sample also overlapped generation 573's build preflight and must be treated as in-build evidence. The issue records have been corrected; do not use that sample as post-build evidence.

## Change

`devscripts/dev_loop/build_resource_sampler.py` now maps live MiMITA PIDs to their own dev-loop state or isolated `run.json` manifest, rejects manifest entries whose process start time does not match (to prevent PID-reuse attribution), and reads bounded tails from each mapped runtime journal. Samples retain launch-manifest mode/map/generation separately from journal observations, including actual mode when present, host DLL generation/NPC/player counts, NPC module generation and phase, server tick windows, and client FPS/viewport windows. A summary reports whether fresh server/client observations belong to the same run. Runtime manifest scans are cached; journal reads are bounded; the sampler records its discovery wall time.

## Evidence and limits

- Focused sampler suite: 7 tests passed, including isolated-run mapping, foreign-run journal rejection, PID-reuse rejection, and same-run/freshness requirements for paired evidence.
- `py_compile` and `git diff --check` passed.
- The native per-process sampler and runtime mapping have not yet been exercised by a root dev-loop build after this edit. Generation 574 (`game-dll-20261123T225702279702Z-0ed089`) was already active when the final sampler edits were made, so its sidecar is not proof of this change.
- No source-level CPU optimization or matched before/after reduction is claimed. Existing sampled-scope estimates remain partial: `updateOneNpc` at `src/npc/npc.cpp:1580` estimated 12.40% of server process CPU / 3.043% of host; `doCollisions` at `src/physics/movement/physics-collision-dispatch.cpp:181` estimated 5.33% / 1.308% of host. These are 1/64 sampled cycle estimates and do not partition all CPU.
- Generation 574 completed, but its resource sidecar came from the older daemon and attached root-global runtime context. The per-PID journal mapping remains unproven in the root daemon. Current state has a pending generation 56 batch with eleven game inputs, so restart only after its owner confirms persistence/recovery; then verify daemon identity and preserved PIDs before the next build sample.

## Native process I/O and thread-count extension

The Windows process fallback now records the thread count already present in
`PROCESSENTRY32`, and reads `GetProcessIoCounters` for the bounded set of
tracked/top processes. It emits cumulative read/write/other transfer bytes and
operation counts, plus per-second deltas scoped to the PID creation time and
the process-discovery interval. The deltas reset safely when a PID is reused.
These are Windows process I/O counters; they do not measure physical-disk
bandwidth and do not identify network bytes. GPU engine use, per-thread CPU,
CPU stacks, physical-disk throughput, and per-process network traffic remain
unavailable.

The focused suite now passes 8 tests, including per-process I/O rate calculation
and PID-reuse handling; `py_compile` and `git diff --check` pass. This telemetry
change does not alter game code or claim a CPU reduction. It is not loaded by
the root daemon yet: a shared Mode 3 game batch is pending, so a daemon reload
must wait until its owner confirms persistence/recovery. The next proof is a
live Windows API smoke plus a root build sidecar after safe daemon reload.
