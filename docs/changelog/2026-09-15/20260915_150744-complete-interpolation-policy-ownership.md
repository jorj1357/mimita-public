# Complete interpolation-policy ownership (net.interpolate)

- EST timestamp: 2026-09-15 15:07:44 EDT (UTC 2026-09-15T19:07:44Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--interpolation-policy-selftest` 10/10; full suite 29/29)

## Report
- SUBSYSTEM: remote-actor render interpolation policy.
- REAL SHIPPING PATH MIGRATED: `buildReceiveTimeRender`
  (multiplayer-interpolation.cpp) — single `net.interpolate` call at the top now
  drives delay, snap, buffer-dry, extrapolation cap, and alpha.
- COLD POLICY OWNER REMOVED: the cold branches no longer decide
  extrapolate-vs-hold, the extrapolation cap, gap snapping, or generation
  snapping on their own when hot handles the request.
- HOT POLICY OWNER ADDED: `net.interpolate` (`modules/interpolate-policy.cpp`).
- GENERIC PAYLOAD: `GameInterpolateV1` extended (a/b samples, oldest/newest tick,
  bufferDepth, renderTick, delaySeconds, allowExtrapolation, bufferDry,
  packetGapTicks, lifecycleChanged, generation ids; out: handled, mode
  (interpolate/extrapolate/hold/snap/reset), hardSnap, shouldResetBuffer,
  outAlpha, outDelaySeconds, outExtrapolationMs). No Player/Npc/snapshot/client
  pointers.
- INTERPOLATION POLICY NOW HOT: alpha, generation/lifecycle snap, packet-gap
  snap, buffer-dry hold/extrapolate, extrapolation cap, delay override.
- COLD MECHANISM REMAINING: sample ring storage, timestamps, render clock,
  packet decode, numeric mix (`glm::mix`), extrapolation position math, hold/snap
  application, adaptive-delay measurement.
- GENERATION AWARENESS: explicit (a/b/current must match, else snap); ids are 0
  until generations are wired.
- COMPATIBILITY FALLBACK: cold decisions run when no hot handler is registered.
- SELFTEST PROVEN: normal alpha, clamp both directions, generation boundary snap,
  current-generation mismatch snap, buffer-dry hold, buffer-dry extrapolate with
  cap, packet-gap snap, deterministic.
- REAL PATH PROVEN: yes. LIVE HOT-EDIT PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected — did not edit the movement agent's
  `game-api.h` component, `live-behavior.cpp` mapping, or `movement-system.cpp`;
  presentation/effects failures untouched (now green).
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Interpolation delay/extrapolation/
  stale/gap policy bugs: **no** (hot). Sample storage/clock/decode/mix mechanism:
  yes (cold by design).
- INTERPOLATION CATEGORY COMPLETE? **Mostly** — alpha/snap/extrapolation/stale/
  gap/lifecycle are hot; the delay policy is wired but a pass-through placeholder
  (adaptive-delay measurement remains cold), and generation ids are 0.
- NEXT COLD OWNER: rewind / lag-compensation policy, then distributed generation
  delivery with real generation ids.

## Files changed
`src/hot-reload/hot-interpolation.h` (extended payload),
`src/hot-reload/modules/interpolate-policy.cpp` (full policy),
`src/network/multiplayer-interpolation.cpp` (single top call + cold consults),
`src/network/interpolation-policy-selftest.cpp` (new cases); docs + this changelog.

## Next (auto-selected)
Rewind/lag-comp policy (generic historical-state query; hitscan + melee +
projectile/explosion consumers), then distributed generation delivery.
