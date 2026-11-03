# Scenario input recording and playback

## Purpose

Record the local player's per-tick input while playing, then replay that input
through the same fixed-tick gameplay path to investigate movement, dashing,
weapon collisions, seams, edges, and other physics changes.

The current implementation is input playback with partial actor tracks. The
target for collision debugging is a human-recorded runtime regression that can
be replayed through the real movement/collision path, measured in the canonical
`events.jsonl` journals, and restored at reviewed checkpoints. Current behavior
and compatibility details later in this document are explicitly labeled; they
do not weaken the target contract below.

The expected physical behavior remains owned by
`docs/architecture/collision/collision.md` and
`docs/specs/movement/movement.md`; this document specifies how human-observed
failures are recorded, replayed, and judged. NPCs and human-driven actors use
the shared server movement/collision path, with input source as the distinction.

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

## Current recording storage

Implementation status 2026-11-02: new recordings also write canonical
`scenario.record.started`, `scenario.record.client_tick`,
`scenario.record.tick`, `scenario.record.checkpoint`, and completion/invalid
events through `StructuredLogger`. The client and server share one journal in
the dev loop; process role, scenario ID, and tick identities correlate them.
The legacy per-scenario JSONL file is still written as a local playback
artifact, but the canonical journal is the authoritative evidence.

Current local recordings live under:

```text
devscripts/scenarios/user/scenario-YYYYMMDD_HHMMSS[-name].jsonl
```

`devscripts/scenarios/user/*.jsonl` is gitignored. To keep a recording as a
regression, define a scenario definition JSON that references the canonical
journal and scenario ID:

```json
{"journal":"logs/2026-11-02/20261102_162354/events.jsonl",
 "scenario_id":"scn-20261102_162440-27460-quick",
 "require_checkpoint":true}
```

`scenario_play <definition.json>` loads the canonical recording. Older schema
1/2 `.jsonl` recordings remain readable.

## Single-journal collision recording target

Collision recording and playback must use the process's canonical
`events.jsonl` through `StructuredLogger`; do not add a second per-scenario
event file or another collision log sink. Client and authoritative server
processes keep their own canonical journals, linked by run, scenario, process,
actor, and tick identities. A scenario recording begins and ends with
`scenario.record.started` and `scenario.record.stopped` boundaries. Every
captured event carries a `scenario_id`/recording identity so the analyzer can
select that run's evidence from the ordinary journal. Preserve the exact
journal path, rotated segment, line, event ID, server tick, and actor identity
when reporting a failure; a line number without run and event identity is not
stable evidence.

Status 2026-11-03: canonical client capture no longer opens or writes a
per-tick scenario file. Each executable writes exactly one `events.jsonl`.
Legacy schema 1/2 files remain readable for old recordings, but new canonical
capture has no sidecar and does not merge one. A stop-time export from the
journal is not implemented yet; the journal is the recording.

The `\\` bookmark action remains a human annotation control. During an active
scenario it associates the bookmark with the recording ID, current scenario
frame, server/client tick, actor IDs, and canonical journal locations. It must
show a visible notification containing the bookmark/scenario identity and
journal location, then return directly to gameplay without opening a text
prompt. Outside scenario recording, it may continue to create an ordinary
runtime bookmark. Keep bookmarked collision recordings until a human
explicitly removes them. When journal quota is exhausted, do not prune a
bookmarked run to make room.

The human-selected scenario definition may identify a recording by run ID and
event range, but it must not duplicate the telemetry into another log. Do not
have AI invent collision scenarios for the initial implementation. A human
records and names the real failure; AI replays it, evaluates the stated
invariants, and reports the evidence. AI may suggest additional cases for
human review later.

### Detailed per-tick collision capture

Scenario capture is the high-detail mode. It records every fixed server tick
for every relevant actor and dynamic collision object, even when no anomaly is
detected. At minimum, reconstruct the following state for each actor tick:

- stable scenario track ID plus current entity/player ID, actor kind, spawn
  generation, role/preset, team, and weapon-set identity;
- player origin/root position, orientation/look direction, velocity, movement
  intent and accepted input (including primary/secondary held state), alive/HP
  state, equipped weapon, and whether an attack was actually accepted;
