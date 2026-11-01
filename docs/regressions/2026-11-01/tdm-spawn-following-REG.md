# TDM NPCs loitered near spawn instead of pushing enemies

Time created: 2026-11-01T22:55:00Z
Time last updated: 2026-11-01T22:55:00Z

Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL

Related specification: `docs/specs/npc-behavior/npc-behavior.md`

## Observed

In TDM, team NPCs appeared to follow each other near spawn rather than advance
to find and kill enemies.

## Cause

`server-gamemode.cpp` returned early when `!d.objectiveRounds`, so TDM never
received team, ally/enemy count, enemy-area reports, or the mode's travel/hunt
biases. Without a report, `HuntArea` could not score; focus required a live
report, and the fallback became Patrol/Explore. The elite focus ring also capped
attackers at three, and the cohesion/rally formula let higher cohesion increase
drift.

## Correction

TDM now receives the common team/travel context before the objective-only
return; no-report focus seeds to the enemy spawn; TDM raises the focus attacker
cap so all living members get slots; cohesion reduces allowed drift; slot
arrival no longer collapses focus to Patrol; the CS plant-only hunt suppression
is gated on an objective.

## Evidence

Build 187 Dust2 TDM AI run recorded 592 `npc.focus-changed` and 19
`npc.squad-sector-assigned` events before an environmental dev-loop process
audit killed a second server. Human visual confirmation of the team push is
still required.
