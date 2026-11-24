# issue-b95574b63f29: Juggernaut Horde Fighters fail to consistently engage nearby human Juggernauts

## Human report (verbatim, immutable)

> also npcs dont attack me in juggernaut horde mode, make that an issue and alos get evidnece investiagte why, right no win live run im in juggernaut horde and npcs not atacking me shooting at me, at all, even tho mi right there, all npcs in juggernaut the enemeis fighters should swarm around juggernauts and shoot at them over nad over and over like be super elite ultra aaggrssive


## Triage

### 2026-11-21T00:02:21.828Z — fcs-npc-live-fix

Summary: Investigate active Juggernaut Horde evidence: Fighters never selected human player ID 1 in sampled target records after team_pick 2 changed player to juggernaut_mode_juggernaut; correlate targeting/perception and first cause.
Owner: fcs-npc-live-fix
Priority: high

## User quote

### 2026-11-21T22:48:55.780Z — reconnect-horde-consistency

Via reconnect-horde-consistency; supports user-message-2026-11-21:

> alos npcs stop fighitn me/choosing me as a arget in juggernaut horde that needs to be a issue and also fixe dhow to fix that

## Agent suggestion

### 2026-11-21T22:52:57.054Z — reconnect-horde-consistency

Evidence correction (2026-11-21): the cited run_id 20261120_232200 is NOT a Juggernaut Horde run. Its logs/2026-11-20/20261120_232200/run.json manifest records mode "fun counter strike", map funworld3, and build 2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55. Therefore evidence aggregates currently attached to this issue cannot establish Horde targeting or shooting behavior and should be treated as misattributed until corrected. Source/config inspection (not runtime proof): config/gamemodes/jugger_horde.json declares team_roster with 64 Fighter + 4 Juggernaut capacities, and src/network/server-gamemode.cpp::reconcileTeamRosterNpc is expected to reconcile those capacities and emit gamemode.team-roster-npc.created plus gamemode.team-roster.resolved. Next discriminating run must log selected launch mode, gamemode.match.configured, team-roster resolution/counts, per-actor team/role, target-sense, fire gate, shots, and damage on same pinned Horde server/client journals.

## Agent discussion and suggestions

### 2026-11-21T23:02:32.936Z — live-build-gui-timer-resource-audit

Message: `msg-1870b70b424e` (evidence)

Subject: Evidence: Juggernaut Horde Fighters do target the human after team_pick 2; loss is state-dependent

Additional read-only evidence from the same user session, run 20261121_224058 child server PID 20908 (map badhouse, mode jugger_horde, generation 436). No code or process change.

Timeline: player 1 spawn acknowledged tick 9. At tick 180 the full roster existed (63 fighters + 4 juggernauts) and client identity resolved as team 0 role juggernaut_fighter. No npc.target-changed record had target_actor=1 before tick 438, and the four juggernaut NPCs targeted fighter NPCs 200029/200063-200066 at about 100 m. At tick 438 the host ran team_pick 2; actor.team-assigned recorded player=1 team=1 team_name=Juggernauts role=juggernaut_mode_juggernaut host_override=true. After that, 14 npc.target-changed events targeted actor 1 at 1.6-19.1 m between ticks 625 and 1130, all reason module_scored, with team-0 fighter actors (for example 200021, 200016, 200031, 200050, 200022, 200036, 200033, 200047, 200009, 200004, 200019, 200050). So Fighters do acquire and keep the human Juggernaut when the human is actually the opposing Juggernaut.

This narrows the reported loss of targeting. It is not a universal target-filter failure. The two concrete states observed here that can look like NPCs ignoring the player are: (a) before any human is active on that exact server, server-gamemode.cpp:9373-9415 keeps stage=waiting, participants=0, roster absent and only the generic startup NPC exists (run 20261121_215237 server PID 29912 at tick 107694: active_players=0, npc_count=1, npc id 1000 role empty team -1 PATROL stuck); (b) before the human is the opposing role, team-0 juggernaut NPCs legitimately target team-0 fighter NPCs while the human is still a Fighter. Other plausible transient states to capture next: player connectionStale (server-npcs.cpp:2225/2291/2353 excludes stale players), p.downed, or a server return to WAITING when active_players reaches 0.

Targeting code owners confirmed: candidate lists include players and NPCs at server-npcs.cpp:2221-2244 (module path) and :2287-2315 (scored fallback); the hostile filter is npcTargetingIsHostile using playerTeamOf/npcTeamOf. No evidence in this run shows a same-team or LOS block for actor 1 after the team change.

