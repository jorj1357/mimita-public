# Adaptive interpolation-delay ownership (interpolation category complete)

- EST timestamp: 2026-09-15 15:13:53 EDT (UTC 2026-09-15T19:13:53Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--interpolation-policy-selftest` 15/15; full suite 29/29)

## Report
- SUBSYSTEM: adaptive interpolation delay + interpolation policy completion.
- REAL SHIPPING PATH MIGRATED: `adaptiveDelaySeconds`
  (multiplayer-interpolation.cpp) now dispatches `net.interpolate` (delayQuery)
  and adopts the hot result; `buildReceiveTimeRender` already uses the hot
  alpha/snap/extrapolation policy.
- COLD POLICY OWNER REMOVED: the cold adaptive-delay convergence is no longer the
  sole owner (it remains fallback + measurement source).
- HOT POLICY OWNER ADDED: `net.interpolate` delayQuery branch.
- GENERIC PAYLOAD: `GameInterpolateV1` extended with delay-query facts
  (currentAdaptiveDelaySeconds, baseDelaySeconds, estimatedJitterMs,
  recentLossFraction, min/maxDelaySeconds, increase/decreaseRateMsPerSecond,
  jitterMultiplier, lossDelayBudgetSeconds, deltaSeconds). No Player/Npc/client
  pointers.
- ADAPTIVE DELAY OWNERSHIP: HOT — the hot policy computes the desired delay
  (bounds, jitter response, loss/starvation response, convergence rate);
  `outDelaySeconds` is a real result, not a pass-through.
- COLD MECHANISM REMAINING: sample storage, timestamps/clock, packet decode,
  jitter/loss measurement, numeric mix/extrapolation, applying the returned delay.
- GENERATION AWARENESS: explicit (snap on mismatch); ids remain 0 (distributed
  gap, not interpolation).
- COMPATIBILITY FALLBACK: cold adaptive-delay logic runs when no hot handler is
  registered (last-good).
- SELFTEST PROVEN: healthy stream within bounds, jitter raises delay (bounded),
  loss/starvation raises delay, recovered stream permits reduction, min bound,
  deterministic.
- REAL PATH PROVEN: yes. LIVE HOT-EDIT PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected (movement + presentation files
  untouched).
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Interpolation delay/selection/
  extrapolation/stale/gap policy bugs: **no** (hot). Storage/clock/decode/mix
  mechanism: yes (cold by design).
- INTERPOLATION CATEGORY COMPLETE? **Yes (policy)** — all listed decisions are
  hot. Generation ids being 0 is a distributed-generation gap, explicitly out of
  scope here.
- REWIND AUDIT STARTED? **Yes** — `estimateServerRewindTick` +
  `getPlayerPoseAtTick`/`getNpcPoseAtTick` (server-attack.cpp) identified as the
  next cold policy owner: command tick → latency/interp compensation → rewind
  target → history lookup → historical interpolation → clamp → hit query.
  Note the known gap: hitscan rewinds; explosion victim pose is not fully rewound
  (to be addressed by a generic historical-state query, not a rocket special case).
- NEXT COLD OWNER: rewind / lag-compensation policy, then distributed generation
  delivery with real generation ids.

## Files changed
`src/hot-reload/hot-interpolation.h` (delay-query facts),
`src/hot-reload/modules/interpolate-policy.cpp` (adaptive-delay policy),
`src/network/multiplayer-interpolation.cpp` (`adaptiveDelaySeconds` dispatch),
`src/network/interpolation-policy-selftest.cpp` (adaptive-delay cases);
docs + this changelog.

## Next (auto-selected)
Rewind/lag-comp policy: migrate `estimateServerRewindTick` decision hot with a
generic `GameRewindPolicyV1`, and provide a generic historical-state query usable
by hitscan, melee, projectile impact, and explosion evaluation.
