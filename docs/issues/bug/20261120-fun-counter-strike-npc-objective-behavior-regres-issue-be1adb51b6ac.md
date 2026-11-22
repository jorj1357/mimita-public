# issue-be1adb51b6ac: Fun Counter Strike NPC objective behavior regressed: CTs stuck at spawn

## Human report (verbatim, immutable)

> can u see if npc behavoor is a issue and expand it with  how the npc beahvior in fun counter strike mode is broken, it behaced good but broke, need to track regression and show that  it was good like
> 
> terrorist npcs all pick a site and rush to it instatnly, CT npcs like  split or look for either site and look for terrorists, and then once u spot a terrorist  as a CT npc, all CT npcs know wher that last spotted posisiton is, and so they keep fighitng
> 
> right now npcs CT npcs just get  stuck in the spawn conrer and dont leave it, they just dash over and over into the corner, need to get  net movmemnt up higher and figure out regrsison why and how to fix


## Triage

### 2026-11-20T23:31:04.492Z — npc-fun-counter-strike-regression

Summary: User reports recurrence of Counter-Strike NPC spawn trapping specifically in Fun Counter Strike, with CT NPCs repeatedly dashing into a spawn corner. Prior-good behavior is a user-reported baseline that aligns with the current FCS section 12 contract; live proof of the historical baseline is still missing. Investigate movement progress and coordination through a pinned FCS runtime before tuning.
Owner: npc-fun-counter-strike-regression
Priority: high

## Task linked

### 2026-11-20T23:31:20.720Z — npc-fun-counter-strike-regression

Task: fcs-npc-regression-repro (step 0)

## Context

### 2026-11-20T23:42:38.911Z — fcs-npc-live-fix

reproduction: Current FCS run 20261120_232200 client journal shows repeated movement-progress-failed episodes and NPCs dashing; correlate by actor and tick after a pinned rebuilt run.
expected_behavior: FCS NPCs preserve objective travel and only request optional rush dashes when a valid unblocked navigation intent exists and no stuck recovery owns movement. Stuck recovery moves out using its selected open direction without adding repeated dash impulses.
acceptance: Pinned post-build FCS journal shows fewer/no dash events during npc.stuck or failed navigation, while site travel and CT report response continue with positive net movement.
paths: ['src/npc/npc.cpp']
related_build: pending
related_run: 20261120_232200
evidence: {'text': 'run.json says FCS, but client journal is stale and its PID differs from manifest; use only as diagnosis evidence, not after-fix proof'}
risk: Removing recovery dashes could reduce corner escape on a route gap; retain traversal-required dash decisions made by the navigator.
proof_expectation: Post-build pinned live FCS server/client journals and visual observation correlate dash/recovery events with route state, net displacement, site rush and CT last-seen response.
context_id: ctx-6caeb230a3c8

## Assignment

### 2026-11-20T23:43:44.947Z — fcs-npc-live-fix

Owner: fcs-npc-live-fix

## User quote

### 2026-11-22T01:26:56.441Z — fun-cs-npc-objective

Via fun-cs-npc-objective; supports expected-behavior:

> can u amke it so npcs in fun counter strike mode dont run into corners and instead chase the objective and kill each other no rnning into corners, just behave like how spec  sas

## Assignment

### 2026-11-22T01:26:56.846Z — fun-cs-npc-objective

Owner: fun-cs-npc-objective

## Review (needs-evidence)

### 2026-11-22T01:30:07.472Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Observed in existing run 20261122_011031, generation 437, mode fun counter strike, map dust2cyberiav4: server-01 recorded npc.rush with objective/route state, npc.nav-plan-failed(reason=blocked), and npc.stuck-episode with wall_avoid_count as high as 21. Static trace: updateOneNpc receives wallAvoidDirection and starts NpcNavigator local correction, but the stored correction is consumed only in the branch gated by sensors.hasTarget. For objective travel without a visible target, next tick restores route steering and can repeat the same corner correction. Implemented a bounded saved-correction application in the no-visible-target wall-avoidance path; awaiting the matching build and pinned runtime. Existing run is diagnostic only because it uses the pre-edit artifact. Source path src/npc/npc.cpp.

## Review (needs-evidence)

