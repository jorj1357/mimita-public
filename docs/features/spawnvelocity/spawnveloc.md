9 6 2026 1539 -  todo put a template taht is human understandable for how to do feature things 

9 9 2026 1336  jorj - template is rough for n ow 

date file created in iso 8601 format

end goal/end behavior

notes/thoughts 

todo make this nicer and clearer cleaner 

also, 9 9 2026 1348 jojr - u can just  write like freely , then have AI clean up uour wording , but u have to check it to ensure the wording is correct and what u mean

#######################################
#######################################
#######################################
human fills this part out 
#######################################
#######################################
#######################################

# Feature name

spawn velocity

## Purpose ################################

What user-visible capability this feature provides.
this is to provide a initial boost on spawn for plrs or npcs  in a direction so that they start out with momentum so that they dont just sit still in teh air until they touch the ground or dash etc

## Desired behavior ################################

Exact observable behavior.
i spawn and then i am instantly shot out in the look direction i spawn with , like im facing forward = i get  a velocity forward

## Current behavior ################################

What happens now, with evidence.
that happens, but like once every 15 spawns, its very very inconsistent thats not good

9 9 2026 1537 run: its more consistnet now but still fails sometimes, e.g. when i do explode command over and over to kill m own player , sometimes it wont do the velocity when i spawn, so ordering might still be wrong ?  its gotten better tho betweeon now and when it had like a 5% chance of working 

## Current status, from working best to not working at all ################################

its wokring kinda 

solved as of (iso 8601 time) | implemented | not implemented 

#######################################
#######################################
#######################################
AI fills this part out 
#######################################
#######################################
#######################################

## Decisions ################################

Explicit choices and unresolved NEEDS_SPEC_DECISION items. NEEDS_SPEC_DECISION just means the wording is not clear enough and needs better detail. AI should point out what is not super clear and ask for a clearer definition of it before implemneting the feature. 

## Ownership

- Primary code owner:
- Configuration owner:
- Runtime/event owner:
- Network owner, if applicable:
- Animation/physics owner, if applicable:

## Related authoritative documents

- Specification:
- Architecture:
- Workflow:
- Focused review skill:
- Regression record:

## Relevant files

Links to actual source/config files.

## Tests and evidence

- Automated tests:
- Runtime commands:
- Logs:
- Human playtest:

## Acceptance criteria

Exact true/false statements, or exact numbers that proves the feature is a pass/fail.

## Changelog and regression links

Links only; do not duplicate full history.

## Unified lifecycle acceptance

Spawn velocity is owned by the shared authoritative actor lifecycle, not by a
player-only or NPC-only respawn path.

Every new actor life emits one spawn event containing the entity identity, actor
kind, spawn reason, generation, transform epoch, server tick, position, look
direction, and velocity. The velocity must be calculated from the same look
direction stored in that event.

First join, reconnect, normal respawn, NPC creation, NPC respawn, duel start,
gamemode start, map change, and `respawn_all` must use that lifecycle. Existing
terminal command names and network acknowledgement behavior remain unchanged.

ECS migration is incremental: transitional player/NPC structs may remain while
identity, transform, movement, health, lifecycle, and timer data are moved into
shared components over time.
