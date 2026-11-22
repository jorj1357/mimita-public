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