### 2026-11-22T01:48:50.549Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 439 successfully compiled src/npc/npc.cpp and published DLL SHA-256 5b2bb40fe6535abe5d6d43fe8fb68d6ee4a21fb2b5fde52b6b72382042fa7215. Pinned FCS run 20261122_013904 loaded generation 439; server reached objective roster (19 NPCs) and objective.carried, with npc.objective-context/MoveToObjective. The server also logged npc.nav-plan-failed for T-side actors: Recast comparison failure nearest_polygon_failed_start, and positions indicate movement around a spawn obstruction; no NPC-v-NPC damage/kill event was captured before the runner marked the test failed. Client journal did contain network.mode.start with phase 4 at server_tick 30, but runner required stage=active (actual stage=phase), so readiness validation rejected the run and stopped processes. Behavior remains unproven; inspect/fix the FCS T-spawn route failure and capture sustained objective progress plus cross-team damage/kill on a healthy pinned run. Visual inspection unavailable because no native app window was returned.

## Review (needs-evidence)

### 2026-11-22T02:26:00.551Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 440 (DLL code hash c9099e1bd97c81f640df89170b2ac41dedb091aef4ba82495173221c55a1a51c) contains the objective wall-correction change from src/npc/npc.cpp. Managed run 20261122_020300_fcs-npc-objective loaded gen440 and ran to t≈1073s. Journal: logs/2026-11-22/20261122_020300_fcs-npc-objective/server-01/events.jsonl. T NPCs traveled from spawn toward site; at t≈1068 actor 100016 had site destination [-899.6,153.7,2361.3], and at t≈1068 actor 100009 at [-833.3,65.3,2342.5] still failed initial route with path_nodes=0. Two server.damage.applied events at t≈1073 show NPC 100015 (team 1) damaging a non-NPC player; no NPC-on-NPC kill or bomb plant/defuse evidence appears. Earlier gen439 isolated test terminated at readiness mismatch and is not behavioral proof. Visual inspection unavailable. Daemon 16872 became unresponsive during isolated-runtime stop; after verifying PID/checkout ownership, stopped only it, watchdog restarted daemon PID 21044, responsive status confirmed; shared server PID 26748 preserved; isolated runtime stop request accepted afterward. Behavior remains unproven.

## Review (needs-evidence)

### 2026-11-22T03:03:36.984Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Gen441 managed FCS journal 20261122_023327_fcs-proof ran to t≈532s and recorded five NPC-on-NPC kills, but no objective.planted event. It recorded seven objective.carried events and one objective.dropped; actor 100017 (T carrier) remained near [-614,90] while non-carrier 100009 reached site A near t=80, then repeated rounds restarted. Fifteen npc.stuck-recovery records showed the baseline open-direction strategy sometimes pointed away from site (e.g. actor 100010 at t=716, dot≈-0.67). Gen443 short run 20261122_025700_fcs-objective-progress loaded generation 443 and recorded objective dashes with nav_path_active=false despite the earlier guard (actors 100013, 100011, 100004 at t≈68.96); tracing source shows module/traversal can set dash before that guard. Updated route-biased recovery, then moved the no-enemy safety gate after module/traversal requests. Attempt game-dll-20261122T030300829638Z-7fb0c2 is building; runtime proof for the latest source is pending.

## Review (needs-evidence)

### 2026-11-22T03:16:48.230Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Evidence review for ctx-c00ad9484fa1.

A. NPC-on-NPC kills exist on the latest published generation 443.
Run 20261122_025700_fcs-objective-progress (runtime fcs-objective-progress, launch mode 45 fun counter strike, map dust2cyberiav4, EXE build 2026-11-22T02:37:06.972018Z, game DLL build/game/generation-443/game-g443.dll, started 2026-11-22T02:57:00.283Z, stopped 2026-11-22T03:02:33Z).
Journal: logs/2026-11-22/20261122_025700_fcs-objective-progress/server-01/events.jsonl.
- 4 authoritative server.damage.applied NPC-to-NPC kills with killed=true: 2 team0-to-team1 and 2 team1-to-team0. First: attacker 100008 (npc team 0) killed target 100009 (npc team 1), source physical_contact, damage 22, health 7 to 0, t=108.436, tick 5042.
- 7 kill.reward events with victim_is_npc=true. Example killer 100008 NPC to victim 100018 NPC, t=216.527, tick 10678.
- Client journal logs/2026-11-22/20261122_025700_fcs-objective-progress/client-01/events.jsonl has network.mode.start mode=funcounterstrike phase=4 participant_count=20 at t=27.154.
- T-side (team 1) site choices A/B and npc.rush MoveToObjective from spawn toward site A are present.

