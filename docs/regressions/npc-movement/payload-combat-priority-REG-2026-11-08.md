# Payload NPCs sometimes ignore a human near the cart

- Recorded: 2026-11-08 (human report; exact time not supplied)
- Status: REPORTED / NOT YET REPRODUCED
- Mode and map: Payload on FunWorld3

The human reports that NPCs move to the cart but sometimes do not attack until
after the human kills one of them. The report has not yet been reproduced with
a target-correlated attack decision and the human's actual position/visibility.

The 2026-11-08 `20261108_141222` live run did record 27 accepted NPC shots at
player 1. Its sampled target-sense records were mostly not visible and the
attack-gate record did not identify a target or complete suppression reason.
This proves that attacks sometimes occur, not why the reported failure occurs.

Source inspection found that `src/network/server-gamemode.cpp` gave every
Payload NPC the cart destination and skipped per-NPC formation slots. The
Payload utility objective bias could also beat combat scoring, but that has
not been established as the cause of a particular missed attack.

The current source attempt adds hot Payload role destinations and immediate
visible-threat priority, plus actor/target-correlated attack-gate diagnostics.
Status remains reported until a fresh EXE and live human scenario show the
target, LOS, goal, role, attack decision, and outcome on the same ticks.

Live source attempt, 2026-11-08: build
`2026-11-08T15:28:28.157458Z` ran on FunWorld3 in
`logs/2026-11-08/20261108_152945`. Server PID 17964 activated
`npc-behavior` generation 14. Sampled `npc.payload-role` records show six
distinct pusher positions with cap 6, nearby attacker positions, and a
defender contest position. `npc.attack-gate` now reports target ID and
suppression reason. Most sampled non-attacks had no visible target; near
player-1 samples also alternated between valid LOS and blocked LOS. This
does not recreate the human's exact ignored-while-visible case. The tested
human-pusher count stayed zero; a follow-up source edit aligned it with the
authoritative team resolver and requires a new build and live check.

Acceptance: attackers within the configured push cap enter the interaction
box; humans in the box count toward the cap; remaining attackers and defenders
hold distinct nearby positions; visible legal threats interrupt movement;
cover prevents fire; hidden targets beyond 25 m from the cart release pursuit;
the assigned objective position resumes after target invalidation.

Lane source pass, 2026-11-08: the NPC hot module now owns target selection,
attack intent with exact suppression reason, dash policy, and the Payload role
destination. The host keeps actor state, perception, weapon legality, damage
authority, and movement execution. This is source/build work only. The live
19-NPC Payload/FunWorld3 test is blocked by the navigator stall recorded in the
server-tick regression; the report remains REPORTED / NOT YET REPRODUCED.
