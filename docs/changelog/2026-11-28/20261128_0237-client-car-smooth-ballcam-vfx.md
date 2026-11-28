# Client-owned car, smooth interpolation, ball prediction, vehicle SFX/VFX

- Timestamp: `2026-11-28T07:37:00Z`
- Agent: `rocket-rl-settle`
- Issues: `issue-a4958b2df5e5`, `issue-a3e088b8181c`
- Result: local car is client-owned and never snap-corrected; render
  interpolation is 120 Hz; the owner's pose is relayed to other clients; ball
  cam, `va_car_reset`, proximity fade, 0.5 stiffness and vehicle SFX/VFX are
  live.

## User request

- Wall hits must not jitter; the local car must be 100% smooth, never jitter
  between two positions; the ball and other cars too.
- Client-authoritative cars: each client sends its pose, missing packets show a
  ghost/disagreement effect, extrapolate, never snap to the server unless a
  major disagreement, and prefer report/vote-kick over correction.
- Ghost ball when the ball stream is stale.
- Ball cam must look at the ball.
- Proximity fade off by default in vehicle arena mode.
- `va_car_reset` for the car the player is in.
- Placeholder VFX and SFX with replaceable WAV paths for boost, jump, air jump,
  wall bump, demolition, ball hit and goal.

## Changes

### Client-owned car and smoothness

- `enterLocalDriver` now enables prediction for arena cars (the old
  `!serverAuthoritative` gate kept the local car a 20-30 Hz mirror).
- `PACKET_VEHICLE_POSE` was added, but the resident server executable does not
  dispatch new packet ids; the pose therefore rides the already-dispatched
  `PACKET_VEHICLE_INPUT` as a trailing `VehiclePoseBlock`
  (`VEHICLE_INPUT_HAS_POSE`). The server adopts it in
  `handleVehicleInputPacket` after finite/12 m/40 m/s validation.
- `mpProcessPhysicalEntityStatePacket` no longer moves a `clientPredicted`
  chassis. Server pose divergence is logged (`vehicle.pose.server_error`,
  `vehicle.pose.server_divergence`) and never applied. `hard_resync` is gone.
- The server relays the owner's pose (`PACKET_VEHICLE_POSE`) to every other
  spawned player. A receiving client marks the car `poseRelay`, applies the
  owner's pose with timed interpolation, and ignores the server's physical
  entity state for that car.
- `renderAlpha()` used the 1/60 constant against a 1/120 accumulator; it now
  matches the physics step. The camera follows
  `physicalEntityRenderPosition` (interpolated) instead of the raw tick pose.
- Replicated bodies (remote cars, ball, crates) interpolate between the last
  two network poses over wall clock and extrapolate up to 250 ms when a packet
  is late.
- The ball is locally predicted and reconciled 15%/packet; only a >4 m
  reset/goal snaps it. A stale ball shows `vehicle_ghost_ball_fx` and plays
  `vehicle/ghost`.
- The server's `VehicleStatePacket` no longer overwrites engine/gear/boost/
  control fields on the local predicted car.

### Walls and containment

- Arena2 walls are 2 m thick (inner face unchanged) and the upper wall profile
  overlaps 1 m into the lower wall; the floor is 3 m thick and extends 4 m
  beyond the walls, and the lower wall profile reaches below the floor.
  Regenerated the map and reference.
- `vehicle.out_of_world` containment projects a car back to the nearest legal
  face, zeroes the outward velocity, and logs it. It fires only beyond the solid
  outer faces (52.15 / 41.9 / -3.0), so internal penetration is left to the
  solver and does not create a containment snap.
- `vehicle.wall.contact` is rate-limited to 0.5 s.

### Camera, UI and reset

- The replicated arena ball is marked in the spawn packet (`reserved` bit 2),
  so the client ball keeps `persistenceId="arena-ball"`. Ball cam finds it,
  uses the interpolated render pose, and falls back to the car nose instead of
  freezing. `vehicle.camera.ballcam` records `ball_found`/network id.
