# Handoff: finish the Rocket League mode

Written 2026-11-27 by `rocket-league-feel` / `rocket-league-input2`.
Everything below is reproducible from this checkout; nothing is committed to git
(the working tree is the artifact). Read `AGENTS.md`, then
`docs/specs/gamemodes/vehicle-arena.md` (sections 9.0-9.1, 12, 12a, 13-17), then
the issue index at the bottom.

## 1. Where the mode stands

### Proven with journal evidence

| Feature | Evidence |
|---|---|
| Launch stability (5 clean launches, no crash) | gen 693, runs `20261126_161031`-`161313` |
| Dodge is a full 360 (`rotation_deg_total 359.85`) | `rocket_dodge_360_v1` |
| Dodge cancel stops the rotation (`cancelled=true`, 162 deg) | `rocket_dodge_cancel_v1` |
| Straight-line driving (`yaw_deg 0.00` over 70 m) | run `20261126_190324` |
| NPC vs NPC 1v1, 5-minute timer, 7 goals, 4-3 | run `20261126_200435` |
| Aerial goal with jump+boost+air roll+dodge (`aerial=true`) | run `20261126_195128` |
| Aerial save (`arena.save aerial=true`, aimed 0.93 -> -0.55) | run `20261126_194927` |
| Roof-or-wheels fuzz (`wheels 52/55`, `tilted 0`) | run `20261126_201305` |
| Spin changes the bounce (mirrored +/-60 rad/s) | runs `20261126_201529`, `_201615` |
| arena2 loads and plays (goal at 12.7 s) | run `20261126_231251` |
| Keyboard drives (W -> throttle 1.0, 8.45 m/s; A turns left) | run `20261127_042814`, gen 767 |
| Pad readable and standard-mapped (6 axes, 15 buttons) | run `20261127_041915` |

### Open / not started

1. **Freestyle flip reset**: the bot program exists but never reaches a reset
   (`issue-1ffbfffaefa0`). Needs a closed-loop aerial position controller.
2. **Ball physics parity**: loses speed ~5x faster than RL and rests ~10 cm high
   (`issue-2ba48bb85d4a`). Fixes feel and freestyle tuning.
3. **Side-rest guarantee**: rule added, fuzz classification fixed, but
   `side_rests == 0` is not yet proven on the current build.
4. **Cruise proof**: throttle+boost forever on arena2, no jitter, only
   wheels/roof settlements. Not written.
5. **Tick VFX**: ball touch red 0.1 m, boost yellow 0.3 m at the exhaust,
   powerslide dark purple 0.3 m under the car, dodge bright pink, jump turquoise
   above the roof; all 1 tick, alpha 0.5.
6. **Hitbox debug view**: per-face bright colours (front red, back blue, right
   green, left yellow, top white, bottom magenta) plus a big forward arrow.
7. **Free play pass**: D-pad left throws the ball at the player,
   `speed = clamp(12 + 0.35 * distance, 12, 50)`.
8. **Camera JSON**: all Rocket League camera settings in one file. Requested:
   distance 220, height 90, fov 110, angle -7, swivel 10.0, transition 10.0,
   stiffness 1.0.
9. **1v1/3v3 vs a pro NPC**: the probe bots are evidence tools, not opponents.
10. **arena2 polish**: goal recess back curve measured? Ceiling/upper ramp on the
    back walls uses the side-wall profile (assumption, see §5).

## 2. How to run and test

- Launch: `python devscripts/dev-loop.py --request launch --launch-mode <n>`.
  Modes: `60` free play on **arena2**, `62` ball probe, `64` 1v1 vs NPC,
  `65` settling fuzz (`MIMITA_VEHICLE_FUZZ=7`), `66` NPC vs NPC, `67` aerial goal
  probe, `68` aerial save probe, `69`/`70` spin bounce (+/-), `71` arena2 bot
  probe, `72` flip-reset freestyle probe.
- Console: `python devscripts/dev-loop.py --command "<cmd>"`. Useful:
  `vehicle_input local <thr> <brake> <steer> <clutch> <handbrake> [pitch yaw roll
  jump boost]`, `gamepad_virtual <t> <b> <s> <j> <bo> <ps> <bc>|off`,
  `focus_window`, `ball_set <x y z [vx vy vz [wx wy wz]]>`, `ball_status`,
  `input.binds`, `gamepad_*`.