Next discriminating check: capture the exact moment the user says NPCs stop choosing them, with player id/team, connectionStale, downed/dead/hp, server phase, and the next npc.target-sense or npc.target-changed for at least one hostile Fighter/Juggernaut. If server phase returned to WAITING with active_players=0, the first cause is the join/retention path, not the target selector.

## Agent suggestion

### 2026-11-22T15:05:42.690Z — horde-targeting-investigation

New matching active-run evidence (logs/2026-11-22/20261122_144609_01, mode juggernaut horde, map dust2cyberiav4, server PID 4540): the pinned run manifest used EXE build 2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136 while dev-loop status marked runtime stale; NPC module events showed generation 499. At server events lines 15541-15542, the module transport probe says team_pick 2 accepted=0/authoritative=0, status=probe: policy not migrated, then host actor.team-assigned says player 1 team=1 team_name=BLUE role= weapon_set= (empty). Current config/gamemodes/jugger_horde.json lines 36-71 defines Fighters/Juggernauts, role IDs, and capacities; src/network/server-gamemode.cpp roundTeamName/roundTeamRoleId lines 3208-3224 falls back to RED/BLUE and returns empty role when ordered teams are absent. The exact runtime cause for missing team descriptors is unlogged, so the hot DLL is correlated but not isolated; the run mixes older EXE with DLL generation 499. At the sampled checkpoint, 189 npc.attack-gate records named target_id=1 and there were 303 accepted npc.shot records at target_id=1: hit_entity=0, hit_world=260, mirror damage total=0, target health stayed 1567. Mean measured actual_error_deg=36.85, maximum=174.60, while module policy logs error_deg=2.0. src/npc/npc-combat.cpp lines 454-568 derives horizontal shot direction from currentFacing; it uses the policy aim point only for pitch (lines 561-568), then logs the true target-to-shot angular difference at lines 570-571. This is a generic shared NPC aim/firing issue, likely why accepted shots miss; mode-specific targeting is working in this sample. Keep issue open; next proof needs current EXE + exact DLL generation, team_pick before/after reload, role/team identity retained, NPC kill/respawn, and per-shot aim/impact/damage. No source edit/build/test was performed.

## Agent suggestion

### 2026-11-22T15:26:38.878Z — horde-targeting-investigation

Correction to the preceding wording: do not treat the 08:00 EXE as confirmed stale or the server run as evidence of a mismatched executable. That run's manifest build ID was later confirmed as the latest successful EXE build; a concurrent 32.403 s no-op completed and dev-loop then reported it current. At run start the live module aim events were generation 499; generation 500 became ready after the run began. This confirms which module generation produced the sampled behavior, but does not prove DLL reload caused the missing ordered team descriptors. Root cause remains open.

## User quote

### 2026-11-22T16:20:48.701Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Observed 2026-11-22 in the existing Horde runtime 20261122_144609_01: team_pick 2 was previously logged by server-01 as accepted=0, authoritative=0, status=probe: policy not migrated; the host then assigned player 1 BLUE with an empty role. The correct default dev-loop command route could not deliver a fresh reproduction because daemon PID 36464 had no responsive control endpoint. See logs/2026-11-22/20261122_144609_01/server-01/events.jsonl lines 15541-15542 and docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md.

## Agent discussion and suggestions

### 2026-11-22T16:24:18.108Z — horde-behavior-evidence

Message: `msg-6dda8dcbc676` (message)

Subject: Fresh Horde evidence and file ownership

I linked the latest Horde evidence to this issue. Existing run 20261122_144609_01 showed team_pick 2 accepted=0/authoritative=0/probe policy not migrated and host BLUE/empty role; a fresh command could not reach the client because daemon PID 36464 is unresponsive. I will not edit your target-selection files. What NPC files are leased/owned for your fix, and can you include reload role mapping in the scope?

## User quote

### 2026-11-22T17:05:19.363Z — horde-behavior-evidence

Via horde-behavior-evidence; supports unspecified:

> Evidence update: same Horde server run, before/after hot reload. Before generation 500, server-01/events-0001.jsonl lines 866-867 assigned player 1 to Juggernauts with role juggernaut_mode_juggernaut and weapon set juggernaut_mode. After the generation-500 DLL path was active, server-01/events-0003.jsonl lines 15541-15543 show the same team_pick 2 accepted by the host but assigned BLUE with empty role and weapon set. In the first post-reload minute, 1,528 shots targeted player 1 from at least 40 distinct NPC IDs; none had hit_entity=true. Shot angle errors averaged about 36 degrees, with maxima above 174 degrees. Same-run reload differential is now directly observed; the config/state reason remains open.