- Proximity fade is forced off while seated, and restored on exit/despawn; the
  mode JSON also declares `presentation.proximity_fade: "off"`.
- `va_car_reset [blue|orange]` resets the seated car to the team kickoff, zeroes
  velocity/flip state, refills boost, and sends a server command so the
  authoritative copy resets too.
- HUD km/h uses the 3D velocity (wall climbs no longer read 0).
- The seated net overlay reports `n/a (client-authoritative car)`.

### VFX / SFX placeholders

`devscripts/gen-vehicle-placeholder-sfx.py` writes synthesized WAVs. Replace
the files directly; the game resolves `vehicle/<name>` to
`assets/sound/vehicle/<name>.wav`.

| Cue | Path |
|---|---|
| Boost | `assets/sound/vehicle/boost.wav` |
| Jump | `assets/sound/vehicle/jump.wav` |
| Air jump | `assets/sound/vehicle/airjump.wav` |
| Dodge | `assets/sound/vehicle/dodge.wav` |
| Land | `assets/sound/vehicle/land.wav` |
| Wall bump | `assets/sound/vehicle/wall_bump.wav` |
| Ball hit | `assets/sound/vehicle/ball_hit.wav` |
| Goal | `assets/sound/vehicle/goal.wav` |
| Ghost ball | `assets/sound/vehicle/ghost.wav` |
| Demolition (ready, no mechanic yet) | `assets/sound/vehicle/demolish.wav` |

Effect labels: `vehicle_jump_fx`, `vehicle_airjump_fx`, `vehicle_boost_fx`,
`vehicle_wall_fx`, `vehicle_ball_hit_fx`, `vehicle_goal_fx` (blue goal beam),
`vehicle_ghost_ball_fx` (purple stale-ball ring).

### Coordination

- Build policy: when host CPU/RAM is over the limit but no peer is working and
  no path is held, the request auto-approves as
  `resource_pressure_idle` instead of stalling the tree. This is the requested
  "auto yes for a busy/abandoned host" rule.
- `agent_coord.py` child Python helpers now use `CREATE_NO_WINDOW` +
  hidden startup info so no Python console flashes from the prompt plugin.
- Stale payload servers from the 01:09/01:25 runs were stopped with the user's
  authorization after more than an hour without a client.

## Evidence

Builds: generation 811 (first successful full compile), 812 (pose on input +
no snapshot snap), 815 (thick floor), 816 (outer-face containment). Run
`20261128_073416`, mode 60, arena2, gen 812:

| Check | Evidence |
|---|---|
| Prediction on | `vehicle.prediction.enabled` 1 on client |
| Pose adopted | `vehicle.pose.accepted` 10 server samples, `source=input`, displacement 0.9-5.5 m |
| No local snap | `vehicle.pose.hard_resync` 0 |
| Divergence logged | `vehicle.pose.server_divergence` 37 client samples (logged, not applied) |
| Remote relay | run `20261128_080510` (gen 814), client-02: `vehicle.pose.relayed` 17+ samples, owner player 1, speed 17.56 m/s |
| Ball cam | `vehicle.camera.ballcam` `ball_found=true`, `ball_network_id=1` |
| Reset | `vehicle.reset` on server and client, position `(-40, 0, 0.25)`, team 0 |
| Boost/jump feedback | `vehicle.boost` 2, `vehicle.jump` 1 on both sides |
| Stiffness 0.5 | run `20261128_080939`, gen 815: tau 0.1539 s; 8.22 m/s -> 1.36 m extra vs 1.27 expected; 8.33 -> 1.25 vs 1.28 |
| Wall containment | run `20261128_081709`, gen 816: 17 kicks, 17 wheels, 0 `vehicle.out_of_world`; floor 3 m + 4 m margin |
| Fade | run `20261128_074000`: `camera.proximity_fade.world` only at t=3.45/4.68 before the seat; prediction enabled at t=4.71 and no fade event afterwards | proven off while seated |