- Scenarios: `python devscripts/dev-loop.py --scenario-file <path>`. Existing:
  `rocket_dodge_360_v1`, `rocket_dodge_cancel_v1`, `rocket_settle_fuzz_v1`.
- Journals: `logs/<date>/<run>/{server-01,client-01,dev-loop}/events.jsonl`.
  Always quote the run id and generation in any claim.
- The dev-loop run id comes from `dev-loop.py --status`; do not pick "newest log
  directory" - parallel runtimes create sibling directories.

## 3. Code owners (touch these, not copies)

| Concept | File |
|---|---|
| Vehicle physics, flip, side recovery, landing, telemetry | `src/vehicles/vehicle-system.cpp` |
| Vehicle state fields | `src/vehicles/vehicle-types.h` |
| Profile registry + JSON schema | `src/vehicles/vehicle-profile.{h,cpp}` |
| RL profiles | `config/vehicle-profiles/rl_soccar*.json` |
| Car template (RL numbers) | `config/vehicles/octane_rl.json` |
| Arena/bot controller, goal + save events | `src/network/vehicle-networking.cpp` |
| Ball spawn/reset/state | `src/gamemode/arena-ball.cpp` |
| Map config schema | `src/gamemode/map-config.{h,cpp}` |
| Authored geometry, curved primitives | `src/map/map-authored-geometry.cpp` |
| Map editor commands | `src/mapeditor/map-editor*.{h,cpp}` |
| arena2 generator + reference | `devscripts/gen-arena2.py`, `config/reference/arena2-geometry.json` |
| Input | `src/input/input-poll.cpp`, `host-input-fallback.cpp`, `gamepad-input.cpp`, `input-commands.cpp` |
| Joints/hitbox | `src/physics/physical-entity.{h,cpp}` |

## 4. Hard-won lessons (each cost a debugging cycle)

1. **The car template must be 1:1 RL**: `length 1.18`, `width 0.842`,
   `height 0.362`, collision height the same. A second, taller collision box made
   the hittable car 7 cm bigger than the drawn one.
2. **Grounded yaw is set by steering**, not contact friction, and the sign is
   `-1` (`+y` is left). Contact friction integrated a ground torque into 4 deg of
   drift while parked and 225 deg over 9 s while driving.
3. **A profile can be empty and that silently disables all key bindings.** The
   client's `setActiveProfile("")` used to wipe the active profile. Fixed: empty
   ids are ignored and `profileFor` falls back. If keys die again, check
   `vehicle.input.no_profile` and `local_vehicle_profile` first.
4. **Injected input is a poke, not a mode**: `vehicle_input` now expires after
   120 ticks. If you add another injection path, give it a lifetime.
5. **A car cannot yaw at zero speed.** "Stuck nose-first in a wall" is usually the
   controller driving into the wall, not a collision trap.
6. **Rotated boxes**: `size[0]` rotates with the block. A `(span, 1, h)` wall at
   90 deg became a 79 m wall across the field and dropped the ball through the
   floor. Put the span on the axis that rotates into the wall direction.
7. **Field-sized floor quads** also dropped the ball through; tile at 10 m.
8. **`tilted_rests` conflated roof (allowed) with side (failure)**. Use
   `wheels_landings` / `roof_rests` / `side_rests`; acceptance is `side_rests==0`.
9. **Landing events need a debounce**: a car resting on its side flickered and
   logged 126 "landings" in one run.
10. **`glfwGetGamepadState` failing must not abort the read**; fall through to raw
    axes. `input.gamepad.poll` now logs once per second regardless of movement.
11. **Focus**: `SetForegroundWindow` is refused without an ALT unlock. The game's
    `focus_window` now does `keybd_event(VK_MENU)` first, then minimize/restore as
    a fallback.
12. **Build cache**: see §7. A link failure with `compiled: 0` is a cache fault.

## 5. Arena reference (measured, not guessed)

`assets/maps/rl/pitch.obj` (4,384 verts / 8,028 tris, UU, 1 UU = 1 cm) was
measured by `devscripts/gen-arena2.py`. Plan dimensions are DOCUMENTED; curvature
is MEASURED:

- Lower ramp (inward offset u from the wall plane, z):
  `(-2.560,0.000) (-2.044,0.052) (-1.564,0.201) (-1.129,0.438) (-0.750,0.750)
  (-0.438,1.129) (-0.201,1.564) (-0.052,2.044) (0.000,3.550)`
