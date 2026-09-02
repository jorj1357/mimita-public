# Remote Z Jump-Snap — Investigation & Fix Plan

Date: 2026-08-12
Status: implemented (Phase 1 — see "Recommended fix" below)

## Problem

In duels mode with bad network conditions (badconn on both clients), remote
players:

- snap extremely high upward on Z when they jump (~10x real jump height), then
  come back down / float
- snap upward/downward on Z during packet loss
- appear airborne while actually grounded
- desync vertically on chainofjudgement / other duel maps

The user found that lowering `ease_correction_rate` to `0.01` fixed the big X/Y
horizontal snapping, but Z (up/down) stayed broken, and there was no obvious
Z-specific JSON setting.

## Root cause (code evidence)

The X/Y snap and the Z snap are caused by two different code paths.

1. **X/Y snap was the correction pull-back.** With `ease_correction_rate` at
   its committed default of `12.0`, the ease filter body was glued to the
   interpolated target. When the target jumped ahead (extrapolation) and then
   came back (data resumed), the body snapped forward and snapped back.
   Lowering the rate to `0.01` removed the pull-back, fixing X/Y.

2. **Z snap is extrapolation, which the rate knob does not touch.**
   In `buildReceiveTimeRender` (`src/network/multiplayer-interpolation.cpp:201`),
   when the snapshot buffer runs dry the game guesses the remote position with a
   straight line: `position + velocity * time`. For a jump the last-received
   velocity is `vz = +19` (jump strength, `src/physics/config.h:14`). During a
   badconn blackout this guess runs for up to `maximum_extrapolation_ms` (1000)
   plus an exponential "keep moving" decay, so the extrapolated Z can climb
   ~19 * 1.5s = ~28 units. The real jump apex is `v^2/(2g)` = `19^2/116` =
   **3.11 units**. That ~9x-10x overshoot is the reported "jump 10x too high".

   Walking (X/Y) is not as broken because walking velocity is roughly constant,
   so a straight-line guess approximately matches real motion. Jumping (Z) is a
   gravity arc; the guess only ever sees the first instant of the arc (+19) and
   holds it forever.

3. **The body then floats.** The ease filter (`multiplayer-interpolation.cpp:1254`)
   drives position almost entirely from a low-passed copy of the interpolated
   velocity. With `ease_correction_rate = 0.01` the pull back to the target is
   ~0.00016/frame — effectively zero. After the Z overshoot the velocity
   returns to ~0 (grounded snapshots) and nothing pulls the body down, so it
   floats. `filter_clamp_z_below_target` (line 1287) and the geometry-safety
   clamp (line 905) only ever push up, never down.

4. **onGround is sent but ignored for vertical settle.** The snapshot carries
   the grounded flag (`SnapshotEntity.onGround`, mixed-source broadcast from
   the client report — `server-players.cpp:1137`), and the client stores it
   (`player.ground.onGround`, line 1473), but it is never used to stop upward
   extrapolation, zero vz, or settle Z onto the authoritative height.

Contributing, not causal:

- Server broadcast position = eased server re-sim (`simBroadcastPos`,
  `server-players.cpp:728-750`) which lags landing by ~3 ticks; velocity and
  onGround come from the client report. Small vertical lag on landing.
- Duels spawn/respawn uses raw map spawn Z (~107.6 on chainofjudgement, ~34
  units above the z≈73 floor) with frequent epoch-change hard snaps
  (`server-duel.cpp:130-135`), amplifying how visible the Z error is.
- Server 3-sample capsule collision vs client full-body collision can rest at
  slightly different Z.

## Fix (implemented — Phase 1)

All client-side, no gameplay change, hot-reloadable:

1. **Gravity-aware ballistic extrapolation** (`buildReceiveTimeRender`). Z is
   projected with `vz*t + 0.5*g*t^2` instead of `vz*t`, clamped to never go
   below the last authoritative height. A guessed jump now peaks at the real
   3.11-unit apex and returns to takeoff height, so there is nothing to snap
   back from — even through a 2s blackout.

2. **Grounded settle in the ease filter.** When the authoritative snapshot says
   `onGround`, stop feeding vertical velocity into the inertia state and pull Z
   toward the authoritative height at a capped speed.

3. **Rate-limited Z ceiling (all filtered modes).** The rendered body can never
   sit more than `filter_max_z_above_target_units` above the authoritative
   height, and it converges at `filter_max_z_settle_speed` units/sec (like a
   fast fall, not a hard snap). This makes "floating while grounded"
   structurally impossible while real jumps (whose authoritative target itself
   rises) are unaffected.

New config keys under `remote_motion_smoothing` in `config/networkingconfig.json`:

```json
"filter_max_z_above_target_units": 0.75,
"filter_max_z_settle_speed": 250.0
```

No per-axis smoothing knobs were added; Z behaves correctly from gravity + the
grounded flag. `ease_correction_rate 0.01` keeps the user's preferred X/Y feel.

## Files changed

- `src/network/multiplayer-interpolation.cpp` — ballistic extrapolation, ease
  grounded settle, universal Z ceiling clamp.
- `src/config/networking-config.h` — two new `RemoteMotionSmoothingConfig` fields.
- `src/config/networking-config.cpp` — parse the two new keys.
- `config/networkingconfig.json` — new keys + tuning comment.
- `config/networking/presets/default.json` — new keys (parity).
- `src/terminal/network-commands.cpp` — `netconfig` knobs + `netstats` line.
- `docs/investigations/remote-z-jump-snap-plan.md` — this plan.

## Test plan

| Case | Setup | Pass criteria |
|---|---|---|
| Clean | no badconn, both clients | remote jump apex ≈ local (2.5-4.5u), no float |
| badconn 1 both | `badconn 1` both, active duel, chainofjudgement | no Z spike > ~5u; no sustained float; X/Y stable at rate 0.01 |
| Loss | preset 3 | spike ≤ cap; body returns to floor quickly after landing |
| Blackout | presets 6/7 on the jumping client | grounded body stays at floor; mid-jump bounded rise then settle |
| Reorder | preset 4 | no old-airborne-after-new-grounded snap |
| Duel map | chainofjudgement | all of the above incl. respawn teleports |

Instrumentation: `netinterp debug 1` on the observer -> `[NETINTERP STATE]` logs
(extrap, raw/final xyz, dz, vz); `netstats`; per-second `client-divergence`
structured log; `netconfig` to tune the new keys live.

Validation gates: build via `python build_agent.py` (Status: SUCCESS);
`devscripts/networking_config_selftest.py`; `python overseer.py` -> PASS.

## Open questions / follow-ups

- If duels raw-Z spawns still read badly, consider ground-snapping duel spawns
  (normal mode already raycasts in `serverPickSafeSpawn`) — deliberately left
  unchanged (commit `1c7dddc` chose "max action, no grounding").
- During a genuine fall, ballistic extrapolation hovers at takeoff height
  instead of continuing down (clamped at newest authoritative Z). A client-side
  floor raycast (reuse `NpcNavigation::groundHeightAt`) could clamp to the real
  floor instead; deferred as optional polish.