- selected collision backend and fallback reason, grounded/world-support
  state, position and velocity before/after the solve, and collision correction;
- final body-part/limb transforms or collision-mesh bounds, held-weapon bounds,
  and each actual actor-triangle/world-triangle contact with actor part and
  triangle IDs, world triangle/solid IDs, point, normal, penetration, and
  response;
- separate root containment, body-part contact/penetration, actor-pair contact,
  and recovery results. Do not collapse them into one `inside` boolean.

Record stable role, preset, weapon-set, and loadout facts when the actor spawns
or those facts change; include their stable IDs in each tick row so state is
reconstructible without repeating large unchanged definitions. Record each
grenade and other dynamic collision object per tick with stable identity, kind,
transform, velocity, physics state, fuse/lifetime, and contacts/impacts. Record
map identity and collision-mesh revision in the scenario header.

Root-in-solid and limb-contact results must distinguish `inside`, `outside`,
and `unknown`. `outside` is valid only when the queried geometry is a recognized
closed solid with trustworthy solid identity. A point with no matching solid
ID, an open/decorative surface, or missing collision geometry is `unknown`,
not proof of being outside. For a body part, report actual mesh/world contacts
and containment classification separately; lack of a triangle intersection
does not prove an entire limb is outside an enclosing solid.

Capture every-tick detail only in the human-recorded scenario mode initially.
It may reduce wall-clock throughput, but it must retain the fixed 60 Hz
simulation tick and must never silently drop rows. Record capture overhead and
logger drop/quota status. If the journal cannot keep the complete capture, mark
the scenario run incomplete/invalid rather than passing it. Normal live play
uses always-on low-cost anomaly monitors and event-triggered context; broad
all-actor per-tick logging can be considered after its cost and journal limits
are measured.

The recorded actor is a server gameplay actor regardless of whether its input
came from a human or NPC decision. Human and NPC intents must reach the same
authoritative movement/collision path; do not create fake network clients for
NPCs. For all relevant actors, the per-tick trace includes stable track and
runtime IDs, spawn generation, actor kind, origin, orientation/look, velocity,
accepted input/intent, equipped item and loadout identity, role/preset/team,
health/alive state, action/attack acceptance, collision backend, support,
body/limb transforms or bounds, actual triangle contacts, and separate
root/body containment outcomes. Grenades and other dynamic collision objects
receive equivalent identity, transform, velocity, physics, fuse/lifetime, and
contact records. Record values needed to reconstruct a tick, not just a summary
of the expected failure.

### Human bookmarks and checkpoint restore

A bookmark is a restore-after checkpoint, not a point that skips the recorded
failure immediately before it. Replay naturally simulates the segment from the
previous checkpoint through the bookmark, compares and records its evidence,
then restores the complete recorded authoritative gameplay state at that
bookmark and continues. Never teleport the actor back on every divergent tick.
Emit `scenario.checkpoint.restored` with checkpoint/frame/tick, restored entity
and world-state counts, and verification result. A checkpoint must cover all
relevant authoritative state needed for the next segment: actors and their
movement/action state, dynamic collision objects, world changes, and required
random-generator state. If any required state cannot be captured or restored,
mark the next segment non-comparable instead of claiming deterministic replay.

The replay must not stop merely because position differs from the reference.
It records expected/actual position, velocity, support, contacts, and
containment, continues through the current checkpoint to expose the whole
failure, then restores at that checkpoint. Preserve pre-restore divergence
evidence and label post-restore frames as a new aligned segment. A missing or
failed checkpoint restore invalidates later comparisons but does not erase
already captured evidence.

Scenario completion is event-driven: process the recording through its final
event and finish. Do not use an elapsed-time scenario timeout. A hung run stays
pending until an explicit stop or process failure; do not convert elapsed time
into a gameplay verdict. A finite recording end is a completion event.

### Capture integrity and retention

The logger must report whether every required scenario record was accepted and
written. If a write, rotation, quota reservation, checkpoint, or required state
capture fails, stop the capture cleanly, mark it incomplete/invalid, and show
the reason in the runtime journal and dev-loop status. Never silently drop a
required per-tick row or turn an incomplete trace into a pass. Retain
bookmarked scenario evidence until explicit human removal. Log segment and
tree byte limits come from `config/debuglogger.json`; the defaults are
100,000,000 bytes per segment and 1,000,000,000 bytes for the logs tree. These
are decimal byte counts. Configuration belongs in JSON, not the dashboard.

