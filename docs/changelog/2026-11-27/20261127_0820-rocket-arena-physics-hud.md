# Vehicle Arena: ball contact parity, arena HUD, and freestyle climb

Written 2026-11-27 by `rocket-arena-physics-hud`.

## Physics: ball contact and rolling parity

**Finding.** `solveBatchedCorrection` builds its target as `penetration + slop`
(10 mm). Every sphere contact therefore lifted the arena ball, and the friction
solve turned that lift into energy. The sphere also used a collision radius with
a 5 mm skin. `arena.ball.state` showed the symptom directly: rest `z`
oscillated `0.90933 -> 0.91750` and speed rose inside each cycle.

**Change.** `src/physics/physical-entity.cpp`:

- sphere contact uses the authored radius with no skin,
- sphere correction slop is zero (actors keep 10 mm),
- restitution/friction/damping values are unchanged.

**Proof.** New isolated mode `74` (`clean rolling-ball drag probe`, velocity and
spin written in the same order as `ball_set`), run `20261127_074605_01`,
generation 786:

```text
2.9902 -> 2.2638 m/s over 9.185 s   k = 0.03030 /s
3.2769 -> 2.4809 rad/s              k = 0.03030 /s
omega / v = 1.0959 = 1 / 0.9125
centre z = 0.90250, floor top -0.01 -> exactly one radius, 160/160 samples
```

This closes the drag and rest-height rows of `issue-2ba48bb85d4a`. The previous
`0.052 /s` fit came from a path that had already hit walls and ramps.

**Regression check.** Spin-dependent bounce re-confirmed on generation 786:
mode 69 run `20261127_074747` and mode 70 run `20261127_074833` both rebound from
`-8.67` to `+5.47 m/s` (restitution 0.63) with mirrored `+/-5.10 m/s`
horizontal deflection.

## Arena presentation

- `hud_speed on|off` added, default **off**. The km/h + rpm line is no longer
  always drawn.
- While seated in `rl_soccar`/`rl_soccar_freeplay`, the combat HUD is suppressed:
  panels, player name, HP, ammo, crosshair, hitmarker, combat speed line, mode
  text, hotbar, healthbars, and sprint bar. Chat remains.
- The arena HUD is a bottom-right segmented boost meter with the numeric value
  and a `BALL CAM ON/OFF` label.
- `config/gui/gamemode-meta-gui.json` gained a `vehicle_arena` section
  (`modeTitle`, `scoreText`, `matchTime`, `phaseText`, `goalText`,
  `intermissionText`, `countdownText`).
- `CommunityMatchClient` now stores `scoreA/scoreB`; the generic overlay uses
  them for arena modes instead of `roundWins`.
- Client-observed goals show a 2 s `{team} SCORES` flash and log
  `arena.presentation.goal_flash`. The HUD logs `arena.hud.snapshot` on phase or
  score change.
- `config/cameras/vehicle-camera.json` now owns distance 2.20 m, height 0.90 m,
  pitch -7 deg, FOV 110, stiffness 1.0, transition/swivel 10. The vehicle camera
  path applies them and logs `vehicle.camera.applied`.

Evidence:

- `20261127_080131` / `20261127_080234`: `vehicle.hud hud_mode=arena`,
  `hud_speed=false` by default, `hud_speed on` logged and the HUD then reported
  `hud_speed=true`; `vehicle.camera.applied` with the values above.
- `20261127_080258`: `arena.hud.snapshot` with phase, `score_a/b`, clock limit,
  and `layout_section=vehicle_arena`.
- Visual placement of the boost meter and the absence of combat HUD remain a
  human check; no screenshot command exists in the dev loop.

## NPC freestyle

Baseline (gen 789, run `20261127_080709`): the freestyle started 2.47 m out,
never boosted, peaked below the ball, and ended `landed` after 107 ticks with
zero resets.

Three fixes in `src/network/vehicle-networking.cpp`:

1. Freestyle starts only from the ground (`carPosition.z < 0.9`).
2. The tracking jump is held for 6 ticks, matching the proven aerial path.
3. Self-right no longer overwrites freestyle input while a freestyle is active
   (it was zeroing boost whenever `chassisUp.z < 0.75`).
4. `flyAtTarget` applies the aerial 0.25 boost floor while turning.

Result, gen 794 run `20261127_081833`: the car jumps (`z 0.30 -> 2.18`), boosts
during the climb (`vehicle.boost` at 4.83-5.96 s), and reaches the point above a
`z=1.18` ball. It still ends `landed` with `resets_this_run=0`; the remaining gap
is the reset/descent controller, recorded on `issue-1ffbfffaefa0`.

## Tooling

- `devscripts/dev-launch-modes.json`: mode `74` clean-roll probe.
- `devscripts/dev_loop/parallel_test_runs.py`: launch-mode `env` now reaches
  isolated test servers/clients; profiles with `require_mode_start=false` no
  longer fail on free play.
- `devscripts/dev-profiles/vehicle-freeplay.json`: isolated free-play profile.
- `AGENTS.md`: two-ask no-response build rule (user request).

## Open items

- Goal flash still needs a scored goal in a run; the scoreboard data path is
  proven.
- Boost-pad world visuals, tick VFX, hitbox debug view, and audio remain spec
  items, not implemented.
- `issue-a3e088b8181c`: the ball also escapes through the lower side wall at
  `x~30, y~-40.9` at only ~7 m/s on gen 789/790. Evidence appended; the exact
  sphere contact does not see the wall/ramp triangles there.
- NPC flip reset, 1v1 opponent quality, 50/50, dribble, car-ball curve, and
  cruise proof remain open (`vehicle-arena.md` sections 19-20).
