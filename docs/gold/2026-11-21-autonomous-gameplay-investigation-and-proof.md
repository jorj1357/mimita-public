# Gold workflow: autonomous gameplay investigation and proof

- Recorded: 2026-11-21
- Status: preferred workflow; this document does not claim the current melee fix is runtime-proven.
- Applies to: gameplay regressions, networking, AI behavior, launch failures, and other user-visible bugs.
- Related: `docs/gold/2026-11-04-events-jsonl-only-improvement-loop.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/agent-proof-and-verification.md`.

## Start from the user's outcome

Preserve the report verbatim in the issue, then convert phrases such as “it works” into observable outcomes. Name actors, mode, action, expected state change, repetition count, recovery cases, and failure-free duration. Keep source/build, runtime journal, and human acceptance as separate evidence claims.

For melee, “works” means the local human can damage NPCs and enemy players; enemy players and NPCs can damage the local human; each valid contact causes exactly one authoritative damage application and a lower target health value. Repeat across independent pinned runs and after reconnects to a server with a lower tick epoch. Require successful reconnection, fresh snapshots, subsequent melee, and a post-reconnect crash-free soak. Record exact EXE/build ID, DLL generation, configuration, mode/map/loadout, process identities, run ID, and per-process journal paths. Human visual/gameplay acceptance remains distinct.

## Trace before assigning blame

Use the exact run the user reports when available. Identify process and build identities, then follow one action through input, client claim, transport, authoritative validation, damage application, and resulting health. Find the first missing or rejected stage in that chain. Do not blame the NPC controller, networking, or the binary until the journal and owner code support that conclusion. Preserve uncertainty when run metadata conflicts.

Read the owner code and relevant history/specification; compare the last known working revision only when its artifact and journal can be identified. Use existing events first. Add only bounded, rate-limited owner-level diagnostics where the journal cannot distinguish competing causes.

## Iterate through the canonical runtime

Make the smallest owner-level change. Before saving a DLL compiler input, queue its build note. Use the root dev-loop as the sole builder and launch authority. Reuse the user's run only when it is the target run and safe; otherwise use a pinned isolated run that does not replace the shared runtime. Never substitute a successful build or a synthetic harness for live behavior evidence.

After each attempt, inspect the exact attempt timings, artifact identity, and that run's `events.jsonl`. If it fails, identify the stage and reason, update the issue, and correct the earliest divergence. Keep attempts and proof distinct; do not call behavior fixed until acceptance evidence passes. Stop only at completion or a recorded external blocker, preserving healthy user-owned processes.

## Regression memory

For a confirmed behavior regression, append a regression record with the prior symptom, first failing stage, source-level cause, attempted change, exact evidence, and what remains unproven. Keep the status honest: “attempted fix” until runtime proof succeeds. Once the same behavior passes, append the exact version-specific proof so a later change can be compared against it.

This workflow is “gold” as the preferred investigation behavior. The current tick-epoch melee patch remains unproven until an isolated pinned build and runtime journal meet the stated acceptance.

## Agent-console control for in-game GUI

When Codex needs to inspect or operate MiMITA's in-game GUI, use the dev-loop agent console commands such as `agent.gui_scan`, `agent.gui_button <element_id> down|up`, `agent.mouse_click`, and `agent.screenshot`. Do not use Computer Use for MiMITA GUI interaction until that path is ready and the user asks to change this preference. A console command being delivered is not proof that the intended widget received the click: compare the scan's element ID and rectangle with the resulting `ui.button_input` event's element ID, click edge, and result. If the IDs differ or `mouse_click_edge_missing` appears, record it as a control-path bug and fix the shared coordinate/input owner before relying on agent-driven GUI acceptance.

This user has reported that console-driven GUI button presses still show a longstanding bug and explicitly prefers not to use Computer Use yet. In run `20261122_024031`, pressing scanned `leaveButton` down/up was logged by the UI owner as `resetButton`, with `clicked=false` and reason `mouse_click_edge_missing`; this is a confirmed mismatch in that run, not a proof of its cause. Retest scan -> press -> release against the exact run journal after any fix.

Follow-up evidence from run `20261122_184056` (EXE build `2026-11-22T17:39:49.763492Z`, game DLL generation 512): scans exposed `leaveButton` on the pause menu, but agent-console down/up requests did not produce a matching `ui.button_input` for that ID or leave the menu. The client instead reached a confirmation panel and then a later scan returned zero widgets; no Community Browser row was reached. The server remained alive with room code `GEFS5FF`. This reproduces command delivery without verified target activation and confirms the workflow must stop short of claiming a GUI action until a matching button-owner event and resulting screen transition are observed. Journal: `logs/2026-11-22/20261122_184056/client-01/events-0025.jsonl` seq514228 (release), and `client-01/events.jsonl` seq249034/249036 (pause-menu scan/command), `events-0025.jsonl` seq493440/545972/552158 (press, confirmation panel, empty scan). No Computer Use was used.

Follow-up control case from run `20261122_195603` (pinned EXE build `2026-11-22T19:53:22.116562Z:131d88c3f6f4720e5ec53f1019564256b1368dbddf349ec87bb4f15149ce0221`, game DLL generation 513): `agent.gui_scan` exposed public room row `serverbrowser_join_4686QTU` (client seq 89762). `agent.gui_button` produced a matching `ui.button_input` with `clicked=true` and `decision=accepted` (seq 90784); the client then requested room `4686QTU` and passed preflight (seq 90792/90794). Connection 2 subsequently received snapshots from `ice:4686QTU`, with 20 entities and roughly 1.0–1.43 second interarrival; one metrics window recorded 26 snapshots in 1003 ms and RTT 45 ms. This proves one in-game public-list button-to-snapshot path works through the agent console. It does not prove the user's reported server acceptance failure is fixed or reproduce that failure. Keep the networking issue open. No Computer Use was used.

Correction and additional runtime evidence for run `20261122_195603`: the server accepted the public-list join, not just the client click. The server journal records `network.server.join.validated` seq 81206 at 20:09:52.218Z and `network.server.join.accepted` seq 81216 at 20:09:52.221Z for player 2 (`join_accept_sent`, `result=ok`). The client applied a complete snapshot at seq 90990 at 20:09:52.285Z with 20 entities / 19 NPCs, then seq 91184 one second later with `snapshots_missed=0`. This proves acceptance plus authoritative snapshot delivery for this control case.

The same run also produced a real stale-snapshot event under the active machine workload: client seq 94936 at 20:10:25.363Z reported `snapshot_age_ms=3081` against a 2500 ms stale threshold. Server seq 84854 at 20:10:25.380Z reported the matching fixed-tick stall: 3081.053 ms total, 2775.864 ms in `snapshot_ms`, 228.492 ms NPC simulation, 26.872 ms gamemode, with 19 NPCs and one player. `runServer` measures `serverFrame.publish_us` around the publish callback (`src/network/server.cpp`, around line 1593); `serverRuntimePublish` calls snapshot construction/sends and replication (`src/game/server-runtime-module.cpp`, around lines 479–493). The callback timing does not yet divide those subcalls, so it narrows the first late stage to publish/snapshot work without identifying the exact blocking operation. Keep this performance/connectivity issue open until subphase evidence and a runtime fix pass.