### Replay and comparison behavior

For collision diagnosis, replay recorded inputs or accepted movement intents
through the normal authoritative movement/collision path. Replay recorded NPC
decision outputs as intents when necessary to avoid waiting for variable NPC
navigation to rediscover the same event. Do not force recorded positions,
velocities, or collision results onto actors whose collision response is under
test.

At each recorded input tick, compare the actual state to the captured reference
and emit divergence events with expected/actual values. Collision replay must
continue after position divergence through the next bookmark or recording end
to finish gathering evidence; it must never snap the actor back to the recorded
path during that segment. Results after a large spatial divergence are marked
unaligned with the original geometry and cannot count as proof that later
contacts were reproduced. The checkpoint restore is the only replay snap-back.

Compare by scenario-relative input frame and stable actor track ID, while also
recording the current authoritative server tick and runtime entity ID. Replay
must restore or verify the relevant map/collision revision, mode, loadout,
actor spawns, and random seeds before calling the run comparable. If the start
state cannot be matched, report `NOT_REPRODUCED` instead of judging the fix.
At the frame where the human observed the issue, evaluate the reviewed
invariants (for example, expected speed retention, no unexpected support loss,
and no confirmed penetration). A changed trajectory in the desired direction
is evidence, but it is not an automatic pass. Later frames may be spatially
unaligned after a successful correction; report them separately rather than
forcing positional equality or treating the useful divergence as a failure.

Current playback does not snap the player back: it stops when position error
exceeds 2 m. Raising that threshold alone would only move the stop point; it
would not improve replay or correct the actor's position. The diagnostic
continue mode above is a separate target behavior.

The first validation priority is the human-recorded failure on its reported
map and geometry, not random map selection or an AI navigation soak. Add other
human-recorded failing cases as separate scenarios over time. One scenario may
bundle multiple failures only when the starting state and route make their
results independently attributable.

TAS-style save states, pause/step, and wall-clock speed controls may help restore
complex initial conditions later. They are optional future work, not a
requirement for the first reliable human-recorded collision replay. Simulation
logic remains fixed at 60 Hz even when scenario playback runs slower in wall
time.

## Legacy format and current storage (compatibility only)

The format, schema, separate scenario files, sidecars, and merged files in the
remainder of this document describe the legacy implementation and existing
recordings. They are retained for compatibility; new collision recording must
follow the single-journal target above. Do not describe legacy transform-follow
playback as collision re-simulation.

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

### Collision and final-pose tracks

Collision investigations require the `client_collision` track. It records the
canonical post-solve contacts available to the client and must identify contact
kind, source, shape, subshape, point, normal, penetration, surface id, source
entity id, and sweep velocity.

When the server or NPC final-pose contract is under test, the recording must
also carry:

- `server_collision` — authoritative contact/manifold evidence for the same
  server tick, when available;
- `npc_pose` — per-NPC root, final body-part transforms or collision-mesh
  bounds, held-weapon bounds, penetration result, last-clear state, and
  containment/reset events;
- `actor_pair` — actor-pair capsule contacts and response, separate from
  static-world triangles.

Collision tracks are evidence, not presentation. They must be written from the
same final transforms and contact owner used by gameplay.

### Expected-good and forbidden invariants

A checked-in scenario may declare named invariants instead of only checking
whether an event happened. Initial invariants:

- `no_collision_speed_collapse` — no unexplained `collision.speed_collapse`;
- `no_unexplained_position_reversal` — no repeated backward correction at a
  seam with held forward input;
- `no_grounded_seam_stall` — no grounded input-held/no-progress window;
- `straight_dash_keeps_direction` — no lateral velocity flip on a flat surface;
- `down_dash_slope_bounces` — slope contact produces positive normal velocity;
- `npc_no_static_penetration` — no final NPC body/weapon overlap beyond the
  configured standoff;
- `npc_last_clear_recovery` — a forced caught pose restores position, yaw, and
  grounded state with zero velocity.

