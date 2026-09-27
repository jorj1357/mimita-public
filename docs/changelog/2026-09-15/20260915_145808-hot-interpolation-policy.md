# Hot interpolation policy (net.interpolate)

- EST timestamp: 2026-09-15 14:58:08 EDT (UTC 2026-09-15T18:58:08Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--interpolation-policy-selftest` 6/6; full suite 29/29)

## Report
- SUBSYSTEM: remote-actor render interpolation policy.
- REAL SHIPPING PATH MIGRATED: `buildReceiveTimeRender`
  (src/network/multiplayer-interpolation.cpp) dispatches `net.interpolate` after
  computing alpha.
- COLD POLICY OWNER REMOVED: the alpha/generation-snap decision is no longer
  solely cold.
- HOT POLICY OWNER ADDED: `net.interpolate` (`modules/interpolate-policy.cpp`):
  alpha clamp/override + generation-boundary hard snap.
- GENERIC PAYLOAD: `GameInterpolateV1` (sample A/B position+velocity+tick,
  renderTick, alpha, allowExtrapolation, bufferDry, packetGapTicks,
  lifecycleChanged, a/b/current generation ids -> handled/hardSnap/outAlpha/
  outPosition/outVelocity). No Player, Npc, snapshot, or network-client pointers.
- COLD MECHANISM REMAINING: sample ring storage, timestamps, render clock, packet
  decode, the numeric `glm::mix`, extrapolation math, hold/snap execution.
- GENERATION AWARENESS: yes — any mismatch among sample A / sample B / current
  generation forces a hard snap (no cross-generation blend). Ids are 0 today;
  the policy handles them once populated.
- COMPATIBILITY FALLBACK: cold path runs when no hot handler is registered.
- SELFTEST PROVEN: normal alpha preserved, alpha clamped (both directions),
  generation boundary -> snap, current-generation mismatch -> snap, deterministic.
- REAL PATH PROVEN: yes (wired into `buildReceiveTimeRender`). LIVE HOT-EDIT
  PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected — did not edit the movement agent's
  `game-api.h` component, `live-behavior.cpp` mapping, or `movement-system.cpp`.
- UNRELATED FAILURES IGNORED: earlier `--hot-combat-selftest` camera/effects
  failures (presentation agent) are now green again; not touched.
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Interpolation alpha/snap-policy bugs:
  **no** (hot). Sample storage/clock/decode/mix mechanism: yes (cold by design).
- NEXT COLD OWNER: rewind / lag-compensation policy, then distributed generation
  delivery.

## Scope note
This pass migrates the alpha/snap decision at the real call site. The remaining
interpolation policy (delay re-targeting, extrapolation allow/cap decision, stale
handling) still lives cold in `buildReceiveTimeRender`; those are the next
interpolation slice. I kept the first hook small and real rather than rewriting
the whole function.

## Files changed
`src/hot-reload/hot-interpolation.h` (new),
`src/hot-reload/modules/interpolate-policy.cpp` (new),
`src/network/interpolation-policy-selftest.{h,cpp}` (new),
`src/network/multiplayer-interpolation.cpp` (hot dispatch + fallback),
`src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`; docs + this changelog.

## Next (auto-selected)
Extend hot interpolation policy (delay, extrapolation decision/cap, stale
handling), then rewind/lag-comp policy, then distributed hot generation delivery.
