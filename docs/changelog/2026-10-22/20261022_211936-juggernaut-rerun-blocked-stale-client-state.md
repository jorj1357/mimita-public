# Juggernaut rerun blocked by stale dev-loop client state

- Task: Rerun the Juggernaut scenario and collect fresh journal/resource evidence to narrow the FPS and memory problems.
- Result: `BLOCKED` for a fresh game-client run. The existing dev loop accepted the launch request but falsely treated the stopped client as already running. Prior run evidence remains useful but no new gameplay performance window was collected.
- Time: `2026-10-22T21:19:36Z` (America/New_York display: 2026-10-22 17:19:36 EDT).
- Branch: `20261011zt`.
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created).

## Inspection and runtime evidence

- Existing dev-loop run was build 2849, Juggernaut mode 9, run `20261022_205937`, journal `logs/2026-10-22/20261022_205937/events.jsonl`.
- A fresh launch request was sent through the existing dev loop (`python devscripts/dev-loop.py --request launch --launch-mode 9 --visible`). The loop recorded `dev-loop.control` with action `launch`, mode `9`, and status `completed`, message `AI launched last successful build 2849` at `2026-10-22T21:18:21Z`.
- Independent process inspection found no non-server `mimita.exe` process. The state file simultaneously says `status=launch_latest`, `client_ready=true`, `builds_match=true`, `client_pids=[]`, `server_alive=true`, and retains old run ID `20261022_205937`. Journal size and last-write time remained unchanged at 12,760,762 bytes and 2026-10-22 17:18:21 local time. Therefore the launch message is not proof that a client was started, and no new game workload was measured.
- Source evidence: `devscripts/dev-loop.py:2301-2307` reuses a client when cached `client_ready` and `builds_match` are true, checking the server but not verifying a live client PID. `stop_processes()` at `devscripts/dev-loop.py:2615-2627` clears its process list and build marker but leaves those cached flags and `client_args` unchanged. The launch request handler at `devscripts/dev-loop.py:1646-1654` reports success when `launch_latest()` returns true. This explains the false-success path with high confidence.
- The relevant state predates this rerun: PID 9128 was stopped through the dev loop in the previous session, while the dedicated server PID 32016 remained alive. This run did not terminate or replace the server.

## What the previous gameplay evidence still says

The previous fresh client run did prove that the target scenario loaded 68 server collision actors and rendered 67 remote player bodies. Its last complete 60-frame window averaged 8.46 ms per frame, with 60/60 over the 4.17 ms budget for 240 FPS; main-thread CPU averaged 7.81 ms/frame. Those measurements point to client critical-path cost across rendering, remote-entity processing, effects, and frame presentation. They do not establish which owner retains memory.

The outside sampler recorded the previous client reaching 16.67 billion private bytes during a severe system-memory-pressure episode. The prior run journal stopped recording frame data after a performance spike while that client process remained present. That is strong evidence to prioritize client memory profiling and stall/crash diagnostics, but it is not proof of a leak or a specific allocator owner.

## Smallest next action

Fix the dev-loop launch reuse invariant so it confirms a live client process before returning “already running,” and clear `client_ready`/`builds_match` when stopping the client. Then retry a bounded mode-9 run with the external sampler and canonical journal. Do not change NPC/rendering code based on this blocked rerun; it produced no new gameplay measurements.

## Validation and scope

- Reviewed: `docs/ROUTER.md`, performance specification, runtime scenario workflow, efficiency checker, logging checker, build/EXE operations, task-completion procedure, and time/file-format standard.
- No source or runtime behavior was modified in this session. No build was run. No new gameplay run completed. No human gameplay review was performed.
- Existing user changes and prior session work were preserved. No process was stopped by this session; the live server remains running.
