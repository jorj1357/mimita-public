# Generic integrator authority: player reads/writes generic directly + typeless-actor proof

- EST timestamp: 2026-09-15 14:22:49 EDT (UTC 2026-09-15T18:22:49Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--generic-integrator-selftest` 3/3 +
  full suite 27/27)

Architecture-first pass: make the player's authoritative movement input/output
generic, and prove the substrate is class-agnostic.

## Report
- SUBSYSTEM: movement integrator authority.
- OLD AUTHORITY: vertical/horizontal read from typed `ServerPlayer` into
  `MovementState`; collision wrote typed; scope guard recovered generic from typed.
- NEW AUTHORITY: `MovementState` position/velocity/yaw are read DIRECTLY from the
  entity's generic Transform/Velocity; collision operates on the generic working
  state; the result is written DIRECTLY to generic Transform/Velocity.
- COLD OWNER REMOVED: typed `ServerPlayer` as the authoritative movement input
  and the typed re-read between Phase 2 and Phase 3.
- GENERIC INPUT SOURCE: generic `TransformComponent`/`VelocityComponent` on the
  actor entity.
- GENERIC OUTPUT SINK: the same components (typed fields are a projection).
- TYPED PROJECTIONS REMAINING: `ServerPlayer.pos/vel/yaw/onGround/health/dead`,
  `ServerNpc.pos/vel/health`, `Npc.body.*`; and the jump/dash/freeze runtime
  working state (`player.movement`).
- GROUND/CONTACT AUTHORITY: still `ServerPlayer.onGround` (no generic grounded
  component yet) — documented, next.
- PLAYER PROOF: reads/writes generic directly (compiler + suite; not exercised by
  the air-parity harness which bypasses `simulatePlayer`).
- NPC PROOF: not done.
- RUNTIME-GENERIC ACTOR PROOF: PROVEN — `--generic-integrator-selftest` runs a
  typeless entity (domain None) through the same pipeline + hot policies,
  integrates, and repeats deterministically.
- REWIND SOURCE: generic Transform (unchanged).
- SNAPSHOT SOURCE: typed compatibility fields projected from generic.
- SELFTEST PROVEN: generic-integrator (typeless actor + determinism); full suite
  27/27.
- PARITY STATUS: air-parity `[warn]` unchanged (maxDev 2.2699, first divergent
  tick 68).
- LIVE HOT-EDIT PROVEN: no.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Ordinary movement policy/integration
  bugs: no (hot). Collision mechanism bugs: yes (cold by design).
- MOVEMENT CATEGORY COMPLETE? **No** — remaining: generic grounded/contact,
  generic jump/dash/freeze runtime state for the server player, and one real NPC
  path on the same substrate.
- NEXT COLD OWNER: generic grounded + runtime ability state, then the NPC path,
  then reconciliation policy.

## Bridges classification
- `serverProjectActorSpatialFromGeneric` (top-of-tick typed refresh) —
  **compatibility projection** (typed consumers within the tick); no longer the
  authority.
- `serverProjectActorSpatialToGeneric` / `simulatePlayer` scope guard —
  **compatibility failsafe** (generic is written directly now).
- `FromGeneric` is no longer required to make movement authoritative; it is kept
  only to keep typed compatibility consumers consistent.

## Files changed
`src/network/server-players.cpp` (generic-direct input/output; no typed re-read),
`src/network/generic-integrator-selftest.{h,cpp}` (new),
`src/game/game-cli.cpp`; docs + this changelog.

## Next (auto-selected)
Generic grounded/contact + generic runtime ability state; one real NPC path on
the same substrate; then reconciliation policy → interpolation → rewind policy →
distributed generation delivery → READY/shared SWITCH.
