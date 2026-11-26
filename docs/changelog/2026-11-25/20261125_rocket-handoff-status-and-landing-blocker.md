# Rocket League mode: correct the stale handoff and record the real blocker

## Request

The user asked to continue `docs/handoffs/20261125_rocket-league-mode-handoff.md`
in the vehicle-ball-contact wake.

## Change

- Rewrote the handoff's status, open defects, proven/unproven list, bindings,
  journals, environment facts, and next actions so they match the verified
  build/runtime state instead of the superseded 23:33 session.
- Recorded the controller and keyboard bindings and the expected behaviour of
  each (R2/L2, left/right stick, Cross/Square/Circle/R1/L1/Triangle/R3; keyboard
  WASD, mouse buttons, Shift, Space), noting that they already match the user's
  requested binds.
- No source or config code was edited. No build was requested. No process was
  started, stopped, or restarted.

## Evidence

- Profile now applies: `vehicle.profile.applied` = 2 in fresh server run
  `20261125_233342` (`rl_soccar_freeplay`, `octane_rl`, gravity 6.5).
- Free-play launch works: `dev-launch-modes.json` mode 60 has
  `require_mode_start=false`; run `20261125_234424` launched and the scenario ran
  all steps.
- `onlinemodes.json` ordering is fixed (`vehicle_freeplay` appended last; mode 50
  maps to `vehicle_arena`).
- Command-to-runtime trace in `20261125_233342`:
  client `vehicle.input.local` `jump_held=true` at t=67.70, `roll=0.9` at t=68.46,
  `pitch=-0.9` at t=69.46, `yaw=±1` later; server `vehicle.jump` at t=67.94,
  `vehicle.airroll` at t=68.80 (roll 0.9) and later yaw/pitch variants,
  `vehicle.jump.double`, `vehicle.dodge`, `vehicle.flip`, `vehicle.jump.apex`;
  server `vehicle.state` `airborne=true`, `air_control=true`,
  `up_dot_world_up` 1.0→−0.59, `angular_speed` capped at 5.50.
- Real remaining blocker: run `20261125_233748` shows a 59.6 s continuous stretch
  (t=107.9–167.6) of `airborne=true, grounded_wheels=0, speed<0.5` while the car
  tumbles on its roof/side; `vehicle.bounce` fires ~45×/s. No ground-jump reset
  occurs because the reset requires all four wheels.
- Evidence-integrity gap: run `20261125_234424` reported `vehicle.jump count=6`
  and `airroll 66` although the retained server journal had no `vehicle.jump`
  after `scenario.started`; scenario assertions are not windowed.

## Limit

The landing/settle defect is diagnosed from journals only; it is not fixed here.
Jump/air-roll/dodge proof is from a fresh server run, but the dev-loop scenario
verdict that reused a retained server is not trustworthy until scenario
assertions are scoped to the scenario window.
