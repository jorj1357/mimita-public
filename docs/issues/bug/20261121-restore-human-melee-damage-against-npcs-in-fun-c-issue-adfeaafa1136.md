# issue-adfeaafa1136: Restore human melee damage against NPCs in Fun Counter Strike

## Human report (verbatim, immutable)

> how come all the melee is not working int he live run righ tnow im having? i cant damagae npcs what changed
> 
> like it worked in fun counter strike i tihnk, me as human vs npcs the melee
> butnow it doesnt work, no melee does any damage


## Agent suggestion

### 2026-11-21T15:35:09.071Z — codex-melee-and-tdm-issues

Current follow-up status at 2026-11-21T15:29Z: dashboard launch run 20261121_152950_01 selected stale EXE artifact 2026-11-21T02:26:50.306117Z (SHA-256 8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871), mode 58 / Infinite World. Launch failed before gameplay because MiMITA process enumeration timed out; logs/2026-11-21/20261121_152950_01/dev-loop/events.jsonl. This is not the reported melee run and has no melee evidence. At follow-up status the runtime had no client/server and game-DLL generation 423 was still building. Do not infer the cause from this launch.

## Agent suggestion

### 2026-11-21T15:47:00.405Z — codex-melee-change-trace

Repository trace found pinned run 20261121_140624 (TDM melee validation, profile devscripts/dev-profiles/tdm-melee-validation.json, scenario devscripts/scenarios/melee_tdm_ai_v1.json, map dust2cyberiav4) on game DLL generation 420 and stale EXE artifact 2026-11-21T02:26:50.306117Z. The scenario explicitly drove player actor 1 through player_control ai melee mace, so this is AI-driven client input, not proof of ordinary human input or Fun Counter Strike. Client journal: 935 Mace claims + 100 Chainsaw claims; 924 batches marked sent. Server journal: all 924 batches received; 321 accepted and applied to NPCs (285 Mace, 36 Chainsaw), 629 rejected: 383 invalid_contact_bounds, 82 target_generation_changed, 58 out_of_reach, 51 target_dead, 47 friendly_fire, 8 world_occluded. Of 285 accepted Mace contacts, 232 had action_active=false/multiplier=1.0 and damage was 1-5; 53 had action_active=true/multiplier=10.0, including 27 at damage 250 and some >250. This proves this exact generation did not globally disable melee and identifies low non-active Mace damage plus rejected contacts as likely contributors to a 'does no damage' perception; it does not establish the current human/FCS failure. Changelog pointers: 20261120_072300-arcade-npc-melee-and-mace-active-pose.md documents the Gen398 Arcade/Mace change and that its first two launches failed before logger.started; issue-503f3e410c01 carries the later Gen417 AI rejection evidence. Follow-up must capture a human-controlled FCS run and first-divergence events.

## User quote

### 2026-11-21T15:55:15.083Z — codex-melee-ai-input-trace

Via codex-melee-ai-input-trace; supports issue-adfeaafa1136:

> i didnt do damage at all to any npc at all in the run i was in , like i saw the macepass thru enemies , it didnt do damage at all,i s that related to the  ai npc controller for the client ?

## Agent suggestion

### 2026-11-21T15:55:15.887Z — codex-melee-ai-input-trace

User clarifies the observed symptom was zero damage while seeing the Mace pass through NPCs, and asks whether the client AI controller was involved. The cited run 20261121_140624 was explicitly AI-driven: scenario melee_tdm_ai_v1 executes player_control ai melee mace. It predates current uncommitted engine-tick-combat.cpp change gating AI melee primaryHeld on the target-gated lastAiInputFrame().primaryHeld. Earlier gen417 evidence in the issue reports 145 AI claims sent and 0 accepted due own-team target/friendly_fire and invalid bounds. Gen420 evidence is mixed (321 applied contacts, many rejects). Therefore this AI path is a plausible explanation if the user's local player was under PlayerAiController; it does not explain a manually controlled human client, whose branch continues to use meleePrimaryPressed/mouseDown. Must identify whether PlayerAiController was active in the user's actual run before assigning cause; reproduce manual FCS separately.

## Agent suggestion

### 2026-11-21T16:11:10.492Z — codex-autonomous-melee-runtime-investigation

Source trace indicates the reported manual-input failure should not be attributed to the client AI melee controller: src/engine/engine-tick-combat.cpp uses AI lastAiInputFrame primaryHeld only when aiMeleeMode is active, while the human branch still uses meleePrimaryPressed and mouseDown. Available run 20261121_140624 runtime evidence is AI-driven TDM on stale EXE artifact plus game DLL generation 420, with Mace claims reaching authoritative NPC damage; it does not cover human-controlled Fun Counter Strike. The registered run 20261121_144350 is Payload mode, stale build_match=false and client-01 offline, so it cannot cover the manual swing. Next decisive run: launch isolated human-controlled FCS on identified EXE/DLL artifacts and inspect client input/contact claim, server receive/reject reason, and NPC health event in exact run journals.