An event that is expected in normal play, such as an intentional freeze or a
real wall stop, must be listed as an allowed exception on the scenario. The
runner must not suppress evidence globally.

### Automated baseline runs

For collision debugging, baselines begin as human-recorded real failures and
human-reviewed expected results. AI runs those recordings on the real
executable and compares the current canonical journal against the captured
reference and explicit invariants. AI does not invent collision routes for the
initial implementation. A replay fails when a forbidden event appears, a
required capture is missing, or a metric is outside its accepted range. A
change in behavior is evidence to inspect, not automatically a fix; the first
failure tick and all later unaligned state must be reported. Baseline changes
are deliberate and reviewed.

Each iteration records the scenario/recording ID, source revision, executable
build ID, run ID, iteration number, initial-state match result, first failure
tick, expected/actual metric values, and the canonical journal path plus event
identity. AI repeats the same human recording after a candidate change and
reports each attempt in order (for example, still failing, changed failure, or
all required checks passed). A pass requires complete capture and matched
starting conditions; if repeated runs disagree, report the variation instead
of claiming a stable fix. Human review follows as soon as a candidate meets the
recorded invariants.

### Recording finalization and headless soak

`scenario_endrecord` must produce a finalized recording with a footer. A
collision investigation must not accept a client-only partial file as complete
evidence. The recorder must either finalize automatically at the configured
limit/exit path or mark the file explicitly as incomplete.

Headless soak runs are optional supplemental coverage, not a replacement for a
human-recorded failure. Collision debugging runs finish on a decisive scenario
event, completion of the recorded input, or explicit stop; elapsed duration is
not a pass/fail condition. If required events never arrive, leave the run
pending until explicit stop or process failure and report that no verdict was
observed.

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

The readiness timeout and recording length cap above describe the current
implementation. The requested collision-debugging workflow does not use a
fixed elapsed-time timeout as a result: it waits for readiness/completion
events, an explicit stop, or process failure. Any storage/resource cap must
mark a recording incomplete rather than silently presenting a partial capture
as a valid scenario.

### Collision re-simulation requirement

The current schema 2 server playback follows recorded actor transforms for
NPC/remote-player proxies and disables their normal AI. That is useful for
replaying actor state and contact context, but it does not re-run those actors'
movement decisions and collision response from recorded inputs. Do not use that
mode alone to claim NPC collision behavior was re-tested.

For a human-recorded collision route to test a fix, the actor under test must be
driven by the recorded input/intent through the normal authoritative movement
and collision path. Per-tick recorded transforms, velocities, contacts, and
ground-support state are comparison evidence; playback must not force recorded
transforms onto the actor being tested. Report which actors were re-simulated,
which followed recorded transforms, and which used ordinary live AI.

The desired server-side collision recording captures per fixed tick: actor
identity/lifecycle; accepted input or movement intent; position and velocity
before and after movement; world-support/grounded state; collision backend;
contact IDs, normals, and penetration; and containment/recovery result. Compare
the replay result with the human recording, continue through the next bookmark
or recording end after divergence, and preserve the complete tick window. This
replay is a focused check of a known route; it does not replace unscripted live
gameplay.

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

This is the current runner format. The requested collision-debugging runner
must not require `timeout_seconds` for scenario completion; it terminates on
the matching completion/failure event, full recorded input consumption, or
explicit stop/process failure.

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

## Position divergence policy — legacy implementation

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

## Schema 3: client-owned scenario journals (2026-10-29)

Schema 3 makes the scenario file client-owned.

Header additions:

```json
"journal_owner": "client",
"client_instance_id": "client-1",
"server_instance_id": "server-1",
"server_journal_path": "logs/YYYY-MM-DD/run/server.jsonl"
```

Rules:

- Only the recording client writes the scenario file.
- The server writes its own `server.jsonl`.
- Server-authoritative data included in the scenario file is data the client
  received, tagged `source:"server"`.
- The client and server files are linked by the run/server/client IDs and ticks.
- The client no longer merges a server sidecar into its scenario file.
- Multiple clients can record at the same time with unique
  `scenario-YYYYMMDD_HHMMSS-client-<id>.jsonl` paths.
- Schema 1 and schema 2 remain readable for existing recordings.

A separate merge/compare command may join files for analysis, but it must not
replace the separate live journals.

