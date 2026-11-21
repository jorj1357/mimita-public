# issue-0a2a3fc00768: Fun Counter Strike NPC aim and time-to-kill should reflect human reaction limits

## Human report (verbatim, immutable)

> npcs need to be difficult but they have too quick of a kill time,need liek 200 miliseconds? or in ticks idk pro level human reaiton time, but npcs laser me to death with revolver, cant ahve that, in fun counter strke ur a lot more inaccurate bc the hpscial arm aim momvement mode, npcs need to reflect that in teh mode i think per mode


## Assignment

### 2026-11-21T15:18:27.331Z — opencode

Owner: opencode

## Agent suggestion

### 2026-11-21T16:06:28.081Z — visual-access-and-issue-review

REVIEW EVIDENCE (visual-access-and-issue-review), no code changed; issue owner is opencode.

Root cause is config + code defaults, not a mystery:
1. config/gamemodes/funcounterstrike.json:285 selects fun_cs_rush.
2. config/behavior-profiles.json:323 fun_cs_rush sets skill_tier elite and does NOT override aim_error_deg or reaction_delay, so src/npc/npc-behavior.cpp:70-72 applies elite: aimErrorDeg=0.06 deg, reactionDelay=0.01 s (10 ms), turnSpeed=1800 deg/s, fireCadenceMultiplier=1.35.
3. config/npc-difficulty.json sets perceptionReactionTicks=1 (16.7 ms), perceptionFovDegrees=359, perceptionHearingRange=1600, targetMode=player, npcHitRadius=0.1, npcDebugVisuals=true.
4. npc.cpp:3670 uses behavior.reactionDelay when >0, else perception delay; npc-combat.cpp:175-181/255-271 applies the behavior aim error. Combined acquisition-to-first-shot is ~10-27 ms at 0.06 deg, i.e. effectively instant.
5. TTK: revolver damage 15 (headshot 3x=45) vs 100 HP, fire_delay 0.008 s, cadence x1.35 -> 3 headshots or 7 body shots after acquisition. That matches "laser me to death".
6. Per-mode gap: NPC aim path never reads GameplayConfig::aimMode(); the physical aim model is only applied to human shots in src/combat/weapon-fire-raycast.cpp:80-107. So Fun CS "physical arm aim" inaccuracy is not reflected in NPC accuracy at all.

Lowest-risk fix aligned with docs/specs/npc-behavior/npc-behavior.md:945 (edit JSON, hot reload, no parallel tuning file or mode branch):
- In fun_cs_rush add reaction_delay ~0.20 (user says ~200 ms, ~12 ticks) and aim_error_deg ~2-4, and a fire_interval_ticks hard floor for sustained TTK; keep elite movement/objective tactics so NPCs stay difficult. Easy profile already shows the pattern (25 deg, 120 ticks).
- For per-mode: add ONE shared aim-model-to-NPC-accuracy mapping (physical/hybrid adds movement-scaled aim error and/or lower effective turn speed) rather than a funcounterstrike branch. That requires human pre-review because it touches shared NPC combat tuning.
- Audit config/npc-difficulty.json debug-looking globals before shipping; they affect every profile that does not override them.

Proof expectation: pinned FCS run journal with npc.shot actual_error_deg/max_error_deg and npc.fire-blocked reaction_timer; report reaction-to-first-shot and shots/ms-to-kill across repeated encounters; human balance acceptance. Treat 200 ms as a starting value, not a final constant.

## Agent discussion and suggestions

### 2026-11-21T16:10:35.627Z — visual-access-and-issue-review

Message: `msg-fc45a26ff03d` (message)

Subject: FCS NPC TTK/accuracy review evidence on issue-0a2a3fc00768

I appended concrete review evidence to issue-0a2a3fc00768 (owner: opencode). Root cause: fun_cs_rush inherits elite aimErrorDeg=0.06 deg and reactionDelay=0.01 s from src/npc/npc-behavior.cpp:70-72; config/npc-difficulty.json has perceptionReactionTicks=1, fov 359, hearing 1600, targetMode player, hitRadius 0.1, debug visuals true; revolver fire_delay 0.008 s. NPC aim also never reads GameplayConfig::aimMode(), so physical arm-aim inaccuracy is not reflected. Suggested config-first change is in the issue; shared per-mode aim-model mapping needs human pre-review. No code changed.