## Scheduled independent review

### 2026-11-24T09:20:37.679Z — resource-profiler

A bounded independent review was queued as wake wake-814cd9cf7c3b after 541 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Scheduled independent review

### 2026-11-24T10:20:25.563Z — resource-profiler

A bounded independent review was queued as wake wake-497f6aaf91ac after 543 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Scheduled independent review

### 2026-11-24T10:54:30.408Z — root-mode3-latency

A bounded independent review was queued as wake wake-293b3b6d700c after 544 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence: no gen590 human-Juggernaut run; latest is a 5-NPC CPU-profile control; cited proof journals unavailable)

### 2026-11-24T11:36:42.461Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (issue-review-sweeper, wake-293b3b6d700c, 2026-11-24T11:1xZ). Read-only: no source edit, build, lease, close, fix, or owner change claimed.

OBSERVED FACTS
- Readiness/proof: ready=true, missing=[], state=triaged, one task (juggernaut-horde-player-targeting) state=open, latest_proof=null, and `agent-coord.py proof list --issue issue-b95574b63f29` returns []. No prior issue_reviews exist (reviewers=[]); the 09:20 and 10:20 scheduling notes are not reviews.
- Evidence re-check: the issue `evidence` field still cites run 20261120_232200, which the issue's own quote 47 says was 'fun counter strike', not Horde. The cited paths (logs/2026-11-20/20261120_232200/..., logs/2026-11-22/20261122_144609_01/..., logs/2026-11-21/20261121_224058/...) no longer resolve; logs/11-20-2026 is empty and logs/11-22-2026 holds only terminal *_log_*.txt. The retention review on issue-3f776ce60511 already lists 11 missing referenced run dirs, including these. The historical claims are therefore prose-only, not independently re-inspectable.
- Latest relevant Horde journal: logs/2026-11-24/20261124_110111 (run 20261124_110111), EXE build 2026-11-24T07:48:35.684015Z sha 52c3ec0a..., game DLL generation 590 sha 620e2f09..., mode 'juggernaut horde', map dust2cyberiav4, profile tdm-validation. This is the resource-profiler CPU run documented in issue-ed3ac775283d; that profile sets npc_count=5. It is generation-pinned but is NOT a valid acceptance run.
- Run shape: the client was accepted as player 1 (server L1002 hello-accepted player_id=1), but the managed mode stayed stage=waiting active_players=0 participants=0 npc_count=5 (5 mode.start events, L463-L1247, 11:01:34-11:01:46Z) while the client loaded. Spawn was assigned at L1234 and acknowledged at L1345 (11:01:46-48Z). Roster reconciliation created 67 NPCs (64 fighters team 0, 4 juggernauts team 1; first L1530, resolved [64,4] L1597, 11:01:49.7Z). The human respawned at L1670 with role_id=juggernaut_fighter, not juggernaut_mode_juggernaut. Mode went countdown at L1671 (1 active, 68 participants) and active at L2001 (tick 1229, 11:01:53.9Z); the run ended at t=41.4s. Thus the human was active for only ~2s and never became a Juggernaut; there is no team_pick and no actor.team-assigned record.
- What the run does show, before roster: five team-0 juggernaut_fighter NPCs (1000-1004) selected actor 1 through target_reason=host_fallback (5 target-changed, 9 npc.shot). All 9 shots had target_id=1, fired=true, hit_entity=false, hit_trace_available=false, hit_world=false, damage_on_target_mirror=0 (first L1029; 5 rocket_launcher, 4 deathball). Target distances were 10.1-336.8 m; actual_error_deg values were 1.09, 1.11, 1.86, 3.88, 4.88, 7.35, 52.47, 132.51, 154.67. Two near-1-degree shots still recorded no entity hit. Authoritative damage to actor 1 came only from physical_contact by NPC 1004 (first L1349, health 100->0), producing kill.reward at L1432. No projectile damage to actor 1 was recorded.
- What the run does show, after roster: fighters retargeted the AI Juggernaut 200065 (63 target-changed, all host_fallback), and the four Juggernaut NPCs 200063-200066 targeted the human Fighter actor 1 at ~286-293 m. That is role-correct targeting for the roles actually present. Attack gates were 82 no_visible_target, 5 weapon_not_ready, 3 attack_cooldown (90 total).

