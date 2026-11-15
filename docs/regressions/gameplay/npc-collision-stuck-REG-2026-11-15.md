# NPCs stuck inside geometry

Date: 2026-11-15
Status: OPEN — hot-fixable gameplay issue
Owner: NPC collision/depenetration

## Symptom

NPCs remain inside world geometry or fail to resolve penetration.

## Requirement

All NPC collision/behavior sources must be hot. The other agent is converting
`module:npc-*` sources into the single game DLL; do not create a cold path.

## Evidence to add

`npc.collision.stuck`: actor, position, penetration, contact normal, world
triangle, support state. Prove a fix live with agent commands and journals.