- Upper ramp: `(-6.243,20.440) (-4.089,20.376) (-3.128,20.077) (-2.259,19.605)
  (-1.501,18.979) (-0.875,18.221) (-0.403,17.352) (-0.104,16.391)`
- Corner plane `|x|+|y| = 80.64`; goal mouth 17.8551 x 6.42775; depth 8.80; net
  back at 60.00; ceiling plate bottom 20.44.
- **Assumption to revisit**: back walls and diagonals reuse the side-wall ramp
  profile. The mesh has per-component geometry (`SideBot`, `SideTop`, `Corner`,
  `Goal` components per RLBot) that could be measured separately.

Regenerate with `python devscripts/gen-arena2.py` (writes the reference JSON, the
map and a minimal `assets/maps/arena2.glb`). Never ship `pitch.obj`.

## 6. Next steps, in order

1. **Prove side-rest == 0** on the current build: run mode 65, assert
   `rocket_settle_fuzz_v1` (`side_rests == 0`, `wheels_landings >= 2`). If any
   side rest survives 0.4 s, strengthen the recovery rule (see
   `vehicle.side.recovery` in `vehicle-system.cpp`).
2. **Ball parity**: make the collision sphere analytic (or circumscribe the
   tessellation) and re-measure roll distance and bounce against
   `issue-2ba48bb85d4a`. This is a prerequisite for freestyle feel.
3. **Closed-loop aerial controller**: target position + target velocity + brake,
   replacing the nose-at-target law. Then the freestyle program
   (pop -> track -> reset -> dodge) can actually reach a flip reset
   (`issue-1ffbfffaefa0`). Acceptance: `vehicle.flip.reset` from an NPC, then
   `arena.goal.detected` with `freestyle=true`.
4. **Cruise mode** (mode 73, `MIMITA_VEHICLE_CRUISE`): hold throttle+boost, no
   jitter, smooth on both measured ramps, no fall-out-of-world.
5. **Tick VFX** and the hitbox debug view (§1.5, §1.6).
6. **Free play pass** (§1.7) and **camera JSON** (§1.8).
7. **Pro NPC**: replace the probe controller with something that can 1v1.

## 7. Build system state (read before your first build)

- The only builder is the root dev-loop. Never call a compiler, `build.py`, or a
  second loop. `--request game-dll` (or `batch-build`) is the path.
- **Object cache quarantine** was performed 2026-11-27 ~04:00Z for flags identity
  `b10e3a31b3ae8637`; the old objects are at
  `build/game/objects/b10e3a31b3ae8637.bak-20261127-0400` (moved, not deleted).
- A link failure naming a symbol whose definition exists, or `compiled: 0`, is a
  **cache fault**. Follow the BUILD SELF-REPAIR runbook in `AGENTS.md` 0b. The
  full occurrence is
  `docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md`.
- `packet_registry_codegen.py` can fail the pre-build check; run it once and
  retry.
- A `progress.json` `WinError 5` should no longer fail a build (10 s retry), but
  the "wait 900 s for nothing" symptom is still open
  (`issue-5aafd2207fd5`).

## 8. Coordination

- `AGENTS.md` 0b now carries **LIVENESS: KEEP GOING** (silence = the file is
  released; never leave the tree unbuildable) and **BUILD SELF-REPAIR**.
- The unresolved design TODO is agent conflict avoidance: concurrent same-file
  edits, silently stale build objects, and missing shared context. Propose under
  the coordination/brain issues, do not invent authority in a task.
- When a user reports a gameplay symptom, read the live `events.jsonl` before
  blaming the binary (this is also how the input regression was actually found).

## 9. Issue index

| Issue | Subject |
|---|---|
| `issue-dbff2cfc9a6c` | Input regression (focus + profile) - fixed in 767, keep open until pad is confirmed physically |
| `issue-1ffbfffaefa0` | NPC freestyle never reaches a flip reset |
| `issue-2ba48bb85d4a` | Ball loses speed 5x too fast; rests 10 cm high |
| `issue-9b7bf195d8af` | Car "stuck" nose-first in a wall/goal frame (reclassified as controller) |
| `issue-428f7f3b9dba` / `issue-4ad4db42be02` | Build cache loses translation units (duplicate pair, merge them) |
| `issue-70b27f66e00e` | Object cache reports a changed source as skipped |
| `issue-5aafd2207fd5` | `progress.json` rename failures / 900 s empty wait |