B. Bomb plant exists on the previous published generation 441.
Run 20261122_023327_fcs-proof, DLL build/game/generation-441/game-g441.dll, journal logs/2026-11-22/20261122_023327_fcs-proof/server-01/events.jsonl.
- objective.carried for carrier 100018 (team 1) then objective.planted at t=882.927, tick 50265, operator 100018 NPC, site B, position [-903.267,-102.953,2353.251], plant_seconds=5, explosion_seconds=40.
- The same run also has 8 kill.reward victim_is_npc=true in the retained window.

Divergence and uncertainty:
- No single run on the latest source contains both plant and kills. The gen443 runtime was stopped after about 5.5 minutes with 4 objective.carried and 0 objective.planted, before a plant could occur.
- CT net travel away from spawn is NOT established. CT (team 0) deaths cluster at x -839..-870, y 113..135, z 2343..2362, within about 40 units of the ct_spawn (-873..-882, 118..125, 2349). 672 team0 npc.stuck-episode records were observed. T rushing to site A is clear. The original CT-spawn-trap symptom is therefore not disproven by this data.
- Confidence: high for NPC-on-NPC kills on gen443; high for a bomb plant on gen441; low that the CT spawn trap is fixed.

Next: one longer pinned run on the newest published generation, same FCS/map, kept alive long enough to capture objective.planted plus cross-team kills, then compare CT displacement from ct_spawn.

## Review (evidence)

### 2026-11-22T03:26:52.065Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper). Run 20261122_023327_fcs-proof, launch_mode 45 fun counter strike, map dust2cyberiav4, game DLL generation 441, sha256 c469fa8223ee9b2d3be143e52f84b702a7601fbc1dd2c2977b6e203aa3aa3cd2. Server journal: logs/2026-11-22/20261122_023327_fcs-proof/server-01/events.jsonl. This run contains the exact behavior requested in ctx-c00ad9484fa1 (NPC-on-NPC kills and a bomb plant) and corrects the 03:03 review's 'no objective.planted' snapshot, which was taken before the plant occurred: t=871.552 objective.plant-start operator=100018 operator_kind=npc site=B mode=hold; t=882.928 objective.planted operator=100018 site=B explosion_seconds=40; t=929.200 objective.exploded site=B; t=929.201 round.result round=9 winner_team=1 reason=2 wins_ct=8 wins_t=1 (bomb detonation decides the round). NPC-on-NPC combat: 22 kill.reward events have killer_is_npc=true and victim_is_npc=true, and all 22 are cross-team (team 0 vs team 1), e.g. 100005(team0)-chainsaw->100018(team1) and 100007(team0)-swordsword->100012(team1); plus 32 npc.death and 542 server.damage.applied. CT NPC 100005 issued objective.defuse-start three times (operator_kind=npc) but was interrupted. Team map: planter 100018 and carrier 100012 are team 1 (T); killers 100005/100007 are team 0 (CT); objective.carried carrier=100012 allowed_team=1. Recorded as issue evidence evidence-8572793982d1 with required events objective.planted, objective.exploded, kill.reward, round.result, server.damage.applied; result=proven. Caveat: the journal was compacted/rotated, so the retained window starts near t=849 but still holds the full plant chain. This proves generation 441; source has since advanced (generation 443 observed and generation 444 build game-dll-20261122T030300829638Z-7fb0c2 was in progress at review time), so a fresh pinned run on the current generation is still needed before claiming the latest build. Human visual review not performed. Confidence in the gen441 events: high.

## Agent discussion and suggestions

### 2026-11-22T03:26:52.687Z — build-wait-reviewer

Message: `msg-5c5b2cce1acf` (note)

Subject: Correction: gen441 journal DID record objective.planted; gen443 had 4 cross-team NPC kills

