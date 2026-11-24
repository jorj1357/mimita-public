# Rocket League reference vehicle physics

**Date:** 2026-11-24 UTC
**Agent:** vehicle-rl-physics
**Issue:** `issue-e5a086b0a441`

## Purpose

Replace MiMITA's guessed arcade car numbers with the documented Rocket League
reference, and build the controls as general rules that interact instead of
scripted tricks. Vehicle Arena becomes the first Rocket League reference
consumer.

## Reference sources

Every constant in `config/vehicle-profiles/rl_soccar.json` is tagged with its
origin. No value was invented.

- `https://wiki.rlbot.org/v5/botmaking/useful-game-values/` — gravity 650 uu/s²,
  car mass 180, ball mass 30, ball radius 91.25 uu, drag 0.03056 s⁻¹,
  restitution 0.6, car max 2300 uu/s, supersonic 2200, unboosted 1410,
  boost accel 991.667/1058.333 uu/s², air angular acceleration 12.46/9.11/38.34
  rad/s², max angular velocity 5.5 rad/s.
- `https://wiki.rlbot.org/v5/botmaking/jumping-physics/` — jump Δv 291.667 uu/s,
  sustained 1458.33 uu/s² for 0.2 s, sticky 325 uu/s² for 3 ticks, double-jump
  window 1.25 s.
- `https://www.smish.dev/rocket_league/aerial_control/` — `alpha = A·u − D·w`
  per axis, input-scaled pitch/yaw damping, combined ω clamp.
- `https://www.smish.dev/rocket_league/ball_simulation_3/` — car/ball
  inelastic collision plus Psyonix's extra non-conserving ball impulse.
- `https://github.com/ZealanL/RocketSim` (`src/RLConst.h`, `Car.cpp`,
  `Ball.cpp`) — `FLIP_*` constants, dodge direction cancellation, independent
  flip Z damping, dodge speed scaling, extra-impulse curve, suspension,
  autoflip/autoroll, and the fixed-point torque conversion
  (`2π/65536·1000 ≈ 0.0959931`).

`1 uu = 1 cm`, so every RL value converts to SI by dividing by 100.

## What the rules are

- **Air control** is per-axis `alpha = A·u − D·w`, then one combined
  `|w| <= 5.5` clamp. There is no shared torque budget; rolling never weakens
  pitching.
- **Jump** is one Δv impulse, a sustained acceleration while held, and a
  three-tick adhesion term. Apex is an output of gravity and jump power.
- **Dodge** activates from `|pitch| + |yaw| + |roll|` but takes direction from
  the signed `-pitch` and `yaw + roll` components. Opposing yaw and roll
  therefore consume the dodge while producing zero direction, zero torque and
  zero impulse. The independent flip vertical damping still runs, which is the
  Rocket League stall. No `performStall()` exists.
- **Flip cancel** scales the flip's pitch torque by `1 − |pitch input|` when the
  stick opposes it; the linear impulse already granted is kept.
- **Flip reset** is the same all-four-wheel rule on any surface, including the
  ball.
- **Car→ball extra impulse** is the one deliberate non-conserving kick, applied
  to the ball only, once per contact tick.

## Source changes

- `src/vehicles/vehicle-profile.h` / `.cpp`: new `flip`, `ground`, `ballHit`,
  `settle`, `gravity`, and provenance fields; piecewise-curve parsing.
- `src/vehicles/vehicle-system.cpp`: replaced the impulse air model with the
  per-axis torque model; delta-v jump with sustained force, adhesion, and the
  Rocket League jump-reset pad; dodge program with direction cancellation,
  pitch cancel, and flip Z damping; flip reset from wheel-to-ball contact;
  surface-aware ground drive; analog powerslide; car speed clamp; settle and
  sleep; metric telemetry; per-profile gravity for the car; pushes the
  car→ball impulse parameters to the contact owner.
- `src/vehicles/vehicle-types.h`: normalized `pitch`/`yaw`/`roll` intent,
  air-roll modifier, dodge/flip state, metric accumulators.
- `src/input/gamepad-input.*`: PS4 layout (Cross jump, L1 boost, R1
  powerslide/free air roll, Square/Circle directional air roll, Triangle ball
  cam, R3 look behind), trigger upper deadzone, `pitch`/`swivel` axes.
