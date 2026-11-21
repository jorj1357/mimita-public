# Client AI controller and Mace pass-through trace

## Clarification

The user reports zero damage to every NPC despite seeing the Mace pass through enemies, and asks whether the client AI controller could be involved. The verbatim follow-up was linked to issue-adfeaafa1136.

## Evidence

The TDM journal cited in the prior investigation was explicitly AI-driven: devscripts/scenarios/melee_tdm_ai_v1.json invokes player_control ai melee mace for the local client. It is not a human-input run and cannot prove the user's manual attack path.

The issue records a prior gen417 AI failure: 145 client claims sent, zero accepted, due target selection on same-team NPCs and invalid contact bounds. Gen420 TDM run 20261121_140624 is mixed: server received 924 batches, accepted 321 damage contacts, and rejected 629. Therefore AI-driven melee can produce invalid/rejected contacts and can plausibly look like the weapon passes through targets.

Current src/engine/engine-tick-combat.cpp gates AI melee press/hold on lastAiInputFrame().primaryHeld. Its normal human branch still reads meleePrimaryPressed and mouseDown. Current src/engine/engine-tick-net.cpp saves the AI input frame for that gate. The gen420 run predates this current uncommitted source change. A human-controlled client does not take the AI branch.

The latest dev-loop status has no running client/server and the published EXE is stale; a game-DLL build is in progress. No live manual Fun Counter Strike reproduction is available in this checkout.

## Conclusion

The AI controller is a plausible cause if it was driving the local client in the user's run. It is not the explanation for a manually controlled human client. Determine whether PlayerAiController was active in the exact run, then compare its client claims to server batch receipt/rejection and NPC health events. The user's follow-up and this distinction were appended to issue-adfeaafa1136. No gameplay source changed and no runtime fix is claimed.
