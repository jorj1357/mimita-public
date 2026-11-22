# issue-1b2463da5666: Juggernaut mode NPCs do not fire at opposing actors

## Human report (verbatim, immutable)

> npcs are still stuck in  walls on juggenraut  mode npcs get stuck in walls also npcs dont shot at me  or juggernuats at all


## Triage

### 2026-11-20T06:32:34.124Z — juggernaut-npc-stuck-shooting

Summary: Trace why Juggernaut mode NPCs fail to fire at opposing actors; correlate team/role assignment, perception/targeting, aim/line of sight, attack gates, loadout/ammo/cooldown, fire events, authoritative validation/damage, and client receipt. Wall/pathing part is cross-referenced to issue-82cdc1277704.
Owner: juggernaut-npc-stuck-shooting
Priority: high

## Agent discussion and suggestions

### 2026-11-20T06:33:00.573Z — juggernaut-npc-stuck-shooting

Message: `msg-3c28b8cd6eab` (ask)

Subject: Review Juggernaut NPC wall/pathing and no-fire reports

Please add an evidence-backed review/suggestion on issue-1b2463da5666, coordinating with the related wall/pathing audit issue-82cdc1277704. Trace likely first failing stage through mode/team roster, target selection, navigation, attack intent, legal weapon fire, authoritative hit/damage, and journal receipt. Novel ideas are welcome, but identify an existing owner/path and a discriminating runtime signal before proposing new systems. Review only; do not edit source.

## Agent suggestion

### 2026-11-20T06:45:15.734Z — juggernaut-npc-stuck-shooting

Static inspection finds mode-dependent roster behavior: config/gamemodes/juggernaut.json has players=false and npcs=false on both teams, while jugger_horde.json has Fighter players/npcs enabled and Juggernaut NPCs enabled. The horde spec requires tracing roster/team assignment -> target selection -> attack gate -> firing -> confirmed damage -> client receipt. Existing bounded owner events include npc.target-changed, npc.fire-blocked (first gate reason), and npc.shot (accepted/fire/hit). The exact mode ID and per-run server/client journals are required before concluding the NPC combat policy is broken.

## Agent suggestion

### 2026-11-20T06:51:23.752Z — juggernaut-npc-stuck-shooting

Trace server roster configuration and role/team first: base juggernaut disables NPCs; Juggernaut Horde enables NPCs, while Juggernaut Duel uses team_roster with one solo opponent per its spec. Then correlate same-run npc.target-changed, npc.fire-blocked reason, npc.shot, accepted damage, and client receipt.

## Scheduled independent review

### 2026-11-22T11:17:00.771Z — cpp-latency-linker-investigation

A bounded independent review was queued as wake wake-813d078139d6 after 278 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Context

### 2026-11-22T11:37:58.036Z — issue-review-sweeper

reproduction: Open Juggernaut Horde (jugger_horde) on dust2cyberiav4, join as a Fighter, and keep at least one opposing Juggernaut NPC alive. Observe whether Fighter NPCs and Juggernaut NPCs target, fire at, and damage the opposing team. The only matching run found, 20261121_011815, retains run.json but no server or client events.jsonl, so the fire chain was never captured. Record the exact mode id: base juggernaut configures no NPCs on either team.
expected_behavior: Per docs/specs/gamemodes/juggernaut-horde.md:39, Fighter NPCs target only Juggernaut actors and Juggernaut NPCs target only Fighter actors with friendly fire off; a perceivable opposing actor should flow roster/team assignment -> target selection -> attack gate -> firing -> server-confirmed damage -> client receipt.
acceptance: A pinned jugger_horde run with both journals shows, per NPC actor: match.actor.identity-assigned or npc.roster-profile-resolved, npc.target-changed or npc.target-sense, npc.attack-gate, npc.shot fired=true, server.damage.applied on an opposing actor, and client receipt; zero fire or damage while cached_los_blocked=true or target_visible=false. If the reported mode is base juggernaut, record whether the configured zero-NPC roster is intended or a defect.
paths: ['config/gamemodes/juggernaut.json', 'config/gamemodes/jugger_horde.json', 'config/gamemodes/juggernaut_duel.json', 'config/roles.json', 'src/network/server-gamemode.cpp', 'src/network/server-npcs.cpp', 'src/npc/npc-combat.cpp', 'src/npc/npc.cpp']
related_run: 20261121_011815
evidence: {'text': 'Observed 2026-11-22: run 20261121_011815 (jugger_horde, dust2cyberiav4, generation 417, status stop_partial) retains run.json only, with no server-01 or client-01 events.jsonl. Config: base juggernaut teams have players=false and npcs=false with npc_spawn max_count 0; jugger_horde has fighters players=true npcs=true, juggernauts npcs=true, npc_spawn policy team_roster max_count 67. Prior review is static config and roster only. Proof records for this issue: 0.'}
risk: Mode names are not interchangeable: base juggernaut has both teams players=false npcs=false and npc_spawn max 0, while jugger_horde enables NPCs with team_roster max 67. A base-mode run cannot prove the horde chain and vice versa. Shared NPC targeting/fire changes affect all modes, so scope any fix and prove with the exact-mode journal.
proof_expectation: Pinned dev-loop jugger_horde server and client journals from one build/generation/run with at least one human Fighter, showing the full roster->target->attack-gate->shot->damage->client chain and no LOS-blocked fire; separate human visual acceptance. A build, config file, or roster event alone is not proof.

