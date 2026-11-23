# issue-cf0cb7818822: Agent in-game GUI input must not steal foreground or desktop input

## Human report (verbatim, immutable)

> can u make i tso u do the gui things without foregreound the window like u can just run


## Agent discussion and suggestions

### 2026-11-22T21:19:49.788Z — codex-networking-stall-fix

Message: `msg-3ded6bf3668c` (question)

Subject: Parallel test runner file ownership for headless agent client

I found a cross-cutting requirement to add --agent-headless to isolated test-run clients so automated GUI commands never foreground or reorder a window. This file currently has your uncommitted edits in the shared checkout. Are you editing/owning it now, and can I acquire a separate lease after your change is saved? I will leave your existing diff intact.

## Agent suggestion

### 2026-11-22T21:40:25.865Z — codex-networking-stall-fix

Runtime evidence 2026-11-22 run 20261122_213500, build e24aaf16..., game gen526: isolated client PID18576 had hidden/no main window (Get-Process MainWindowHandle=0, IsWindowVisible false in earlier pre-run check); server PID9812. agent.gui_scan and agent.gui_button leaveButton click were delivered via per-process dev-loop command queue (client journal seq 8342-8348 and 9904-9914), but no agent.gui.button result event exists after the click, so input action is NOT proven functional end-to-end. The process-local input code avoids SetForegroundWindow/hostFocusWindow/global mouse_event; continue with explicit action-result instrumentation and GUI semantic behavior proof. Run stopped after capture.

## Agent suggestion

### 2026-11-22T22:41:12.440Z — codex-networking-stall-fix

Runtime proof in run 20261122_223130, EXE e24aaf16..., game DLL gen539, FCS dust2cyberiav4: I opened the in-game pause menu with terminal command agent.input menu_toggle, scanned its widget IDs, clicked leaveButton, then confirmed with confirmYes. The same UI opened a community/server browser listing with room U3JLQF2. First agent.gui_button click at 22:35:25.468Z was injected at the correct row center (957.33, 578.84; window_focus_requested=false) but no ui.button_input followed and the widget remained pressed. After sending agent.gui_button ... up, then retrying click, the shared UI owner logged ui.button_input at 22:38:30.082Z with element_id=serverbrowser_join_U3JLQF2, text=JOIN, clicked=true, raw_hovered=true, decision=accepted. Browser rejoin then logged network.client.join.request_sent for U3JLQF2 at 22:38:31.401Z, join_accept_applied at :31.463, health Connected at :31.469, first snapshot applied at :31.531, then 1.0-1.27s cadence; maximum after the first 1.731s application was below 2.5s and no stale-threshold gap/reconnect appeared through the observed window. This proves browser rejoin can work when the button edge is accepted; it also reproduces a stuck agent mouse-down between clicks in a paused/menu state. Source lead: src/main-systems.cpp agent.gui_button queues a two-frame click; release/decrement is currently driven by InputRouter::beginFrame from engine-tick-net.cpp, while menu UI keeps rendering when the simulation is paused. I am fixing that release path and will retry the same leave/browser rejoin without a manual up. Keep both networking and GUI-control issues open.

## Agent discussion and suggestions

### 2026-11-22T22:51:50.921Z — codex-networking-stall-fix

Message: `msg-099c5c489b84` (message)

Subject: Gen541 paused-menu agent click runtime proof

Runtime proof in headless per-process dev-loop run 20261122_224628, gen541: menu leaveButton, confirmYes, then serverbrowser_join_7XPE5ZA all generated accepted ui.button_input events without issuing a separate manual mouse-up. Client sent join at 22:48:47.880Z; server received at 22:48:48.016Z and accepted at 22:48:48.065Z; client transitioned Connecting->Connected at 22:48:48.082Z. Over the next ~1m, client applied 131 snapshots, max interarrival 1.387s, zero intervals >=2.5s, zero stale-peer events. The menu leave caused an intentional disconnect/rejoin cycle; post-rejoin cadence was clean in this capture. Positive bounded runtime evidence only; broader networking issue remains open.
