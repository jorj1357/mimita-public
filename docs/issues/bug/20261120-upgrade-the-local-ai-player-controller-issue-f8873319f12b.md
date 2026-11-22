# issue-f8873319f12b: Upgrade the local AI player controller

## Human report (verbatim, immutable)

> my computer has a .exe of mimita being controlled by the ai NPC controller and it is stuck focusing on teammates, walking into corners, looking at npcs that are behind walls/it shouldnt know about, stuck in corners not mkaing meaningful progress, not dashing or doing any movement things etc, it needs tob e super upgraded


## Task linked

### 2026-11-20T04:59:51.417Z — runtime-60hz

Task: ai-target-visibility (step 0)

## Task linked

### 2026-11-20T04:59:52.620Z — runtime-60hz

Task: ai-navigation-movement (step 0)

## Context

### 2026-11-20T06:25:26.910Z — work-metrics

reproduction: Launch a local match with a player controlled by the in-game AI controller and observe target choice, navigation around walls, corner escape, and movement actions.
expected_behavior: The controller selects visible hostile targets, ignores unseen targets, escapes corners, makes progress, and uses jump/dash/weapon actions when appropriate.
acceptance: Live journal shows hostile visible target selection, no teammate focus, no wall-only tracking, and measurable progress with movement actions.
paths: ['src/actor/player-ai.cpp', 'src/actor/player-ai.h', 'docs/specs/ai/player-ai.md', 'docs/specs/dev-loop/agent-coordination.md']
evidence: {'runtime': 'logs/2026-11-20/20261120_040807', 'symptom': 'teammate focus, wall targets, corner sticking, no meaningful movement'}

## Context

### 2026-11-20T06:49:04.429Z — work-metrics

reproduction: Launch a local match with a player controlled by the in-game AI controller and observe target choice, navigation around walls, corner escape, and movement actions.
expected_behavior: The controller selects visible hostile targets, ignores unseen targets, rejects its own actor, escapes corners, makes progress, and uses jump/dash/weapon actions when appropriate.
acceptance: Live journal shows hostile visible target selection, no teammate focus, no wall-only tracking, and measurable progress with movement actions.
paths: ['src/actor/player-ai.cpp', 'src/actor/player-ai.h', 'docs/specs/ai/player-ai.md']
related_build: 397
related_run: 20261120_063800_01
evidence: {'build': 'generation-397', 'build_attempt': 'game-dll-20261120T063933641878Z-848734', 'compiled': 'src/actor/player-ai.cpp', 'runtime': 'pending; active server is generation 394'}

## Assignment

### 2026-11-20T14:39:24.115Z — arcade-collision-elite-proof

Owner: arcade-collision-elite-proof

## Triage

### 2026-11-20T14:40:07.199Z — arcade-collision-elite-proof

Summary: Upgrade the controllable client-side AI player for high-skill, human-like play and as a repeatable stress driver for Arcade capsule collision. Preserve fair shared weapon/action rules while improving target selection, navigation recovery, fast movement abilities, team coordination, and combat execution; prove movement and combat from a pinned live run.
Owner: arcade-collision-elite-proof
Priority: high

## Agent discussion and suggestions

### 2026-11-20T14:41:55.551Z — arcade-collision-elite-proof

Message: `msg-89e89eac14f4` (coordination)

Subject: Local AI controller file lease coordination

Please share your player-ai.cpp edit scope and release your active lease when your change is saved. I’ve taken issue-f8873319f12b and need to continue client AI improvements for the collision stress goal. I will not edit that file while your lease is active.

## Context

### 2026-11-20T14:45:18.758Z — arcade-collision-elite-proof