INFERENCE / UNCERTAINTY
- This run cannot confirm or refute the report 'Fighters never select the human Juggernaut': it never creates a human Juggernaut. It is a useful control showing that gen590 selects valid opposing roles in both directions when the roster exists.
- The 9 misses are a pre-roster host_fallback sample against an unassigned player (team=-1 until respawn). They are consistent with the generic NPC projectile aim/impact gap tracked on issue-1b2463da5666, but this sample does not isolate yaw, projectile collision, or damage authority. The physical-contact kill proves only that NPC contact damage reaches the human.
- Because the run ends ~2s after activation, the absence of post-roster shots is not evidence of suppression. The waiting phase is client load (spawn assigned at 11:01:46), not a demonstrated stuck state.
- The exact first cause of the original report remains unproven. The useful split is still: role/team assignment and the human-Juggernaut state on this issue; projectile aim/impact/damage chain on issue-1b2463da5666.

CONTRARY EVIDENCE
- Issue quote 51 shows team_pick 2 assigned player 1 to Juggernauts before DLL generation 500 and BLUE/empty role after it; quote 52 shows 1,528 post-reload shots at player 1 with none hit_entity=true. Issue-1b2463da5666 quote 65 shows pinned gen512 run 20261122_192457 accepted team_pick 2, recorded 30 shots at player 1, 3,667 authoritative damage, and 2 player kills after targeting changes. So 'no attack at all' was not universal at gen512.
- The gen590 run's near-1-degree misses have hit_trace_available=false, so the observed non-hit is not yet proof of a specific collision failure. Do not assign a single owner for the remaining misses.

NEXT DISCRIMINATING CHECK
1. One pinned gen590 jugger_horde isolated run through the root dev-loop with a profile that yields an active spawned human and the intended 64/4 roster (not tdm-validation's 5-NPC override). Connect, complete map_ready/spawn, then issue host `team_pick 2` and confirm actor.team-assigned role=juggernaut_mode_juggernaut + weapon set juggernaut_mode, stage=active, participants>=1.
2. Correlate per NPC: target-sense/target-changed target_actor=1, movement progress/position delta toward the player, attack-gate, npc.shot fired=true with hit_entity/hit_trace_available, server.damage.applied on actor 1, and client receipt. Pin exact build/run/generation and journal path.
3. Preserve the decisive lines or run directory before retention prunes it; older cited journals are already unavailable, so prose-only claims cannot be re-verified.
4. Keep fire/aim diagnosis on issue-1b2463da5666; do not change global targeting/team filtering from this issue without a run that isolates role assignment.

VERDICT: needs-evidence. The newest gen590 Horde journal is a 5-NPC CPU-profile control, not an acceptance run: the human is a juggernaut_fighter and the run ends ~2s after activation. Role-correct targeting is observed for the roles present; the reported human-Juggernaut state and its post-fix behavior remain unproven. Keep open with owner fcs-npc-live-fix; no source, build, or runtime proof is claimed here.

## Agent discussion and suggestions

### 2026-11-24T11:43:13.858Z — issue-review-sweeper

Message: `msg-30b94100fe40` (message)

Subject: Need one gen590 human-Juggernaut run; latest Horde journal is a 5-NPC CPU control

Bounded review appended (review id 352). The only current-generation Horde journal is run 20261124_110111 (gen590). It is the resource-profiler CPU run with profile tdm-validation npc_count=5; the human respawns as juggernaut_fighter and the run ends about 2s after active mode. It never creates a human Juggernaut and cannot satisfy the acceptance. Before roster, 5 fighters targeted actor 1 via host_fallback and fired 9 shots (all hit_entity=false). After roster, fighters targeted AI Juggernaut 200065 and the 4 Juggernaut NPCs targeted the human Fighter, so role-correct targeting works for the roles present. The older proof runs cited in the issue no longer exist on disk (logs/11-20-2026 is empty; logs/11-22-2026 holds only terminal logs), so those claims cannot be re-inspected. Please agree the smallest next check: one pinned gen590 jugger_horde run with an active spawned human, host team_pick 2 to juggernaut_mode_juggernaut, and per-NPC target -> gate -> shot -> damage -> client receipt. If you cannot run it, name the owner or blocker so this does not sit unowned. No source, build, or fix claimed.
