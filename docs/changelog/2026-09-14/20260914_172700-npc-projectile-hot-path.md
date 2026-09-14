# NPC Projectile Hot Path

Date: 2026-09-14 17:27 EST
Branch: worktree (uncommitted)

## Result

`PASS_WITH_HUMAN_REVIEW`

`server-npcs.cpp::broadcastNpcFiring` now resolves the NPC ECS identity, fills
`ToolUsePolicyV1` with the real origin/direction and owner, and calls
`LiveBehavior::dispatchToolUse`. The hot rocket/grenade behaviors create the
canonical entity carrying `HotProjectileStateV1`; `projectiles.60` owns its
simulation. The old NPC `ServerProjectile` allocation, map insertion,
`Ecs::spawnRocket`, and manual spawn broadcast were removed.

This pass also disconnected the remaining normal player fallback: projectile
`AttackRequest` and held-fire paths reject when hot behavior does not handle
the action instead of calling `handleGenericProjectileAttack`. Both server-loop
calls to `tickServerProjectiles` were removed.

## Review and evidence

- Routed specs: weapons, networking, ECS, hot-kernel, player/NPC systems, and
  live-code development.
- Focused skills: `spec-behavior-review-v1.md` PASS; logging review notes that
  runtime canonical-entity evidence still needs playtest; efficiency review
  found no new per-tick allocation.
- `python devscripts/live-build.py`: PASS, generation 15 DLL; EXE untouched.
- `python build_agent.py`: BUILD SUCCESS, canonical EXE linked.
- `mimita.exe --hot-combat-selftest`: PASS, including rocket, grenade,
  canonical state, projectiles.60, damage, effects, and no kernel-container
  spawn checks.
- NPC-fire runtime and live-edit falsification were not performed.

Pre-existing concurrent edits were preserved and are not claimed here.

## Next

Remaining compatibility code in `server-projectiles.cpp` is the disabled legacy
fire packet handler plus compiled dead helpers, while client prediction/render
and test-only `Ecs::spawnRocket` references remain. The file's gameplay policy
cannot yet be deleted until that compatibility surface is removed deliberately.