Evidence from your earlier pinned runs, read after your review 98: (1) gen441 run 20261122_023327_fcs-proof server journal now contains objective.planted at t=882.927 tick=50265, operator 100018 NPC, site B, position [-903.267,-102.953,2353.251], after objective.carried; it likely occurred after you read the file at t approx 532. (2) gen443 run 20261122_025700_fcs-objective-progress recorded 4 authoritative NPC-to-NPC killed=true server.damage.applied events (2 team0-to-team1, 2 team1-to-team0) and 7 kill.reward victim_is_npc=true. (3) Warning: the gen441 server journal is a rolling window now starting at seq 233590, so earlier kill lines are gone; copy exact lines or record proof evidence promptly. My independent review is issue review 99. I am not editing code or starting a duplicate runtime; your 7fb0c2 build and long pinned run can own the final proof. Suggest keeping the newest-gen FCS runtime alive past t=880s so one journal shows plant plus cross-team kills, and report CT displacement from ct_spawn, since gen443 CT deaths clustered within about 40 units of ct_spawn.

## Agent discussion and suggestions

### 2026-11-22T03:30:39.965Z — issue-review-sweeper

Message: `msg-e2fec3efd744` (message)

Subject: Gen441 20261122_023327_fcs-proof has plant + 22 cross-team NPC kills (corrects 03:03 snapshot)

Your 03:03 review said run 20261122_023327_fcs-proof had no objective.planted; that was a mid-run snapshot. The same gen441 journal now contains the full decisive chain: t=871.552 objective.plant-start operator=100018 operator_kind=npc site=B; t=882.928 objective.planted site=B explosion_seconds=40; t=929.200 objective.exploded; t=929.201 round.result round=9 winner_team=1 reason=2 wins_ct=8 wins_t=1. Also 22 kill.reward events with killer_is_npc=true and victim_is_npc=true, all 22 cross-team (e.g. 100005 team0 chainsaw to 100018 team1, 100007 team0 swordsword to 100012 team1), 32 npc.death, 542 server.damage.applied, and CT NPC 100005 issued objective.defuse-start 3x. I recorded issue evidence evidence-8572793982d1 (result=proven, gen441 DLL sha256 c469fa82...). Caveat: journal was compacted so retained window starts near t=849 but still holds the plant chain; source has moved on (generation 446 build attempt in progress), so a longer pinned run on the current published generation is still needed to prove the latest source and the CT-displacement part of the original acceptance. I did not edit code. Please reuse this gen441 chain and keep the next run alive long enough to capture a plant plus cross-team kills.

## Review (evidence)

### 2026-11-22T03:48:51.718Z — cpp-latency-pressure-profile

Reviewer: cpp-latency-pressure-profile

Independent evidence review for ctx-c00ad9484fa1 (agent cpp-latency-pressure-profile; read-only: no source edit, no build request, no runtime started by me; the active runtime owned by fun-cs-npc-objective was preserved).

Run identity: runtime fcs-route-proof, run 20261122_032407_fcs-route-proof, launch mode 45 fun counter strike, map dust2cyberiav4, EXE build 2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344, game DLL generation 444 sha256 fa4d17a590241ac22bb8d0ab2e1b41ebb3bb895376df30639ab4b0af5f542390, server PID 35960 alive at observation. Server journal: logs/2026-11-22/20261122_032407_fcs-route-proof/server-01/events.jsonl.

NPC-on-NPC combat: the retained server window contains 134 authoritative server.damage.applied events with attacker_type=npc, target_actor_type=npc and killed=true, and every one is cross-team (team 0 vs team 1). Example tick 19938 t=2026-11-22T03:37:25.064Z attacker 100003 (team 0) killed 100013 (team 1), physical_contact, 22 damage, health 22 to 0. NPC-vs-NPC kill.reward events include ranged weapons (shotgun, revolver) and melee (chainsaw, swordsword): e.g. tick 4514 100003 shotgun to 100014, tick 5145 100002 revolver to 100009. Client journal corroborates with client.npc.death.received shooter_id=100017 (team 1) victim npc 100005 at position [-600.7,40.2,2361.1].

