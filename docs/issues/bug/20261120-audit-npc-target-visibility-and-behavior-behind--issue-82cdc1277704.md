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
