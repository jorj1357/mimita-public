# issue-82cdc1277704: Audit NPC target visibility and behavior behind walls

## Human report (verbatim, immutable)

> npc controller is targeting someone that is behind a wall i shouldnt see so it needs to be like , npc controller behavior needs to ignroe things behind walls or just copy other npc behavior in the gamemode its in


## Task linked

### 2026-11-20T04:59:54.118Z — runtime-60hz

Task: npc-target-audit (step 0)

## Context

### 2026-11-20T06:25:29.413Z — work-metrics

reproduction: Run the 67-NPC performance scenario and inspect NPC target selection and pathing near walls and corners.
expected_behavior: NPCs only target actors they can perceive and can reach; they do not track through walls or remain stuck.
acceptance: Audit identifies each owner, first incorrect step, and a proof path; no unseen-target or corner-stuck cases remain in the journal.
paths: ['src/npc/npc-targeting.cpp', 'src/npc/npc-perception.cpp', 'src/npc/npc-navigator.cpp', 'src/npc/npc-movement-policy.cpp']
evidence: {'runtime': 'logs/2026-11-20/20261120_040807', 'npc_count': 67}

## User quote

### 2026-11-20T06:31:01.600Z — juggernaut-npc-stuck-shooting

Via juggernaut-npc-stuck-shooting; supports issue-82cdc1277704:

> npcs are still stuck in  walls on juggenraut  mode npcs get stuck in walls also npcs dont shot at me  or juggernuats at all

## Triage

### 2026-11-20T06:32:35.405Z — juggernaut-npc-stuck-shooting

Summary: Extend the existing NPC wall/pathing audit with the new Juggernaut-mode observation; reuse the existing npc-target-audit and share route/target evidence with issue-1b2463da5666.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent suggestion

### 2026-11-20T06:51:22.455Z — juggernaut-npc-stuck-shooting

Mode configurations are not interchangeable: juggernaut disables NPCs on every team; jugger_horde enables fighter/Juggernaut NPCs; juggernaut_duel uses team_roster policy with a solo opposing NPC. Review of recent run 20261120_064201 found it was TDM on dust2cyberiav4, including NPC stuck-episode/recovery events, so it cannot prove the Juggernaut report. Need a matching run and same-run evidence from roster assignment through target, fire gate, shot, damage acceptance, and client receipt.

## Scheduled independent review

### 2026-11-22T06:07:22.951Z — codex-networking-runtime-proof

A bounded independent review was queued as wake wake-8ea267192afc after 252 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive-needs-matching-run)

### 2026-11-22T06:22:56.004Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper), 2026-11-22T06:19Z. Read-only: no source edited, no build or test started, no fix claimed, issue not closed.

OBSERVED FACTS
- Readiness: `issue readiness issue-82cdc1277704` -> ready=false, score=6, missing=["proof_expectation","risk"]; state=triaged; 1 task (npc-target-audit).
- Proof: `proof list --issue issue-82cdc1277704` returns []. No verified or exempt proof record exists for this issue or task npc-target-audit.
- Linked artifacts: issue evidence cites runtime logs/2026-11-20/20261120_040807. That directory does not exist (recursive search of logs/ found no *040807*). The separately reviewed run logs/2026-11-20/20261120_064201 now contains only run.json; its server-01/events.jsonl is absent. Both the original evidence artifact and the TDM re-review journal are unavailable, so the audit cannot currently be reproduced from its own citations.
- Source mechanism: server-side target acquisition scores distance/vulnerability/threat only, with no line-of-sight term (src/network/server-npcs.cpp:2272-2283 scoreCandidate; fallback nearest-hostile 2345-2367; serverTargetId assigned 2376-2377). The behavior-module scorer likewise omits LOS/FOV (src/npc/npc-behavior-module.cpp:326-368, scoring 346-358; src/npc/npc-targeting.cpp:53-68). In contrast the fire gate requires both TARGET_VISIBLE and LOS_CLEAR (src/npc/npc-behavior-module.cpp:399-401).
- Live journal (read-only, still appending): logs/2026-11-22/20261122_061013_fcs-route-proof/server-01/events.jsonl, NPC behavior profile `fun_cs_rush`, 06:19Z snapshot at ~49.9MB (grew from ~17MB during this review; counts below are a snapshot, not final):
  - npc.target-sense: 1081 events, 1059 with cached_los_blocked=true; sampled events carry candidate_valid=true, target_visible=false, belief_has_target=true, and a nonzero server_target_id. The NPC holds/acquires a target while LOS is blocked.
  - npc.attack-gate: 6745 events. attack=true 46; attack=true while cached_los_blocked=true = 0; attack=false while cached_los_blocked=true = 6556. Firing is LOS-gated; blocked-LOS cases are suppressed.
  - npc.shot: fired=true for every shot sampled in the file; snapshot hit_entity=19, hit_world=43, hit neither=149. NPCs do fire and connect with visible targets.

INFERENCE / UNCERTAINTY
- The literal report ("npc controller is targeting someone that is behind a wall") is consistent with code plus journal: target-ID selection ignores LOS, so a behind-wall hostile can remain the NPC's target (score/stickiness and memory), while the separate fire gate blocks the shot. This is target retention/selection, not necessarily firing through geometry.
- The "shoots through walls" interpretation is NOT supported by the available journal: zero attack-gates fired with cached_los_blocked=true.
- One edge case to inspect, not a conclusion: exactly one attack=true event had target_visible=false (actor 100015, target 100004, state ATTACK, revolver) with cached_los_blocked=false. That is a non-visible-target attack, not a proven wall shot.
- This journal uses behavior profile fun_cs_rush, not Juggernaut (jugger_horde/juggernaut_duel); the run.json profile is npc-navigation. Prior triage already established that Juggernaut mode configs are not interchangeable. This run therefore cannot prove or refute the Juggernaut-specific "stuck in walls / don't shoot" report, and the issue's own cited runs are missing.

CONTRARY EVIDENCE
- Fire gating and shots that hit actors above contradict a simple "NPCs shoot through walls" claim for this profile.
- The missing cited artifacts and the non-Juggernaut profile contradict any claim that the issue is proven either way.

NEXT DISCRIMINATING CHECK (highest value)
- Run one pinned Juggernaut/Jugger-horde scenario with at least one opposing actor, then join per actor+tick: npc.target-changed, npc.target-sense, npc.attack-gate, npc.shot, and damage acceptance. Acceptance: (a) zero npc.shot or accepted damage while cached_los_blocked=true or target_visible=false; (b) explicitly classify behind-cover target acquisition/retention (target ID persists with LOS blocked) as intended or defect; (c) compare the same actor's target-sense/attack-gate counts against the fun_cs_rush baseline. Before the rerun, populate proof_expectation and risk so readiness passes.
