# Hot freeze + post-step speed clamp ownership

- EST timestamp: 2026-09-15 14:13:33 EDT (UTC 2026-09-15T18:13:33Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--movement-algorithm-selftest` 28/28 +
  full suite 26/26; air-parity `[warn]` unchanged)

Architecture-first pass: migrate freeze and the post-acceleration speed clamp into
shared hot policy.

## Report — freeze
- SUBSYSTEM: movement freeze.
- COLD OWNER REMOVED: `updateFreeze` (movement-step.cpp) is no longer the sole
  owner (now fallback); the client freeze velocity-zeroing block is replaced.
- HOT OWNER ADDED: `MimitaHotMovement::freezePolicy` (new
  `modules/movement-freeze.cpp`), payload `GameFreezePolicyV1` + event
  `movement.freeze` — activation eligibility, duration, velocity suppression
  while frozen, held/released transitions, exit.
- COMPATIBILITY FALLBACK: built-in `updateFreeze` logic when no hot handler.
- STATE AUTHORITY: generic runtime movement state (`freezePreviously`) + generic
  `Velocity`; plain numbers only.
- SERVER CALL SITE: `updateFreeze` dispatches `movement.freeze` and applies the
  returned state (incl. `externalImpulse` clear and freeze events).
- CLIENT CALL SITE: `movement.main` calls the same function; frozen velocity is
  suppressed by the shared policy.
- SELFTEST PROVEN: activation + suppression, remain-frozen, release/exit,
  duration clamp.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No.

## Report — post-step speed clamp
- SUBSYSTEM: movement post-acceleration speed clamp / preservation.
- COLD OWNER REMOVED: `applySpeedLimitClamp` body is now a fallback.
- HOT OWNER ADDED: `MimitaHotMovement::speedClamp` (new
  `modules/movement-speed-clamp.cpp`), payload `GameSpeedClampV1` + event
  `movement.speed-clamp`.
- COMPATIBILITY FALLBACK: built-in clamp when no hot handler.
- STATE AUTHORITY: generic `Velocity`; plain numbers.
- SERVER CALL SITE: `applySpeedLimitClamp` dispatches `movement.speed-clamp`.
- CLIENT CALL SITE: none — the client path has no equivalent clamp (documented;
  its velocity is governed by the shared air/ground/speed policies today).
- SELFTEST PROVEN: horizontal max-speed enforcement + passthrough when disabled.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? No.

## Parity status
Air-parity `[warn]` unchanged after both migrations: maxDev 2.2699, first
divergent tick 68 (serverVel=(16.6421,11.6421) vs clientVel=(16.4297,11.4936)).
The clamp is disabled in that scenario, so it did not change the result. The
remaining difference is elsewhere (client collision pipeline / preservation
inside the shared air step). Recorded, not chased per the architecture-first
policy.

## Movement category status
Air, ground, gravity, speed policy, jump, dash/down-dash, freeze, clamp are all
HOT/shared. The only remaining movement item is the typed working-copy integrator
(server-players.cpp) → direct generic Transform/Velocity authority (plus the NPC
path). After that, the movement category is done per the stated criteria.

## Files changed
`src/hot-reload/hot-movement-policy.h` (freeze + speed-clamp payloads/decls),
`src/hot-reload/modules/movement-freeze.cpp` (new),
`src/hot-reload/modules/movement-speed-clamp.cpp` (new),
`src/physics/movement/movement-step.cpp` (server hooks + fallbacks),
`src/hot-reload/modules/movement-system.cpp` (client uses shared freeze),
`src/network/movement-algorithm-selftest.cpp` (freeze + clamp checks);
docs + this changelog.

## Next (auto-selected)
Finish generic movement integrator authority (direct generic Transform/Velocity
for player, then one NPC/generic actor; typed fields become projections). Then
networking/simulation policy: reconciliation, interpolation, rewind/lag-comp,
distributed generation delivery, READY/shared SWITCH.