expected_behavior: The client AI behaves as a strong legal player: it targets enemies using the configured information policy, navigates without corner lock, uses available jump/dash/down-dash/freeze/air-strafe actions when tactically valid, coordinates with teammates, and successfully damages and kills using every usable weapon in its resolved mode loadout. It obeys ordinary movement, collision, aim, ammo, cooldown, and damage authority; it does not force hits.
acceptance: On a pinned local TDM run, journal evidence shows no teammate as a hostile target, no ranged fire through occluding world geometry, successful route/movement progress, legal jump/dash/down-dash/freeze/air movement when available, teammate coordination, and at least one server-confirmed kill with every usable weapon in the resolved loadout. Repeat the same AI controller on Arcade collision stress routes including the reported Dust2CyberiaV4 ramp, long cylinders, seams, walls, corners, and actor capsules; evidence shows capsule-only world contacts, no pose-driven root correction, no penetration, no unexplained velocity cancellation, smooth slide, and configured bounce on real impacts. Keep source/build/runtime/human feel evidence separate.
paths: ['src/actor/player-ai.cpp', 'src/actor/player-ai.h', 'docs/specs/npc-behavior/npc-behavior.md', 'docs/architecture/player-npc-systems/npc-movement.md', 'src/physics/movement/physics-collision-glb-sweep-slide.cpp', 'src/physics/movement/physics-collision-core.cpp', 'src/physics/movement/actor-final-pose.cpp', 'docs/workflows/runtime-scenario-validation.md']
risk: A stronger AI may raise local match difficulty and repeatedly stress shared collision/network paths. It must use ordinary legal actions, preserve authority and teammate safety, and stay bounded in local proof runs.
proof_expectation: Use a fresh executable and pinned game-DLL generation through the root dev-loop. Run actual TDM AI gameplay and inspect exact client/server events.jsonl by artifact, generation, run, actor, tick, weapon, action, contact, and damage identity. Require at least one server-accepted kill per resolved weapon and observed instances of every available movement action. Replay the human-reported collision spot and perform an unscripted AI walk/fight session on the same artifact. A command or synthetic check alone is not proof.
context_id: ctx-ebc8d9f357f9

## Agent suggestion

### 2026-11-20T16:42:45.209Z — arcade-collision-elite-proof

Run 20261120_162116 on generation 405: 1,520 AI actions all had target_id=0 and weapon=revolver, with no dash/jump/down-dash/freeze/air-strafe flags. Client roster resolved local actor 1 as TDM team 0 with 10 participants and 9 remote NPCs; server roster contained opposing team-1 actors 200004-200008 near y=124 while the AI moved to y=-126. Source caps target acquisition at 60m and discards every occluded candidate, leaving no pursuit goal. Proposed fix: pursue the nearest replicated hostile on the map, route around occlusion, and retain LOS gates for firing.

## Agent suggestion

### 2026-11-20T16:42:59.564Z — arcade-collision-elite-proof

Run 20261120_162116 on generation 405: 1,520 AI actions all had target_id=0 and weapon=revolver, with no dash/jump/down-dash/freeze/air-strafe flags. Client roster resolved local actor 1 as TDM team 0 with 10 participants and 9 remote NPCs; server roster contained opposing team-1 actors 200004-200008 near y=124 while the AI moved to y=-126. Source caps target acquisition at 60m and discards every occluded candidate, leaving no pursuit goal. Proposed fix: pursue the nearest replicated hostile on the map, route around occlusion, and retain LOS gates for firing.

## Agent discussion and suggestions

### 2026-11-20T20:46:23.880Z — arcade-collision-elite-proof

Message: `msg-853f1afcfc26` (evidence)

Subject: AI route plan ignored when target is occluded

Static review found a direct route-navigation gap in player-ai.cpp: planRoute() runs for an occluded ranged target because its condition includes !hasLineOfSight, but followRoute() is only called inside the later if (target.valid && hasLineOfSight) block. When a known hostile is hidden, the controller therefore skips the planned route and takes the no-visible-target forward fallback. This directly undermines the existing promise to navigate around cover. Refactor target pursuit and aiming separately: keep LOS-gated facing/fire, but follow the route for any valid hostile and continue rejecting teammate/unknown-team targets. I verified current source SHA matches the latest DLL snapshots, but the coordination conflict remains open, so I did not edit player-ai.cpp.

## Agent suggestion

### 2026-11-20T22:43:00.840Z — arcade-collision-elite-proof

Pinned baseline evidence: run 20261120_220344 on EXE 2026-11-20T20:20:51.788061Z and DLL gen416 logged 22,288 player.ai.navplan calls with points=0; current planRoute condition includes gRoute.empty() despite gRoutePlanTick=tick+30, so unsuccessful route search repeats every frame. Removed that condition; target change/movement and the existing 30-tick deadline still trigger replanning. The DLL request then failed disk_low (2031 MB free, 2048 MB required), so the patch is not built or runtime-proven yet. Baseline AI had 7,690 actions, 955 dash, 5,066 jump, 474 down-dash, 6,282 air-strafe, 292 actions near teammates; 15 authoritative kills were chainsaw 7, swordsword 6, deathball 1, mace 1, with no ranged weapon kills or freeze evidence. Full all-weapon acceptance remains open.

## Agent suggestion

### 2026-11-21T20:20:28.563Z — melee-proof-3weapons

