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

### Schema 2: actor and authoritative tracks

Schema 2 keeps every schema 1 field and adds a combined world track. It is
recorded when the local dev-loop server is active.

- The header adds `schema:2`, `build_id`, `local_player_id`, `tracks`,
  `melee_revisions`, and start `spawn_generation` / `transform_epoch`.
- `kind:"actor_identity"` records a stable `track` and `entity` identity for
  the local player, local NPCs, remote players, and remote NPCs.
- Each `kind:"tick"` adds `server_tick` and an `actors` array of
  client-rendered actor state: transform, health, lifecycle generations,
  equipped weapon, action/phase, collision body boxes, and the rendered weapon
  contact capsule.
- `kind:"actor_lifecycle"` records spawn, despawn, death, and respawn changes.
- `kind:"server_tick"` records the authoritative server transform, health,
  equipment, melee action, collision pose, and contact shape at the
  pre-contact boundary for the same server tick.
- The client writes its track and the server writes
  `<scenario>.server.jsonl`; when recording stops the client merges both into
  one schema 2 file and deletes the sidecar.
- The schema 2 bootstrap rejects recording until the client reports gameplay
  readiness with no pending spawn acknowledgement.

Schema 1 recordings remain playable input-only scenarios.

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
- Accepts schema 1 and schema 2 at tick rate 60. Schema 2 requires the
  `server_actors` track and a completed footer.
- Rejects map or weapon-set mismatch, missing server tracks, and schema 2
  melee move-set hash/revision mismatch before frame 0.
- Restores the recorded start position, aim direction, weapon, and start
  velocity through the normal command/state path.
- Schema 2 sends `scenario_server_play` to the local server, waits for
  authoritative proxy preparation, then sends `scenario_server_play_begin`
  only after local readiness passes.
- Waits up to 5 seconds for restored position, teleport settlement,
  reconciliation, spawn activation with no pending acknowledgement,
  matching spawn generation/transform epoch, and matching equipment. A
  timeout fails the run with an actionable readiness reason.
- The server creates temporary Npc-backed proxies for recorded NPCs and remote
  players, follows the authoritative transforms/collision poses/weapon states,
  disables normal AI for them, and never proxies a connected human.
- Runs at a fixed 60 FPS via `FramePacer` so one recorded tick equals one
  simulation tick and one network input.
- Feeds the recorded `InputFrame` into local simulation and the recorded
  movement/action intent into `MpInput`.
- Applies the recorded camera and mouse/key overrides before combat so firing
  and aiming use the recorded direction.
- Compares the actual player position to the next recorded pre-tick position.
  The first error above 2 m emits `scenario.playback.divergence` and
  immediately fails playback. There is no snap-back behavior.
- Emits `scenario.playback.completed` only for a valid completed run;
  `scenario.playback.failed` reports readiness, track, or divergence failure.
  `scenario.playback.stopped` remains the user-cancel event.

## Structured events

- `scenario.record.started`, `scenario.record.stopped`,
  `scenario.record.rejected`
- `scenario.record.merged`, `scenario.record.merge_failed`
- `scenario.server.record.started`, `scenario.server.record.stopped`
- `scenario.playback.started`, `scenario.playback.start_ready`
- `scenario.playback.server_ready`, `scenario.playback.server_began`
- `scenario.playback.proxy_lifecycle`, `scenario.playback.proxy_divergence`
- `scenario.playback.divergence`, `scenario.playback.completed`,
  `scenario.playback.failed`, `scenario.playback.rejected`,
  `scenario.playback.stopped`
- `melee.client.contact_claim`
- `melee.contact.claim.accepted`, `melee.contact.claim.rejected`
- `scenario.contact.query`, `scenario.contact.weapon`
- `server.actor.spawn_ack.received`, `server.actor.spawn_ack.rejected`,
  `server.actor.spawn_activated`, `server.actor.spawn_activated.sent`
- `client.actor.spawn_ack.sent`, `client.actor.spawn_activated.received`,
  `client.actor.spawn_activated.skipped`

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

- Schema 2 targets are replayed as authoritative Npc-backed proxies; live
  connected humans are always left under their own control. A recorded human
  who is still connected causes playback to fail with `live_target_conflict`.
- Proxy health is initialized from the track and then owned by playback damage.
  The recorded health remains a reference so damage can be observed rather than
  overwritten every tick.
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

## Position divergence policy

Playback never snaps the player back onto the recorded path. The player compares
its position to the next recorded tick position; the first error above `2 m`
emits `scenario.playback.divergence` and terminates the run with
`scenario.playback.failed`, `valid:false`, and the expected/actual positions.
Continue from the wrong start would make contact and damage evidence
untrustworthy.

## Per-tick weapon replay

Each tick records `equipped_weapon` and `equipped_slot`. Playback applies the
recorded weapon result when it changes, rather than replaying raw slot edges.
This is required for recordings that switch between nothing, revolver, shotgun,
and other weapons.
