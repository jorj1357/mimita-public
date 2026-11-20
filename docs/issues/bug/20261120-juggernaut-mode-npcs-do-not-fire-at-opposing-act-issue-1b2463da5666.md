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
