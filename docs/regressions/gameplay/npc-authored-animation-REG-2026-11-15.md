# NPCs do not use authored keyframes and effects

Date: 2026-11-15
Status: OPEN — hot-fixable gameplay issue
Owner: NPC animation / effects

## Symptom

NPC animations look slow and do not visibly use the new authored keyframes.
Freeze and down-dash effects are missing for local movement.

## Evidence

`anim.melee.pose_probe` reports `authored_only=false`, `runtime_count=0`,
`set_valid=false` for the local Jason actor. `anim.dash.clip_missing` is
frequent for NPC avatars.

## Required fix

- Bind authored animation sets per NPC avatar.
- Log `npc.anim.bind`/`npc.anim.playback`.
- Scale playback speed by actor movement speed.
- Trace `effect.freeze`/`effect.down_dash` through the shared effect owner.