CT spawn-trap check (new data, gen444): between run start 03:24:07Z and about 03:30Z, all ten CT NPCs (team 0) whose positions were sampled started at ct_spawn around [-875,122,2349] and ended in a cluster at [-600..-637, 1..68, 2360], i.e. 298-319 units from ct_spawn on the T side. Examples: actor 100006 first [-880.7,118.8,2343.1] to last [-609.5,29.0,2360.3], peak 317.2u; actor 100003 first [-880.9,124.8,2343.1] to last [-614.5,3.3,2361.1], peak 309.0u. This contradicts a present CT spawn-corner trap on gen444 and addresses the gen443 CT-death-cluster concern in the 03:16 review. Caveat: the server journal is a rolling/compacted window (the retained window now starts near tick 17535), so those first-position lines are no longer retained; the observation is from this same journal at about 03:30Z.

Objective: the gen444 retained window has objective.carried (carrier 100012, team 1), objective.dropped, and objective.pickup (100018, team 1) but no objective.plant-start or objective.planted; the carrier keeps dying before planting. The full NPC plant chain is already proven on gen441 in evidence-8572793982d1: t=871.552 objective.plant-start operator 100018 npc site B, t=882.928 objective.planted site B, t=929.200 objective.exploded site B, t=929.201 round.result round 9 winner team 1.

Verdict: evidence. Confidence is high for NPC-on-NPC kills and CT map traversal on gen444; the NPC bomb plant is proven on gen441 but not yet observed on gen444. Remaining work: one run on the newest published generation (446 was still building at observation) whose retained journal window holds objective.planted plus cross-team kills together, captured promptly because of journal rotation. Human visual review was not performed.

## Review (proven)

### 2026-11-22T03:59:53.352Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Follow-up proof from a new pinned run on the latest published generation at start time.

Run 20261122_033318_fcs-proof-bwr (runtime fcs-proof-bwr, launch mode 45 fun counter strike, map dust2cyberiav4, EXE build 2026-11-22T02:37:06.972018Z, game DLL build/game/generation-446/game-g446.dll, started 2026-11-22T03:33:18.193Z).
Server journal: logs/2026-11-22/20261122_033318_fcs-proof-bwr/server-01/events.jsonl
Client journal: logs/2026-11-22/20261122_033318_fcs-proof-bwr/client-01/events.jsonl

One run contains both requested behaviors:
- Bomb chain: objective.plant-start t=918.165 tick=27364 operator 100016 (operator_kind npc) site B, required 5.0s; objective.planted t=925.149 tick=27663 operator 100016 site B position [-910.795,-90.718,2353.122] explosion_seconds=40; objective.exploded t=1020.755 tick=30063 site B; round.result t=1020.755 round=5 winner_team=1 reason=2 wins_ct=4 wins_t=1, so the bomb detonation ended the round. A second objective.plant-start for site A at t=1033.624 tick=30386 operator 100010 (npc) shows repeated rounds.
- NPC-on-NPC combat: 208 authoritative server.damage.applied events with attacker and target both npc and killed=true in the retained window, split 155 team0-to-team1 and 53 team1-to-team0. Example t=902.443 tick=26688 attacker 100004 (team 0 npc) killed target 100011 (team 1 npc), source physical_contact, damage 12, health 5 to 0.
- 214 kill.reward events with victim_is_npc=true in the retained window.
- Client network.mode.start mode=funcounterstrike phase=6 server_tick=30063 participant_count=20 at t=978.715.

Confidence: high that FCS NPCs kill each other cross-team and plant and detonate the bomb in-game on generation 446. Recorded proof evidence task-proof-d08c8ddf3821 (kill.reward) and task-proof-4380993cf802 (objective.planted). Caveats: no human visual acceptance; the server journal is a rolling window; source advanced to generation 449 (revision 6568d050) while this run executed, so the newest source still needs its own pinned check. The earlier gen443/generation-441 evidence remains valid but is superseded for the latest-source claim.

## Agent discussion and suggestions

### 2026-11-22T04:01:52.613Z — build-wait-reviewer

Message: `msg-c7fe562c431a` (note)

Subject: Gen446 pinned run proves NPC-on-NPC kills plus bomb plant/explosion/round result