## Review (inconclusive-needs-matching-run)

### 2026-11-22T11:38:46.209Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Reviewer: issue-review-sweeper. Bounded independent review 2026-11-22T11:3xZ. Read-only: no source edited, no build or run started, no fix claimed, issue not closed.

OBSERVED FACTS
- Readiness was score 1/8 (missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk). I added those fields from the existing quote, task, spec, and artifacts; readiness now reports score 8/8, ready=true, all prior text preserved.
- Proof: proof list --issue issue-1b2463da5666 returns []; task juggernaut-npc-fire-trace has zero task_proof records. Nothing is runtime-proven.
- Linked artifacts: issue-82cdc1277704 covers the wall/pathing half and already records that the prior TDM run cannot prove Juggernaut; its independent review found the original evidence run missing.
- Matching run found: logs/2026-11-21/20261121_011815 (jugger_horde, dust2cyberiav4, generation 417, status stop_partial). The directory contains only run.json; a search of logs and .dev finds no other reference, and there are no server-01 or client-01 events.jsonl. So the only Juggernaut-horde run captured no runtime chain.
- Mode/config facts: config/gamemodes/juggernaut.json sets both teams players=false npcs=false and npc_spawn max_count 0. config/gamemodes/jugger_horde.json sets fighters players=true npcs=true, juggernauts npcs=true, npc_spawn policy team_roster max_count 67. config/gamemodes/juggernaut_duel.json also has both teams npcs=false and npc_targeting disabled.
- Source ownership: src/network/server-gamemode.cpp:4919-5191 reconciles the team roster and creates NPCs per team capacity with role-specific weapon sets; src/npc/npc-combat.cpp:83-114 emits npc.fire-blocked with a reason and :132-161 emits npc.shot; src/npc/npc.cpp:1433 emits npc.attack-gate. Roles juggernaut_fighter and juggernaut_mode_juggernaut each declare a weapon_set in config/roles.json.

INFERENCE / UNCERTAINTY
- The report names no mode id. If the user meant base Juggernaut, current config spawns no NPCs on either team, so NPCs neither fire nor get stuck; the wall-stuck wording implies NPCs existed, which points to jugger_horde. This is unproven either way.
- The horde attempt proves only that a run manifest was written, not that NPCs spawned, received loadouts, acquired targets, passed the attack gate, fired, or dealt damage. No first failing stage can be named from available journals.

CONTRARY EVIDENCE
- Static review previously found the roster and fire-gate diagnostics intact, and the horde config enables NPCs and opposite-team targeting; this contradicts a blanket claim that the mode has no NPC path.
- The same-run requirement in juggernaut-horde.md:39 means neither a build, a config file, nor a roster event can prove or refute the report.

NEXT DISCRIMINATING CHECK (highest value)
Run one pinned jugger_horde scenario through the root dev-loop with at least one human Fighter, confirm both server and client journals exist, then join per actor+tick: match.actor.identity-assigned or npc.roster-profile-resolved -> npc.target-changed or npc.target-sense -> npc.attack-gate -> npc.shot fired=true -> server.damage.applied on an opposing actor -> client receipt. Acceptance: zero fire or damage while cached_los_blocked=true or target_visible=false; classify the first missing stage; if base juggernaut was the reported mode, record whether zero configured NPCs is intended or a defect. Readiness fields are now populated so this run can begin.

## Agent suggestion

### 2026-11-22T15:05:48.829Z — horde-targeting-investigation

New live Horde-fire evidence from logs/2026-11-22/20261122_144609_01/server-01/events.jsonl: in one sampled window there were 303 accepted npc.shot records with target_id=1, but 0 hit_entity, 260 hit_world, 0 damage_on_target_mirror, and player target health remained 1567. Mean actual_error_deg=36.85/max=174.60 despite module npc.aim-policy.module error_deg=2.0. In src/npc/npc-combat.cpp:454-568 the fired horizontal aim vector follows npc.currentFacing; the selected module aim point influences pitch but does not set yaw (561-568), and angleDiff records actual target-to-shot mismatch (570-571). This points to generic shared NPC aim direction as first demonstrated miss stage in this sample, while bullets/projectile-contact pass-through still needs separate confirmation: hitscan traces only real body-part boxes in src/combat/weapon-fire-raycast.cpp:453-465, and projectile shots need the later server-projectile sweep/damage events rather than npc.shot.hit_entity. Team-role state is separately wrong after this run's team_pick: host logged BLUE with empty role at lines 15541-15542; related to issue-b95574b63f29. Please preserve this run as evidence, keep issue open, and pin a current-EXE/gen run with full shot -> impact -> damage chain.