Implemented and runtime-proved the first no-target exploration slice on generation 432, run 20261121_201749 (arena1, NPCs deleted): scanner shows player.ai.explore=11 (rate-limited), player.ai.fallback_spawn=0, target_id=0, dash=24, down_dash=17. Task proof task-proof-851e73217692 is proven for the player.ai.explore event. The old 24 fallback spawns and 57-NPC/log inflation are gone from this run. Still unproven: the broader f887 acceptance (every weapon kill, collision stress route, teammate coordination, full human-contract behavior).

## Scheduled independent review

### 2026-11-22T05:33:59.151Z — cpp-latency-pressure-profile

A bounded independent review was queued as wake wake-d3912edaaf57 after 250 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs_new_runtime_evidence)

### 2026-11-22T05:49:42.077Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (2026-11-22T05:3xZ). Read-only: no source edit, no build, no run.

OBSERVED FACTS
1. Readiness: issue-f8873319f12b reports ready=true, missing=[], 5 tasks, state=triaged, owner=arcade-collision-elite-proof; paths include src/actor/player-ai.cpp.
2. Proof status: only client-ai-exploration-rate-limited has a proof record, task-proof-851e73217692 (event player.ai.explore, gen432, run 20261121_201749, result=proven, verified 2026-11-21T20:20:13Z). Its recorded source hash for src/actor/player-ai.cpp is cead118584f1703f7603dd0fcd44f887dfd7429db825cae8ca738df2810fb46f; the current file hashes exactly cead1185... (HEAD 9fd637d5, no working-tree diff on player-ai.cpp). The source identity still matches.
3. Missing proof artifact: the cited journal C:\mimita-v9\logs\2026-11-21\20261121_201749\client-01\events.jsonl does not exist; that run dir now contains only run.json (4,684 B). The other two tdm-ai-exploration runs, 20261121_194824 and 20261121_201209, also contain only run.json. So the one proven slice is source-hash-current but its evidence is no longer independently inspectable.
4. Remaining tasks: ai-all-weapon-kills and ai-elite-actions have no proof record; the arcade_* proof rows on issue paths (root capsule bounce, capsule sweep/pose, dust2 ramp, actor pairs, client-ai movement, client-ai weapon kills) are all result=unproven with empty journals.
5. Latest relevant journals: a scan of logs/2026-11-22 found zero player.ai.* events in any run journal. The latest run 20261122_052434 is profile=npc-navigation on dust2cyberiav4 (server NPC AI), not the client AI controller. No new client-AI runtime evidence exists since 2026-11-21T20:17Z.
6. Read-only source inspection: HEAD player-ai.cpp contains hostile/unknown-team filtering (lines 84-111), pursuit of the nearest occluded hostile (197-207), route following when !hasLineOfSight (750/762), per-weapon equip visiting (530-588), and jump/dash/down-dash/freeze/air-strafe inputs (947-994). No uncommitted player-ai.cpp changes.

INFERENCE / UNCERTAINTY
- The earlier static-review gaps (route plan ignored when occluded, 60m target cap discarding cover targets, no weapon rotation) appear addressed in current source, but code presence is not behavior proof.
- Absence of player.ai events in 2026-11-22 journals most likely reflects profiles that do not enable the client AI controller, not a regression.
- Confidence: high that source contains these paths; low/unproven that runtime acceptance is met (no fresh pinned combat run, no retained journals).

CONTRARY EVIDENCE
- The exploration proof remains marked proven and its source hash still matches HEAD, so there is no evidence of code invalidation; the gap is artifact retention, not a demonstrated failure.
- That exploration run used --no-npcs (run.json args), so even with its journal it could not evidence teammate targeting, wall-only tracking, or per-weapon kills. It only covers no-target exploration.

NEXT DISCRIMINATING CHECK
On the next published pinned artifact (record EXE sha + game generation), start one bounded tdm-ai-exploration run with opposing NPCs present on arena1 (not --no-npcs), exercise 2-3 minutes, and inspect the pinned client+server events: require player.ai.state/action with a nonzero hostile target plus has_line_of_sight true/false split, observed dash/down-dash/jump/freeze/air-strafe flags, and kill.reward attributable to the AI actor for at least one weapon (player.ai.equip shows rotation). First divergence to locate: no opposing NPCs in roster, target_id stays 0, or no fire while the target has LOS. Capture the journal path into the proof record before retention can prune it.

VERDICT: not ready to close; one exploration slice proven and source-current, broader acceptance unproven and currently unverifiable due to missing journals.
