# Command-driven GUI testing and browser-join investigation

## Scope

Started the requested command-driven GUI workflow: scan visible widget IDs/rectangles, press/release by widget ID, scroll, and capture a run-local screenshot. Continued investigating the white-screen report through the root dev-loop and an additive test client.

## Source changes

- `src/main-systems.cpp`: added `agent.gui_scan`, `agent.gui_button`, `agent.mouse_scroll`, and `agent.screenshot` commands. The screenshot path is constrained to run-local `logs/` BMP output. These changes are source-only at this point.
- Build note `9b5e545cfada` was queued before the edit.

## Evidence and limits

- Root dev-loop run `20261119_155456` remained alive with its original server/client and an added `client-02` (PID 29116). No pre-existing process was stopped.
- The client was launched from artifact `2026-11-19T15:17:05.094869Z` with DLL generation 361; it did not contain the new GUI commands.
- Command CLI invocation for `agent.mouse_move` and a second bounded invocation for `agent.gui_scan` both stalled before producing a control response. The run-local command queue remained 0 bytes, and no command request/dispatch/completion appeared in the journals. Therefore no UI click flow was exercised and no client-side command-delivery conclusion is supported.
- The first automatic DLL attempt failed at `initial_source_coherence` because `src/main-systems.cpp` changed during the snapshot. A subsequent auto attempt (`game-dll-20261119T162017354770Z-d1ce8a`) was still in `manifest_snapshot` at the last status read. No build success, activation, runtime GUI evidence, browser-join reproduction, or fix is claimed.
- Existing report suggests the GUI browser join path sets `GAME_PLAYING` before the pending network join has progressed, but the queue consumer is also nested under the `GAME_PLAYING` entry path. This remains a hypothesis until a live reproduction identifies the first divergence.

## Follow-up

Resolve the daemon/CLI command path so it returns an accepted control response and writes the per-client queue. Then activate the source change through the root dev-loop, verify scan/click/screenshot evidence in `client-02`, reproduce the exact leave/create/join flow, fix the first demonstrated failure, repeat the scenario, and append a confirmed regression record.