Gen446 proof captured in one pinned run: 20261122_033318_fcs-proof-bwr on game DLL generation-446. Server journal logs/2026-11-22/20261122_033318_fcs-proof-bwr/server-01/events.jsonl has the full chain objective.plant-start t=918.165 operator 100016 npc site B, objective.planted t=925.149 site B, objective.exploded t=1020.755, round.result round=5 winner_team=1 reason=2; plus 208 authoritative NPC-to-NPC killed=true server.damage.applied events (155 team0-to-team1, 53 team1-to-team0) and 214 kill.reward victim_is_npc=true. I recorded task-proof-d08c8ddf3821 and task-proof-4380993cf802 and posted issue review with verdict proven, noting no human visual acceptance and that source advanced to generation 449 while the run executed. I am stopping my isolated runtime now to free resources; the shared server PID 26748 is untouched. If you need the newest-source proof, pin your next long FCS run to the post-449 generation and keep it alive past t=925s for the plant chain.

## Review (fixed)

### 2026-11-22T04:37:32.093Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 454 (run 20261122_042716_fcs-route-proof; build 2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344) ran the Fun Counter Strike mode with an attached client for 560.4 simulated seconds. Its server journals recorded two completed NPC bomb plants (A at 195.4s, B at 555.1s), 67 NPC-on-NPC kill.reward events, and 320/320 applied npc.nav-objective-fallback events. Runtime proof recorded as evidence-1a8a4ccfbbf6; objective and NPC combat proof tasks verified. This demonstrates objective completion and cross-team NPC combat on the updated DLL. Remaining limit: the logs do not quantify corner contact directly; fallback events indicate failed local plans and applied movement recovery.

## Review (needs-evidence)

### 2026-11-22T06:10:01.148Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Gen462 run 20261122_055416_fcs-route-proof (EXE build 2026-11-22T02:37:06.972018Z, DLL generation 462) reached sim t=620 with four cross-team NPC-v-NPC kill.reward events and 18 npc.team-report events, but no objective.plant-start/objective.planted. Counterexample: bomb carrier 100011 repeatedly chose MoveToObjective but settled around [-703,139], ~150 units from its objective, by t=570. Gen463 run 20261122_060506_fcs-route-proof showed the two-contact quick breakout at t=37.3 for actor 100009 with no npc.stuck-recovery preceding it; position moved ~49 units east by t=43.7. But actor 100018 exited a first corner at t=48.2 and met another wall at t=116.3; it repeated identical recovery position [-731.87,-19.46] at t=122.05 and 124.72 with one horizontal static normal, proving the two-contact condition was too narrow. I changed the threshold to include persistent single-wall contact and am validating a new pinned generation. No behavior completion claim yet; plant and fast single-wall breakout still need runtime evidence.

## Review (needs-evidence)

### 2026-11-22T06:21:47.883Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 464 run 20261122_061013_fcs-route-proof ran FCS on dust2cyberiav4 to sim t=600 (EXE build c94488e6, DLL generation 464). It recorded 7 cross-team NPC-v-NPC kill.reward events, 49 npc.team-report events, and sustained T MoveToObjective / CT DefendSite decisions. It also recorded 283 area escapes (276 static contact normal based) and 137 short stuck recoveries; close repeated same-position pairs were limited (e.g. actor 100008 at [-887,113] t=133.5/134.3/136.2 then static-contact breakout t=140), but corner loop symptoms remain. Objective acceptance failed: rounds 1-4 ended reason=3 (timeout), winner_team=CT, at t=147.2,272.5,392.0,512.9; no objective.plant-start/planted/exploded occurred. Round carriers were assigned (100010/100014,100012,100016,100017,100010) and repeatedly moved toward but remained far from the chosen site. Investigation found npc.cpp failed-plan fallback overrode profile movement weights with openDistanceBias=1, forwardBias=8, targetProgressBias=20, favoring narrow direct-to-goal progress. I removed those three overrides to reuse the existing MovementCommitmentSettings. The source change is under build attempt game-dll-20261122T062119640904Z-8db74c; no completion claim until a pinned run plants.

## Review (needs-evidence)

### 2026-11-22T06:34:26.819Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 466 run 20261122_063133_fcs-route-proof was stopped at sim t=113 after exposing an over-sensitive active escape replan. It logged 318 npc.area-escape events by t=99, 294 marked replanned_from_blocked_area_escape, with some actors switching between near-identical wall positions about every 1-2 seconds; only one npc.stuck-recovery had appeared by t=49. This is stable movement progress but excessive heading churn. I changed the replan predicate to require the normalized sum of current static contact normals to oppose the active breakout heading by dot < -0.5. The next generation should verify reduced replan rate and retain the successful gen465 objective and NPC kill behavior.

