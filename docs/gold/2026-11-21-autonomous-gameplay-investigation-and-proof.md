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
