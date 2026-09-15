# Generic collision boundary + shared hot movement-policy parity

- EST timestamp: 2026-09-15 11:06:17 EDT (UTC 2026-09-15T15:06:17Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 8/8 +
  full suite 25/25)

## 1. Concurrent movement-state audit
The concurrent agent owns `MovementRuntimeStateComponent`,
`movement-system.cpp` (local hot `movement.main`), and the related
`live-behavior.cpp` bridge. Those files were **not** edited here. Ownership for
this pass: `server-players.cpp` collision, `movement-step.cpp` + `movement-air`
(hot policy), and the parity selftest.

## 4. Collision operates on generic movement state
- Extracted `resolveCapsuleCollisionAgainstWorld(const HeadlessWorld&, glm::vec3&
  pos, glm::vec3& vel, float radius, float height, bool& onGround)` in
  `server-players.cpp` (declared in `server.h`). It uses only position, velocity,
  radius, height, and contact facts — no `ServerPlayer`.
- `resolveWorldCollision(ServerPlayer&, ...)` is now a thin typed wrapper that
  calls the generic primitive and keeps the below-map debug log.
- The cold physics still owns sweep/slide/penetration (mechanism); no game-feel
  policy was moved into collision.

## 7-8. Shared hot movement policy
- The hot `movement.air-accelerate` handler (`movement-air.cpp`) is the single
  air-acceleration implementation, fed by plain numbers.
- New selftest evidence: the shared policy produces **identical multi-tick
  results in two independent contexts** (150 ticks), proving it is context-free
  and deterministic — the structural precondition for one hot implementation to
  drive both authoritative simulation and client prediction.

## 9-12. Rewind / snapshot / invariants
- Rewind already samples generic Transform/Velocity (previous pass); unchanged.
- Wire snapshot remains the typed compatibility format; its values derive from
  the generic authority.
- Invariant helper `serverProjectActorSpatialToGeneric` continues to keep typed
  fields as projections.

## Status labels
- SELFTEST PROVEN: generic collision primitive (no ServerPlayer) is in place and
  the shared hot air policy is context-free and deterministic over multi-tick
  sequences; hot algorithm ownership (previous pass) unchanged.
- COMPILED INTEGRATION: `resolveWorldCollision` delegates to the generic
  primitive; full suite 25/25.
- SERVER/CLIENT MOVEMENT PARITY PROVEN: no. The local hot `movement.main` still
  uses different movement math.
- LIVE MOVEMENT HOT-EDIT PROVEN: no.
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game movement feel; prediction/server alignment.

## Honest limits (success-bar gaps)
- #1/#2 player/NPC integrator still runs on the typed working copy
  (read-early/project-late); it does not yet integrate directly on generic state.
- #6/#7 the client prediction path (`movement-system.cpp`, movement-state agent's
  file) was **not** unified with the shared hot policy. Per the concurrency rule,
  this boundary is documented rather than independently rewritten. Consequently
  #7 (server/client parity selftest) and #8 (one edit changes both) are **not**
  established; only the shared-policy context-freedom is proven.
- #9 snapshot wire payload remains typed (derives from generic).

## Files changed
`src/network/server-players.cpp` (generic collision extraction),
`src/network/server.h` (declaration),
`src/network/movement-algorithm-selftest.cpp` (shared-policy multi-tick parity);
docs + this changelog.

## Next (auto-selected)
Coordinate with the movement-state agent to make `movement.main` call the same
shared hot policy, then build the server/client parity selftest and the
one-edit-changes-both proof. After parity, distributed hot generation activation.
