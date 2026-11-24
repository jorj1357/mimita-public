# Coordination decision: fcs-npc-elite-difficulty

- Decision: `decision-2c2ee150dc3a`
- Made by: `npc-elite-fcs-2`
- Created UTC: `2026-11-23T19:50:00.426Z`
- Conflict: `Older FCS guidance asked for 2-4 degree aim error and a 200ms shot floor; the latest user decision asks for nearly unbeatable elite NPCs.`
- Inputs: `-`

## Chosen approach

Start FCS elite at maximum legal difficulty: 200ms acquisition reaction, true elite aim/turn, 3-tick accepted-shot floor, memory-only information, nearly unbeatable; tune down only through explicit lower profiles.

## Rationale

User confirmed maximum legal difficulty, 200ms acquisition reaction, true elite aim/turn, 3-tick accepted-shot floor, and memory-only information. This is the newest explicit decision.

## Tradeoffs

Much higher lethality and possible player frustration; tune down only through explicit lower profiles after measured proof.

## User directives considered

- none recorded

## Rejected alternatives

- Keep 5 degree aim error
- Keep 12-tick shot floor
- Allow through-wall radar for elite CT

## Affected paths

- `config/behavior-profiles.json`
- `config/npc-difficulty.json`
- `docs/features/npc-behavior.md`

## Dissent

- none

## User override

Add a new user directive for the affected task. The decision becomes stale
automatically and the task owner records a replacement decision.

