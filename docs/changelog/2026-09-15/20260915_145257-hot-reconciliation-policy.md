# Hot reconciliation policy (net.reconcile)

- EST timestamp: 2026-09-15 14:52:57 EDT (UTC 2026-09-15T18:52:57Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` for the reconciliation lane (`--reconciliation-policy-selftest`
  7/7). Suite 28 tests / 27 PASS; the one failure is an unrelated concurrent
  effects/camera breakage (see below).

## Report
- SUBSYSTEM: client reconciliation policy.
- REAL SHIPPING PATH MIGRATED: `mpReconcileLocalPlayer`
  (src/network/multiplayer-reconcile.cpp) now calls the hot policy before
  deriving `MovementCorrectionClass`.
- COLD POLICY OWNER REMOVED: `classifyMovementCorrection(error, config)` is no
  longer the sole classifier (now a fallback).
- HOT POLICY OWNER ADDED: `net.reconcile` (`modules/reconcile-policy.cpp`):
  error metric + thresholds (ignore/smooth/medium/snap) and generation-mismatch
  -> hard reset.
- COLD MECHANISM REMAINING: packet receipt/decode, prediction + input history
  storage, epoch/lifecycle bookkeeping, applying the chosen correction
  (snap/smooth), post-gap flag timing.
- GENERIC PAYLOAD: `GameReconcileV1` (predicted/authoritative position+velocity,
  error, thresholds, ticks, gap, lifecycle flag, predicted/authoritative
  generation ids; out: shouldCorrect/correctionMode/hardReset/replayInputs).
  No Player, client, or snapshot pointers.
- GENERATION AWARENESS: yes — `predictedGeneration != authoritativeGeneration`
  returns an explicit hard reset (mode 4), never silent normal reconciliation.
  Generation ids are currently 0 (not yet populated); the policy handles them.
- COMPATIBILITY FALLBACK: cold `classifyMovementCorrection` runs when no hot
  handler is registered (last-good).
- SELFTEST PROVEN: tiny->no, medium->smooth, large->medium, very large->snap,
  generation mismatch->hard reset, deterministic.
- REAL PATH PROVEN: wired into the real `mpReconcileLocalPlayer` (not synthetic
  only). LIVE HOT-EDIT PROVEN: no.
- CONCURRENCY BOUNDARY STATUS: respected — did not edit the movement agent's
  `game-api.h` component, `live-behavior.cpp` mapping, or `movement-system.cpp`.
- MOVEMENT COMPLETION STATUS: unchanged (still pending freeze/coyote/lock generic
  fields + real NPC path; owned by the movement agent).
- WOULD THIS BUG STILL REQUIRE COLD RESTART? Reconciliation decision/threshold
  bugs: **no** (hot). Raw packet/clock/history-buffer mechanism: yes (cold by
  design).
- NEXT COLD OWNER: interpolation policy (delay/sample choice/lerp-vs-extrapolate/
  stale/jitter), then rewind/lag-comp policy, then distributed generation
  delivery.

## Unrelated concurrent failure (recorded, not fixed)
`--hot-combat-selftest` fails on:
- "real explosion shake reaches the hot camera policy"
- "hot camera-effect command works (no enum)"

These are the effects/camera agent's active files (`effects/*`,
`engine-tick-render.cpp`, presentation). Per the concurrency rule, I did not edit
around them. Not caused by this reconciliation slice.

## Files changed
`src/hot-reload/hot-reconciliation.h` (new),
`src/hot-reload/modules/reconcile-policy.cpp` (new),
`src/network/reconciliation-policy-selftest.{h,cpp}` (new),
`src/network/multiplayer-reconcile.cpp` (hot dispatch + fallback),
`src/game/game-cli.cpp`, `src/hot-reload/hot-modules.json`; docs + this changelog.

## Next (auto-selected)
Interpolation policy hot (real path `EntityInterpolationState` sample selection /
lerp/extrapolate / stale handling), then rewind/lag-comp policy, then the
distributed generation delivery milestone.