- `src/physics/physical-entity.*`: configurable Rocket League car→ball extra
  impulse at the contact owner.
- `src/gamemode/arena-ball.cpp`, `map-config.*`: per-map ball gravity
  (`gravityMps2`) converted to an entity gravity scale; the map's `maxSpeed`
  default is 0 (uncapped).
- `src/vehicles/vehicle-system.cpp` also disables template engine torque for
  arcade profiles so the Rocket League acceleration curve owns the car.

## Data changes

- `config/vehicle-profiles/rl_soccar.json` (new) — the reference profile.
- `config/vehicles/octane_rl.json` (new) — 180 kg, 1.18 × 0.842 × 0.362 m.
- `config/maps/arena1.json` — 102.4 × 81.92 × 20.48 m, goal 17.855 × 6.428 m,
  kickoffs at ±40 m, a collision floor under the whole extent, the Rocket League
  boost-pad layout (6 big + 28 small), and `octane_rl` vehicles.
- `config/gamemodes/vehicle_arena.json` — profile `rl_soccar`; arena NPCs off
  for this pass (the user asked to focus on the human client).
- `config/input/gamepad.json` — PS4 bindings and trigger upper deadzone.
- `docs/specs/vehicles/vehicles.md` — the normative Rocket League reference
  profile section replaces the old arcade-arena section.

## Runtime evidence

Build: game generation **600**, DLL
`build/game/generation-600/game-g600.dll`,
sha256 `53f23d0be0ccb64752a54cb11cc4f5f6d3a23cf04bf428ff47bcf62972de72cc`.

Run `20261124_184753_rl-physics-5`, server journal
`logs/2026-11-24/20261124_184753_rl-physics-5/server-01/events.jsonl`.

- `vehicle.profile.applied` for both cars: `profile=rl_soccar`,
  `template=octane_rl`, `gravity_mps2=6.5`, `jump_delta_vel_mps=2.9167`,
  `steering_max_deg=30.57`, `profile_source=rlbot-wiki+smish+rocketsim`.
- `arena.ball.spawned`: radius `0.9125`, mass `30`, position z `0.9125`.
- Driver seated (`vehicle.auto_seat` distance 20.7 m), client reported
  `has_local_driver=true`.
- `vehicle.input.server` with `throttle=1.0` and a changing position.
- `vehicle.telemetry` (158 samples, vehicle 1): max `speed_mps=9.92`
  (`35.7 km/h`), `distance_travelled_m=47.81`, 22 airborne samples,
  max `angular_speed=5.5` (the clamp is reached), `apex_height_m` peak `1.967`.
- `vehicle.jump`: `delta_vel_mps=2.9167`, `predicted_apex_m=0.654`.

## Not proven yet

- **Jump apex is short of the reference.** Measured rise ≈ 1.78 m against an
  RL full-jump ≈ 2.6 m. The immediate impulse is correct; the sustained portion
  is delivering about two-thirds of its target. This needs a tick-level trace of
  `vehicle.jump` through the sustain window before any claim of RL jump parity.
- **Stall, flip cancel, flip reset, and the car→ball extra impulse have no
  runtime proof.** They are implemented on the documented rules but the
  keyboard-only harness cannot produce `yaw + roll = 0` or a wheel-on-ball
  contact. They need the directional-air-roll input surface.
- **The 60 Hz world clock is unchanged.** The planned 120 Hz car/ball substep
  and swept-sphere CCD were not part of this batch, so a fast ball is still
  bounded by the existing sweep and the global object speed clamp.
- **The arena visual mesh is still the old 60 × 41 m `arena1.glb`.** Collision
  now covers the full RL extent, so gameplay works, but the visible arena does
  not yet match. This is map-authoring work.
- Pinch experiments (tick-rate convergence, energy audit) are not built yet.

## Risk

The `vehicle-types.h` and `physical-entity.h` edits fan out across the game
module (a full DLL rebuild). The arcade profile now disables template engine
torque; any mode that selects `rl_soccar` gets Rocket League ground drive
instead of the drivetrain.
