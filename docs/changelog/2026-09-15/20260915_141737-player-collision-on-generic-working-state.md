# Player collision on generic working movement state (integrator authority, partial)

- EST timestamp: 2026-09-15 14:17:37 EDT (UTC 2026-09-15T18:17:37Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + full suite 26/26; air-parity `[warn]`
  unchanged)

Architecture-first pass: remove the typed `ServerPlayer` from the player collision
input and drive collision from the generic working movement state.

## Report
- SUBSYSTEM: movement integrator / collision boundary.
- OLD AUTHORITY: Phase-2 `resolveWorldCollision(ServerPlayer& p, world)` mutated
  typed `p.pos/p.vel/p.onGround`.
- NEW AUTHORITY: `resolveCapsuleCollisionAgainstWorld(world, state.position,
  state.baseVelocity, radius, height, onGround)` operates on the generic working
  `MovementState`; typed fields are projected from it
  (`applyMovementStateToServerPlayer`), and the scope guard writes generic
  Transform/Velocity.
- COLD OWNER REMOVED: typed collision input in the server player pipeline.
- HOT OWNER USED: the hot movement policies (air/ground/gravity/speed/jump/dash/
  freeze/clamp) already run inside the pipeline; collision is now generic.
- COLD MECHANISM REMAINING: `gatherHeadlessTrianglesForAABB` / the capsule-vs-
  world sweep (`resolveCapsuleCollisionAgainstWorld`) — intentionally cold.
- TYPED PROJECTIONS REMAINING: `ServerPlayer.pos/vel/yaw/health/dead`,
  `ServerNpc.pos/vel/health`, `Npc.body.*` (snapshot/legacy/diagnostics).
- PLAYER PROOF: collision consumes/returns generic working state (compiler-
  verified; not exercised by the air-parity harness, which bypasses `simulatePlayer`).
- NPC PROOF: not done this pass (pending).
- RUNTIME-GENERIC ACTOR PROOF: not done this pass (pending).
- REWIND SOURCE: generic Transform (previous pass; unchanged).
- SNAPSHOT SOURCE: typed compatibility fields projected from generic.
- SELFTEST PROVEN: existing movement/serverspatial selftests + full suite 26/26.
- PARITY STATUS: air-parity `[warn]` unchanged (maxDev 2.2699, first divergent
  tick 68).
- LIVE HOT-EDIT PROVEN: no.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Ordinary movement policy bugs: no
  (hot). Collision mechanism bugs: yes (cold by design).
- NEXT COLD OWNER: finish the generic working-copy read (no typed population),
  NPC/generic-actor path, then reconciliation/interpolation/rewind/delivery.

## Honest scope
This is a partial integrator-authority step: the collision boundary is now
generic, but the working `MovementState` is still populated from typed
`ServerPlayer` (which is refreshed from generic at the top of the tick). The full
"read generic directly, never typed" working copy and the NPC/generic-actor proof
are the next slice.

## Files changed
`src/network/server-players.cpp` (Phase-2 collision on the generic working state);
docs + this changelog.

## Next (auto-selected)
1) populate the working movement state directly from generic Transform/Velocity
   (no typed read), 2) NPC + runtime-generic actor on the same substrate,
3) reconciliation policy, then interpolation, rewind policy, distributed
generation delivery, READY/shared SWITCH.