## Triage

### 2026-11-21T16:23:16.761Z — codex-autonomous-melee-runtime-investigation

Summary: In run 20261121_144350, a focused human-controlled client emitted Mace contact claims against NPCs, but 254 recorded melee claims were rejected by the server as future_contact. The client kept using cached server tick 43683 while incoming snapshots and the server process had restarted to lower ticks. Trace and fix session tick-epoch reset/correlation, then prove a human FCS Mace hit on a current pinned build.
Owner: (unchanged)
Priority: high

## Agent suggestion

### 2026-11-21T16:23:17.336Z — codex-autonomous-melee-runtime-investigation

Concrete runtime failure is in logs/2026-11-21/20261121_144350/client-01/events-0010.jsonl. At line 55468, human client PID 11492 queues a Mace contact claim against NPC 200005 with action_active=true and contact_tick=43683. At line 55482, server PID 29352 rejects the same contact_id=10 as future_contact at server_tick=8998 (delta 34685); another rejection at line 58562 is still future_contact at server_tick=9621. This run has 264 client melee claim events and 254 future_contact rejections. At line 53372, input.state is focused=true and ai_active=false just before the Mace claims, so this is not the AI controller path. The client snapshot diagnostics later report cached server_tick=43683 while incoming snapshot_tick advances around 16053-16337. Source confirms the mechanism: multiplayer-tick.cpp only raises latestServerTick when a snapshot tick is greater, while teardownPreviousSession in multiplayer-packets.cpp clears snapshot/interpolation state but never latestServerTick; melee-client.cpp clamps to that cached value; server-packet-handlers.cpp rejects claims over 120 ticks in the future. Likely fix: reset/re-anchor the existing latestServerTick on server/session epoch change and ensure snapshot tick state is tied to that epoch; do not relax server validation. Run manifest identity is stale too: it records server PID 7556 online/client PID 11492 offline, while rejection events are server PID 29352, so capture process start identity and DLL generation before claiming this is the current user run. The observed process is now server-only, so human current-build acceptance remains open.

## Triage

### 2026-11-21T16:26:52.438Z — codex-autonomous-melee-runtime-investigation

Summary: In run 20261121_144350, a focused human-controlled client emitted Mace contact claims against NPCs, but 254 recorded melee claims were rejected by the server as future_contact. The client retained cached server tick 43683 while snapshots and an apparent replacement server used lower ticks. Trace and fix session tick-epoch reset/correlation, then prove a human FCS Mace hit on a current pinned build.
Owner: (unchanged)
Priority: high

## Assignment

### 2026-11-21T16:38:57.597Z — codex-melee-gold-fix

Owner: codex-melee-gold-fix

## Agent suggestion

### 2026-11-21T16:40:22.128Z — codex-melee-gold-fix

Human-defined acceptance from the latest request: replace the vague phrase melee works with separate authoritative proof predicates. In a pinned human-input TDM/Fun Counter Strike test, prove (1) local player melee damages enemy NPCs, (2) local player melee damages enemy players, (3) enemy players melee damages the local player, and (4) enemy NPC melee damages the local player. For each direction, correlate client input/contact claim, server receipt/acceptance, one server.damage.applied with the correct attacker/victim types, and a strictly lower resulting health value; no duplicate application for the same contact. Repeat each scenario in three independent runs with at least ten accepted damaging contacts per direction total. Exercise three reconnects including a lower server tick epoch; after each reconnect, connection returns to Connected, snapshots advance in the new epoch, subsequent melee is accepted, and no future_contact rejection is caused by stale cached ticks. All involved processes remain alive through a five-minute post-reconnect soak with no crash or abnormal-exit event. Record exact EXE hash/build, game DLL generation/path/hash, config revision, run ID and each journal. Runtime proof and human visual acceptance remain separate.

## Agent suggestion

### 2026-11-21T17:01:22.500Z — codex-melee-gold-fix

The source fix clears latestServerTick, lastSnapshotTick, snapshot diagnostics/timing/counters, and identity mismatch state in teardownPreviousSession; server future_contact validation remains unchanged. The exact root dev-loop attempt game-dll-20261121T165408308526Z-55ebed failed before compilation at packet_registry_codegen.py while generated metadata was stale. After regeneration, standalone packet_registry_codegen.py --check passes, but revision 2 is latched failed in the dev-loop; CLI has no explicit force-retry option. Shared run 20261121_144350 remains untouched and has no client. No runtime proof; retain issue open and run isolated pinned FCS acceptance after a successful build. See docs/regressions/networking/melee-contact-tick-epoch-REG-2026-11-21.md and docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md.
