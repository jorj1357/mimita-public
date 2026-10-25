# Scenario input recording and playback

## Purpose

Record the local player's per-tick input while playing, then replay that input
through the same fixed-tick gameplay path to investigate movement, dashing,
weapon collisions, seams, edges, and other physics changes.

This is input playback, not a full deterministic world replay. NPC state,
remote players, and server corrections can differ between the recording and
playback runs. Run playback on the same map, mode, weapon set, and local dev
server when comparing collision behavior.

## Commands

```text
scenario_startrecord [name]
scenario_endrecord
scenario_status
scenario_play <path.jsonl>
scenario_stop
```

- `scenario_startrecord` arms recording. The next fixed tick writes the header
  and frame 0.
- `scenario_endrecord` writes the footer and closes the file.
- `scenario_play` loads, validates, restores the start state, and begins
  asynchronous playback.
- `scenario_stop` cancels an active playback.
- `scenario_status` reports the active recording/playback path and frame counts.

Recording and playback are mutually exclusive. Recording is rejected during
visual replay or playback. Playback is rejected during recording.

## File location and naming

Recordings are local-only and live under:

```text
devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS[-name].jsonl
```

`devscripts/scenarios/user/*.jsonl` is gitignored. To keep a recording as a
regression, move it out of `user/` and reference it from a checked-in scenario.

## Format

Append-only JSONL, one line per fixed 60 Hz tick, flushed every 6 ticks.

Header:

```json
{"kind":"header","schema":1,"created_utc":"2026-10-25T01:24:22Z",
 "tick_rate":60,"map":"assets/maps/funworld3.glb","mode":"sandbox",
 "weapon_set":1,"player":"NPC Dev","avatar":"clown","source":"human",
 "start":{"position":[84.376,30.715,353.069],"velocity":[1.138,1.066,0],
  "yaw":-90.0,"pitch":10.0,"cam_forward":[-4.3e-8,-0.9848,0.1736],
  "grounded":true,"health":100,"equipped_weapon":"nothing",
  "equipped_slot":18,"movement_simulation_tick":744}}
```

Tick:

```json
{"kind":"tick","frame":0,"client_tick":744,
 "wish":[0.0,-0.0],"move_world":[0.0,0.0],
 "jump":false,"jump_pressed":false,"dash_pressed":false,
 "down_dash_pressed":false,"freeze_held":false,"freeze_pressed":false,
 "crouch_held":false,"reload_pressed":false,
 "primary_held":false,"secondary_held":false,
 "shield_held":false,"grab_held":false,
 "look_yaw":-90.0,"look_pitch":10.0,
 "cam_forward":[-4.3e-8,-0.9848,0.1736],
 "equipped_weapon":"nothing","equipped_slot":18,
 "position":[84.376,30.715,353.069],"velocity":[1.138,1.066,0],
 "health":100,"grounded":true,"cam_yaw":-90.0,"cam_pitch":10.0}
```

Footer:

```json
{"kind":"footer","ended_utc":"2026-10-25T01:24:26Z",
 "frames":163,"duration_seconds":2.7,"stop_reason":"command"}
```

`position`, `velocity`, `health`, and `grounded` are context for divergence
reporting. Playback never forces them.

## Recording behavior

- Captures `InputFrame` at the fixed-tick boundary in
  `src/engine/engine-tick-replay.cpp`.
- Captures movement, camera look, jump, dash, down-dash, freeze, crouch,
  reload, primary/secondary mouse, shield/grab, and weapon-slot held mask.
- Records `equipped_weapon` and `equipped_slot` per tick so weapon changes are
  reproduced from their result.
- Caps a recording at 36,000 frames (10 minutes) and stops with
  `stop_reason="limit"`.
- A crash can leave one partial trailing line; the loader ignores only the
  final malformed line.

## Playback behavior

