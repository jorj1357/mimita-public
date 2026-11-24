# Separate process-pair alignment from mode verification

# Task

- Task ID: session-b1a94efb03e5
- Session ID: session-b1a94efb03e5
- Owner: resource-profiler
- Focus: Build resource sampler runtime alignment evidence
- Summary: Separated fresh same-run client/server workload alignment from independent authoritative mode verification. The sampler can now preserve valid process/frame/tick evidence when one side's mode event is absent or stale without asserting that the two game modes match. Replayed the retained generation-594 resource sidecar; the server/client pair and workload evidence are now visible, while client mode remains unverified. The collaboration specification already contains the user's requested falsifiable scientific loop and at-least-three ranked optimization hypotheses; no duplicate policy text was added in this activation.
- Status: BLOCKED
- Date, time, timezone: 2026-11-24T14:32:00+00:00
- Validation: Focused sampler suite 15/15 passed; Python byte-compilation passed; scoped diff check passed. No runtime optimization, game build, or live workload proof was performed.

# Evidence

- Retained artifact: `.dev/build-timings/game-dll-20261124T122208532874Z-63061a.resource.jsonl`
- Offline replay result: run `20261124_121332`, server PID 25840, client PID 12848, `pair_alignment_status=observed`, authoritative server mode `bloodtest`, `mode_verification_status=client_mode_unverified`.
- Server journal windows: 1 NPC/1 player, 61 server ticks, 33.6106 ms total in the reported window.
- Client journal window: 156.816 FPS, p95 11.3371 ms, 97/157 frames over the 4.1667 ms budget; one remote NPC, out of view.
- Focused validation: `python -m unittest -v devscripts.quality.test_build_resource_sampler` (15 tests passed); `python -m py_compile devscripts/dev_loop/build_resource_sampler.py devscripts/quality/test_build_resource_sampler.py` passed.
- Current live sidecar generation remains unloaded in the root dev-loop at the time of this record; reload/runtime verification is pending coordination with the root owner.

# Exact implementation changes

## `devscripts/dev_loop/build_resource_sampler.py`

- `_runtime_alignment_summary()` at lines 869-959 no longer requires a fresh mode event to identify a same-run server/client resource pair. It separately validates fresh mode events, labels pair alignment and mode verification independently, retains authoritative server mode, reports stale/unreported client mode, and includes fresh server and client workload rows.
- Before: server/client candidate selection required their mode journal rows to be fresh, so stale/missing client mode made pair resource alignment unavailable.
- After: fresh server NPC/tick and client frame/NPC rows establish pair alignment; mode agreement is separately `matched`, `mode_mismatch`, `client_mode_unverified`, or `server_mode_unverified`.
- `stop()` computes the alignment once and reports the client confirmation gap separately from otherwise available pair/workload evidence.

## `devscripts/quality/test_build_resource_sampler.py`

- Added coverage for same-run alignment with missing/stale client mode and for summary-level separation of pair evidence from client-mode confirmation. Updated expectations for explicit pair/mode statuses.

# Scope and remaining work

- Existing uncommitted edits from other work were preserved and not included in this change.
- The captured resource rows are telemetry; they do not identify a function-level CPU hot spot or prove an optimization.
- The next discriminating step is a coordinated root-daemon reload that preserves the current processes, then verify the next sidecar has process-pair alignment and fresh authoritative workload rows. Runtime optimization must wait for an owner-approved matched workload with adequate headroom; test one falsifiable candidate at a time and preserve frame-time, 60 Hz tick, and gameplay checks.
