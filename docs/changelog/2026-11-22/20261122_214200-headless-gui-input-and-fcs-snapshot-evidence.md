# Process-local agent GUI and FCS snapshot evidence

## Code changes

- `src/main-systems.cpp` (former global-input behavior: `mouse_event(...)`, `hostFocusWindow()`, `hostSetCursorPos(...)`; new behavior: `InputRouter::ingestMouseButton(...)`, `InputRouter::ingestCursor(...)`, and `queueAgentMouseClick(...)`). Agent GUI click/move/button commands now target the process InputRouter. `agent.gui_button` records `input_scope=process_local` and `window_focus_requested=false` when its injection path runs.
- `src/input/input-router.h` and `src/input/input-router.cpp` (new `queueAgentMouseClick(int button)` plus per-frame press/release pulse state) implement agent cursor/button state without the OS desktop cursor.
- `src/renderer/renderer.cpp` and `src/host/host-runtime.cpp` (new `MIMITA_AGENT_HEADLESS_GUI=1` checks) create isolated test clients hidden and suppress show/focus/foreground calls.
- `devscripts/dev_loop/parallel_test_runs.py` sets that environment variable for each isolated client process.
- `src/npc/recast-navigation.cpp` now defers full navmesh rebuilding inside fixed ticks and emits bounded diagnosis for expensive Recast queries.

## Validation

- Root dev-loop EXE build succeeded: `2026-11-22T21:32:48.195707Z:e24aaf16cbfb97461c3d51e25f7f1424c56865155889c6be816a7bc9b9b44e82`; isolated runtime pinned that executable and game DLL generation 526. Some unrelated source changed during the EXE build; this artifact is evidence for the task files that were stable before the build, not a globally current snapshot.
- Runtime `logs/2026-11-22/20261122_213500`: FCS on `dust2cyberiav4`, 19 NPCs. Room registration completed in 453.4 ms and issued room code `G6B6QYT`; configuration load took 1404.8 ms and world load 1925.2 ms. Client journal recorded one initial `network.client.snapshot.gap` at 21:35:30.091Z with age 6435 ms against a 2500 ms stale threshold, then 279 applied snapshots by capture end. The server recorded a 737.616 ms tick stall at 21:35:08.551Z, of which NPC work was 731.961 ms. This does not prove resource-pressure reliability or no reconnect.
- Per-run console delivered `agent.gui_scan` and `agent.gui_button leaveButton click` to client PID 18576. The hidden client had `MainWindowHandle=0`, and prior window enumeration showed its windows invisible. The journal has command delivery/completion but no `agent.gui.button` or `ui.button_input` outcome for the click. Therefore no semantic button activation or unchanged foreground/z-order is claimed. This remains open under `issue-cf0cb7818822`.
- Runtime was stopped through the dev-loop after evidence collection; unrelated runs were preserved.

## Issue updates

Posted the snapshot starvation/tick-stall measurements to `issue-cf9cd77eb056` and `issue-453d0596e294`; posted headless per-process command limitations to `issue-cf0cb7818822`. The networking issue remains open pending diagnosis and proof of recovery under load.
