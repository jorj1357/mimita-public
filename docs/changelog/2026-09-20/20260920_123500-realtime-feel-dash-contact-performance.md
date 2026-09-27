# Real-time feel: down-dash contact proof and frame-window logging

Date: 2026-09-20 12:35 EDT (UTC 2026-09-20T16:35:00Z)
Status: BUILD_SUCCESS; LIVE_VISUAL_ACCEPTANCE_PENDING

## Changes

- Kept the existing hot movement, collision, effect, audio, and JSONL owners.
- Added generic `audio.play` output for successful dash and down-dash actions.
- Added unique per-actor/per-tick action IDs and `MOVEMENT/movement.action`
  records with action, availability, position, and velocity.
- Added `MOVEMENT/movement.contact_ability` records proving contact, grounded,
  collision, down-dash availability before/after, and restoration.
- Restored the v2.0.6-style grounded down-dash launch in hot local movement;
  airborne down-dash keeps the configured downward impulse.
- Added bounded JSON `PERFORMANCE/performance.frame_window` records every 60
  frames, sorted by scope self time, using the existing per-frame ring buffer.
- Enabled the performance category in `config/debuglogger.json`.

## Evidence

- `python build_agent.py` -> `BUILD SUCCESS`.
- Executable: `C:\mimita-priv-v8\mimita-20260920T122848.exe`.
- Hot DLL build -> success.
- `mimita-20260920T122848.exe --live-code-selftest` -> `PASS`.

## Runtime acceptance still required

In a live session, press Q on the ground and inspect the active run's
`events.jsonl` for matching `collision.touch`, `movement.contact_ability`, and
`movement.action` records. Confirm `grounded=1`, `touch=1`,
`ability_down_dash_after=1` after contact, then a later `down_dash_fired=1`
with a positive post-solve vertical velocity for the grounded launch.
