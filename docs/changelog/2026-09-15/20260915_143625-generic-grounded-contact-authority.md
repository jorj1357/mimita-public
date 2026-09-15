# Generic grounded/contact authority (player)

- EST timestamp: 2026-09-15 14:36:25 EDT (UTC 2026-09-15T18:36:25Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + full suite 27/27)

Architecture-first pass: move the player's authoritative grounded/contact state
into the generic `MovementRuntimeStateComponent`.

## Report
- SUBSYSTEM: movement grounded / contact authority.
- OLD AUTHORITY: `ServerPlayer.onGround` fed `MovementState.ground.onGround`.
- NEW AUTHORITY: the entity's generic `MovementRuntimeStateComponent.grounded`
  (ensured once per actor and seeded from typed on first sight) is read as the
  authoritative grounded input; the cold collision result writes it back.
- GENERIC COMPONENTS USED: `TransformComponent`, `VelocityComponent`,
  `MovementRuntimeStateComponent` (grounded).
- COLD OWNER REMOVED: typed `ServerPlayer.onGround` as the movement grounded
  input.
- COMPATIBILITY PROJECTIONS: `ServerPlayer.onGround` is written from
  `applyMovementStateToServerPlayer` (and the generic component) — read-only
  mirror for legacy/snapshot/diagnostics.
- GROUND/CONTACT AUTHORITY: generic `MovementRuntimeStateComponent.grounded`.
- ABILITY-RUNTIME AUTHORITY: still the typed `player.movement` working copy for
  jump/dash/freeze (next).
- PLAYER PROOF: generic-direct grounded read/write.
- NPC PROOF: not done.
- RUNTIME-GENERIC ACTOR PROOF: valid (typeless actor test unchanged).
- REWIND SOURCE: generic Transform. SNAPSHOT SOURCE: typed projection from
  generic.
- SELFTEST PROVEN: full suite 27/27 (grounded override test of `simulatePlayer`
  is not directly runnable headlessly; documented).
- LIVE HOT-EDIT PROVEN: no.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Ordinary movement grounded bugs: no
  (hot/generic). Collision mechanism: yes (cold by design).
- MOVEMENT CATEGORY COMPLETE? **No** — remaining: jump/dash/freeze runtime state
  on the generic component, and one real NPC path.
- NEXT COLD OWNER: generic ability runtime state, then the NPC path, then
  reconciliation.

## Notes / ops
- A concurrent `live-behavior.cpp` edit temporarily broke the kernel build
  (`pushSurfaceDecal`/`spawnGenericSurfaceDecal` private); the suite reported 27
  "NO-RESULT" until the build succeeded. Spurious `--gamemode-hot`/`--hot-combat`
  FAILs also occurred during a concurrent relink; re-running gave 27/27. Not code
  regressions.
- Discipline note: I scoped this pass to grounded/contact (one owner) rather than
  bundling jump/dash/freeze runtime state + NPC, to keep each authority move
  verifiable.

## Files changed
`src/network/server-players.cpp` (generic grounded read/write via
`MovementRuntimeStateComponent`); docs + this changelog.

## Next (auto-selected)
Move jump/dash/freeze runtime state onto `MovementRuntimeStateComponent`; migrate
one real NPC path onto the same substrate; then reconciliation policy →
interpolation → rewind policy → distributed generation delivery → READY/SWITCH.