## Default super-granular capture profile (2026-11-03)

`scenario_startrecord` now activates the `scenario_debug_v1` capture profile by
default. The profile is in-memory only; it does not rewrite
`config/debuglogger.json` and is restored when recording stops. It turns on the
detail needed to explain NPC and player geometry failures without a second
recording mode:

- one canonical tick row per fixed server tick, with every live actor;
- accepted input/intent, previous and current pose/velocity, desired direction,
  desired velocity, actual displacement, speed ratio, progress ratio, and
  deflection angle;
- movement contacts with world triangle and solid IDs, plus final-pose
  contacts with actor part, world triangle, solid ID, normal, surface normal,
  and penetration;
- root containment as `inside` / `outside` / `unknown`, final-pose penetration,
  last-clear/safe-point/reset state, grounding and world-contact state;
- NPC state, RNG, cooldowns, body-part bounds, and weapon bounds;
- projectiles and other dynamic collision objects;
- per-tick capture build/write/total timing.

`scenario.record.capture_probe` runs once at recording start. It requires a
live player and, when NPCs exist, at least one live NPC with accepted input,
containment classification, body parts, finite pose/velocity, and no missing
simulated body. A failed or missing probe marks the recording invalid and
stops capture. A live NPC that has no simulated body later produces
`scenario.record.capture_gap` and invalidates the run. `scenario.record.anomaly`
records speed collapse, support loss while descending, grounded no-progress,
penetration, and root-inside cases with expected and actual numbers.

Required scenario writes are synchronous and checked. A rejected write stops
capture and never produces `scenario.record.completed`.

## Future buffered bookmark mode (deferred, documentation only)

Mode 1 remains the existing instant `\` bookmark. A later mode 2 may use the
same key, prompt for an issue label in the in-game command prompt, and capture
30 seconds before and 30 seconds after the press. It must store the label and
full ISO 8601 timestamp, use a filesystem-safe timestamp in the filename, and
create files collision-safely without overwriting an existing file. It uses one
bounded rolling pre-event buffer and independent post-event windows for
overlapping bookmarks, writes only to canonical scenario journals, and obeys
the existing quota/retention rules. If the complete window cannot be retained,
the bookmark must be rejected clearly. Mode 2 is not implemented yet.

The future setting belongs in `config/scenario-recording.json`, owned by the
scenario recorder, with `bookmark_mode` defaulting to `1`. Missing or invalid
configuration keeps mode 1 behavior.

## Implementation status 2026-11-02

- `scenario_startrecord` creates a scenario ID and both processes write
  canonical scenario events to the shared run journal.
- Per-tick records include actor identity, input/intent, pre/post pose,
  grounding/support, contacts, containment, NPC state/RNG, and projectile state.
- `\` bookmarks the active scenario, writes `bookmark.created` and a server
  `scenario.record.checkpoint`, pins the run against pruning, and shows an
  in-game notification. It no longer opens the terminal.
- `scenario_play <definition.json>` loads the canonical recording, replays
  local input and NPC intents through the normal movement/collision path, logs
  divergence and continues, and restores actor checkpoints at bookmarks.
- `config/debuglogger.json` owns `storage_limits.per_segment_bytes` and
  `storage_limits.total_log_tree_bytes`; limits apply at process launch.
- `devscripts/scenario_replay_report.py` reports the verdict and exact journal
  path, line, event ID, tick, first divergence, and checkpoint restores.
- `devscripts/dev_loop_gui.py` renders a native Tkinter dashboard when
  available, with a terminal fallback.

Known limits of this slice: projectile/world-mutable checkpoint restoration is
reported as `unrestored_components`, not silently claimed. A recording whose
live starting life cannot be matched is reported as `NOT_REPRODUCED`/failed
readiness rather than judged as a fix. A forced-quota live run is verified: a
32,768-byte segment limit produced server and client invalid records and no
completed marker.

## Multi-client matrix

The dev-loop must support one server and N clients. Each client has its own:

- command queue
- scenario/role
- assertions
- journal
- network profile

The runner waits for all clients to become ready, synchronizes start, and
collects per-client and server results separately. Matrix scenarios live under
`devscripts/scenarios/matrix/`.