## Follow-up pass: landing stability, wall containment, pads and axes

### Landing / bounce

- Auto-roll is now roof-only (`upDotWorldUp < -0.5` + fresh contact +
  throttle). The old partial-wheel/body-contact test fired 118 times during
  throttle driving and was the constant left air roll.
- Octane damping raised from 1200 to 1900 Ns/m per corner (critical ~1800), so
  a landing no longer pogos.
- Chassis collision centre raised 3 cm (`-0.0345` -> `-0.0045`) so the box
  bottom has 0.126 m clearance against the 0.12 m suspension travel; a hard
  landing cannot bottom the box on the floor and bounce the body.
- Body restitution stays 0.30 for chassis impacts.

### Wheels and slopes

- Wheel rendering uses `physicalEntityRenderTransform(*entity)` and its
  rotation, the same interpolated pose as the car body; the wheels no longer
  lead the body by a physics tick.
- Wheel contact normals are low-passed when the new normal is within 0.7 dot,
  removing the ramp-profile triangle steps from the suspension/friction
  direction.

### Wall / out-of-world

- The vertical wall band now starts where the lower ramp profile ends (4.55 m),
  instead of overlapping the ramp volume from 3.55 m. A car on the ramp at
  z~3.86 was fully inside the band box and got pushed down/out.
- Invisible collision backstops behind the long and back walls (opacity 0,
  42.4 / 52.4 inner faces) stop a car that slips behind a curved profile. The
  containment guard now only fires beyond 43.0 / 53.0 / -3.0.
- Gen 817 fuzz: 16/16 kicks on wheels, 0 side/nose/tail, 0
  `vehicle.out_of_world` on both client and server.

### Boost pads, boost trail and axes

- Pads are rendered as orbs: small = 1 m yellow, big = 2 m orange, depleted =
  grey. A `PACKET_BOOST_PAD_STATE` (10 Hz) sends depleted indices and remaining
  centiseconds; the arena HUD projects `%.2f` above each grey pad.
- A white orb is spawned behind the car every other 120 Hz tick while boost is
  held.
- Six coloured axis arrows (F red, B blue, R green, L yellow, U white, D
  magenta) plus a HUD legend; `vehicle_axes on|off`, default on.

Evidence: gen 819 run pending user visual check; fuzz and packet evidence in
the run journals. The live run `20261128_082433` is the user's and was left
running; restart it to pick up gen 819 if the pads/axes are not visible.


### Sticky force sign and roof-only auto-roll (gen 820)

- `src/vehicles/vehicle-system.cpp` applied the sticky impulse along
  `+groundUp` (the contact normal, away from the surface). It lifted the car
  off the floor by 0.5 g every tick and pushed it away from walls/ceilings.
  The impulse is now `-groundUp` (into the surface).
- Auto-roll now requires inverted **and zero wheel contacts** (a real roof
  rest) plus chassis contact and throttle. Driving on the ceiling has wheel
  contact, so auto-roll no longer twists the car mid-slide.
- A first flat-drive check accidentally attached its client to the old fuzz
  server `20261128_084634`; the car was falling from the ceiling and bounced at
  22.9 m/s. That is a fuzz drop, not a flat-drive result. The leftover fuzz
  server and the contaminated run were stopped. A clean gen-820 flat/drop
  measurement is still pending the user's restarted run.

## Remaining

- The backstop keeps the car in the world, but a car can still briefly sit
  inside the wall volume at the curved-profile seam; the solver owns that
  penetration.
- The fuzz probe still counts its own respawns (`escapes`) when a kick leaves
  the playfield; those are probe artifacts, not containment failures.
- SFX are presence-tested by code path only; audio is not aurally verified.
- Demolition has a ready WAV path but no demolition mechanic.
- The live user run needs a restart to pick up gen 819 for visual pad/axis
  verification.