## Review (runtime-proven)

### 2026-11-22T07:01:47.653Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Latest source was published as DLL generation 470 and exercised in pinned FCS runtime 20261122_065419_fcs-route-proof-final (EXE build c94488e6; map dust2cyberiav4; 301.9 sim seconds; server journal logs/2026-11-22/20261122_065419_fcs-route-proof-final/server-01/events.jsonl). Runtime events: 118 npc.nav-objective-fallback and 134 npc.objective-context; carrier NPC 100009 had objective_known=true, objective_position site A [-902.107,149.336,2361.312], distance 5.113 at t=23.7. Three kill.reward events have killer_is_npc=true and victim_is_npc=true: NPC-100002 killed NPC-100015 at t=116.0 and NPC-100018 at t=120.1; NPC-100018 killed NPC-100008 at t=151.4. Area recovery logged 161 npc.area-escape events and zero same-actor pairs less than 8 horizontal units apart within 2 sim seconds across the run; the gen468 predecessor had thousands under the same threshold. The gen469 trial also had zero such pairs. This supports stable corner-breakout heading and cross-team combat/objective pursuit on latest source. No objective.planted event occurred in this gen470 run (two rounds ended by reason 3), so planting is not claimed for gen470; gen468 did plant/explode but predates the stable-hold patch. No human visual acceptance was performed. The task proof task-proof-4457be9b824c and issue evidence evidence-4fe6ec958a14 both verify against this journal. Residual: numerous generic npc.stuck-episode/nav-plan-failed events remain, so this is evidence of reduced area-escape ping-pong, not proof that every route/corner is now perfect.

## Review (counterexample)

### 2026-11-22T07:17:15.483Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Counterexample to treating the gen470 escape-event repeat metric as complete corner proof: in run 20261122_065419_fcs-route-proof-final (DLL generation 470, FCS on dust2cyberiav4), T carrier NPC 100011 had utility_goal=MoveToObjective and moved from authored T spawn about [-600,30] at t=26 to about [-800,135] by t=100, then remained in a roughly 10-15 unit patch around [-804,135] through t=145 while the site A target remained [-902,149]. Server events include repeated npc.movement-progress-failed on this actor at t=110.36,112.53,114.48,117.89,121.37 and repeated stuck-episode; round 1 timed out without a plant. The 8-unit/2s repeat check counted only npc.area-escape event positions and missed this longer local stall. Other evidence from the same run is positive: CT NPC max displacement from authored spawn was 69-157 units before round end, there were 18 npc.team-report events including visible enemy reports, and 3 cross-team NPC kill.reward events. Hypothesis: updateAreaEscape resets its 4s timer whenever position drifts more than its 5-unit radius, even when movement commitment has repeatedly failed to make goal progress, so a local loop can evade trap recovery. Next change uses the existing commitmentProgressFailed signal at the same navigator owner, then reruns the pinned scenario with goal distance/position and progress-failure windows.

## Review (runtime_proven)

### 2026-11-22T08:23:00.518Z — fun-cs-npc-objective

Reviewer: fun-cs-npc-objective

Generation 478 run 20261122_081330_fcs-route-proof-applied-fallback is pinned to game-g478.dll. Its server journal records an NPC carrier starting and completing an A-site plant (objective.planted at 296.69s), the bomb explosion and Terrorist round win at 397.10s, eight NPC-on-NPC kills, and 48 visible team sighting reports (25 CT). All 1,072 executed npc.dash events have stuck_episode_active=false and nav_plan_failed=false. CT NPCs had positive first-to-last net displacement in this match; sampled maxima exceeded 100 units. The actual movement fallback direction is now applied when nav.valid is false. Runtime task proof task-proof-19abbe522e81 and issue evidence evidence-7d99aad1439a verified.

## Resolution

### 2026-11-22T08:23:07.649Z — fun-cs-npc-objective

Fixed FCS objective fallback steering, refreshed failed routes during recovery, and suppressed dashes while recovery owns movement. Generation 478 live runtime proof shows an NPC A-site plant, bomb explosion and Terrorist round win, eight NPC-on-NPC kills, CT sighting reports and positive movement; all logged dashes had no stuck or failed-navigation state.

Evidence: evidence-7d99aad1439a (integration, proven)
