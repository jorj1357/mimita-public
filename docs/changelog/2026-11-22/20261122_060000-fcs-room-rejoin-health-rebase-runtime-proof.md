# FCS room rejoin and health-clock rebase runtime proof

## Source change

Updated `agent.gui_button` release handling in `src/main-systems.cpp`: when a scanned GUI target disappears between a mouse-down and mouse-up, the up command now releases the agent-controlled left mouse button and logs `target_present=false` and `release_applied=true`. This fixes an in-game console interaction that could leave the mouse logically held after a confirmation dialog replaced its prior screen. The change was included in game DLL generation 459. The EXE remained pinned to build `2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344`.

## Build evidence

The root dev-loop published DLL generation 459 from revision `9556b66...`; its reported code hash was `6eedb0447a21263479fc66998719bec55256bc099bb52368b632001fce82386f`. The pinned test run hotloaded generation 458 and then 459 into its client process. No EXE rebuild was required for this DLL-only change.

## Runtime evidence

Run `20261122_052434` exercised Fun Counter-Strike mode 45 on `dust2cyberiav4`. It pinned the EXE build above and used server PID 11160 and client PID 34316. The server registered room `S5KWDGU` (`server-01/events-0047.jsonl`, seq 146); the client initially joined successfully (`client-01/events-0046.jsonl`, seq 176).

In the in-game GUI, the client opened the leave confirmation, released the disappeared `leaveButton` target (`client-01/events.jsonl`, seq 307324, `release_applied=true`), and accepted the `confirmYes` button (seq 313470). The client returned to its browser and selected the freshly scanned `serverbrowser_join_S5KWDGU` row. The JOIN click was accepted (seq 329800), it requested a connection to the same room (seq 329808), and the client reached Connected on attempt 2 (seq 329986). The server accepted the rejoin and assigned player 2 (`server-01/events.jsonl`, seqs 129847 and 129858). A later post-rejoin snapshot was applied (`client-01/events.jsonl`, seq 448522) with 1013 ms interarrival.

After generation 458 activated, the client logged `network.hotswap.health_rebased` with reason `generation_health_clock_rebased`, rebasing old health ages of 1271 ms to zero (`client-01/events-0046.jsonl`, seq 101984). The generation 459 rebase is in `client-01/events.jsonl`, seq 246574. Journal searches found no client reconnect schedule or reconnect attempt after either rebase and before the deliberate GUI leave; authoritative snapshots continued. This demonstrates no false client reconnect in this observed run window, not a statistical guarantee.

## Remaining issue evidence

The run also exposed a real snapshot interruption around the generation 458 swap: client seq 102202 reported 4743 ms snapshot interarrival and 35 missed snapshots, while the server logged a stale-peer classification after 2516 ms silence at seqs 177544 and 177546. Later snapshots resumed, but the root cause and repeatability are unresolved. The isolated run was stopped through the dev-loop after evidence capture. Parent issue `issue-cf9cd77eb056` remains open; it needs repeated pinned scenarios and investigation of the multi-second snapshot gap before its broader networking acceptance criteria can pass.

## Issue tracker update

Appended this pinned-run evidence and the remaining stall to `issue-cf9cd77eb056`. Proof ledger tasks `fcs-room-create-20261122_052434`, `fcs-health-rebase-20261122_052434`, and `fcs-same-room-rejoin-20261122_052434` were verified from their runtime journals. Build success, runtime observations, and unresolved reliability work are recorded as separate claims above.