- Loads and validates the whole JSONL once; no per-tick JSON parsing.
- Requires schema 1 and tick rate 60.
- Rejects map, mode, or weapon-set mismatch.
- Restores the recorded start position, aim direction, weapon, and start
  velocity through the normal command/state path.
- Waits up to 5 seconds for the client and server to agree on the start
  position and for the teleport to settle before frame 0.
- Runs at a fixed 60 FPS via `FramePacer` so one recorded tick equals one
  simulation tick and one network input.
- Feeds the recorded `InputFrame` into local simulation and the recorded
  movement/action intent into `MpInput`.
- Applies the recorded camera and mouse/key overrides before combat so firing
  and aiming use the recorded direction.
- Compares the actual player position to the next recorded pre-tick position,
  reports `max_position_error_m`, and emits `scenario.playback.divergence` on
  the first error above 1 m.
- Emits `scenario.playback.completed` on finish and `scenario.playback.stopped`
  on cancel.

## Structured events

- `scenario.record.started`
- `scenario.record.stopped`
- `scenario.playback.started`
- `scenario.playback.start_ready`
- `scenario.playback.divergence`
- `scenario.playback.completed`
- `scenario.playback.rejected`
- `scenario.playback.stopped`

## Dev-loop integration

A scenario can use a first-class playback step:

```json
{
  "id": "play_recording",
  "type": "input_playback",
  "path": "devscripts/scenarios/user/scenario-20261025_012422-smoke3.jsonl",
  "timeout_seconds": 120
}
```

The runner reads the recording header before launch, uses its map and weapon
set, ensures a live ready client on a non-stale build, sends `scenario_play`,
and waits for `scenario.playback.completed` with the step timeout.

A scenario that requests playback is relaunched even if a run is already alive
because the recorded map/weapon set must be guaranteed.

## Scenario reliability

The earlier scenario failure reused a run whose server was alive but whose
client was gone. `scenario.started` recorded `client_pids: []` and
`client_ready: false`, so commands timed out.

The runner now relaunches when:

- `client_ready` is false,
- `client_pids` is empty,
- the launch mode differs,
- the running build differs from the latest published build,
- the build is stale,
- or the scenario requests a weapon set.

Playback scenarios also read the recording header and force the recorded map
and weapon set. The runner waits for the long playback completion event instead
of the 15-second per-command timeout.

## Known limitations

- Not a full deterministic world replay. NPCs, remote players, and server state
  can differ between record and playback.
- Terminal-only actions that do not pass through `InputFrame` or the normal
  mouse/keyboard path are not guaranteed to replay. Normal mouse/keyboard play
  is the supported path.
- Full melee combo fidelity is not claimed; shield/grab fields are recorded,
  but melee-specific edge cases may need a follow-up.
- Recordings are local by default and should be promoted deliberately when used
  as regression evidence.

## Terminal handoff

`scenario_startrecord` closes the terminal after it starts, so recording captures
gameplay input immediately. `scenario_play` also closes the terminal before
frame 0. Reopen it with the normal terminal key to run `scenario_endrecord` or
`scenario_stop`.

## Practical collision-test guidance

For collision comparisons, record a short segment starting from a stationary,
grounded position, and start a new recording for each probe (one dash, one seam
crossing, one slope junction). Longer segments accumulate server/NPC/state
differences and make the position error less useful. Inspect the collision
events around the probe rather than expecting the whole trajectory to match.

## Position drift snap

During playback the live server/world state can differ from the recording. The
player compares its position to the next recorded tick position. If the drift is
`>= 5 m`, it snaps the local actor to that recorded position, restores the
recorded velocity, and emits `scenario.playback.snap`. This keeps the recorded
inputs applied at the recorded map location instead of doing the right moves in
the wrong place.

## Per-tick weapon replay

Each tick records `equipped_weapon` and `equipped_slot`. Playback applies the
recorded weapon result when it changes, rather than replaying raw slot edges.
This is required for recordings that switch between nothing, revolver, shotgun,
and other weapons.
