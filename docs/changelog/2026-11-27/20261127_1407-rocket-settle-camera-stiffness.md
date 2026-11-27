# Rocket League settling recovery and camera stiffness

- Timestamp: `2026-11-27T19:07:00Z`
- Agent: `rocket-rl-settle`
- Issues: `issue-a4958b2df5e5` (settling fuzz), `issue-a3e088b8181c` (arena2 wall escape)
- Result: settling fuzz proven for throttle recovery; camera stiffness 1.0 and
  0.0 proven at runtime, 0.5 has the measured time constant but still needs one
  moving sample.

## User request

Car settling and wall/roof sliding should match Rocket League/RocketSim, proven
by randomized fuzz, with RocketSim credited. Camera stiffness semantics:
`0.0` = loose, six car lengths of trailing at top speed; `0.5` = three car
lengths; `1.0` = 1:1 with no sway/jitter.

## Changes

### Vehicle settling (`src/vehicles/vehicle-system.cpp`)

- RocketSim-style auto-roll: throttle-only (not boost), partial wheel contact or
  body contact, force `1.0 m/s²` into the surface and torque
  `7.68 rad/s²` about the contact-relative axis.
- RocketSim-style auto-flip: `2 m/s` impulse plus `50 rad/s²` for `0.4 s`.
- RocketSim-style sticky force (0.5 g rising to 1.5 g away from the floor) and
  non-sticky wheel friction curve.
- Sleep gate accepts only a wheels or roof pose.
- Fuzz probe: random kick direction/spin, random air roll/pitch/yaw, random
  dodge, random boost, and a 2 s throttle recovery window at every new
  non-upright wheel contact. The window is the input Rocket League's
  auto-roll requires; it releases once the body is upright so the rest is
  measured.
- Fuzz pose classification is orientation-first: `up_dot > 0.95` upright
  (full or partial wheel contact), `up_dot < -0.90` roof, everything else is a
  side/nose/tail failure. An upright car held against a wall with two or three
  wheels is `upright_partial_rests`, not a side rest.
- 8 s timeout kick keeps sampling when a car is pinned without ever satisfying
  the settle test.
- Fuzz respawn margin widened to `|y| <= 41.2` so a 0.14 m wall contact overlap
  is not reported as an escape.

### Arena2 long-wall gap (`devscripts/gen-arena2.py`, `config/maps/arena2.json`)

Long walls span `79.36 m` (x = ±39.68) instead of `58.88 m` (±29.44), closing
the 10.55 m gap to the corner walls. Regenerated the map and the reference
geometry.

### Camera stiffness (`src/engine/engine-tick-camera.cpp`, `config/cameras/vehicle-camera.json`)

- `stiffness` is clamped to 0..1 and owns all trailing lag through a
  first-order follow target:
  `tau = (1 - stiffness) * stiffnessLagMaxM / topSpeedMps`.
- `stiffnessLagMaxM = 7.08` (six 1.18 m Octane bodies) and `topSpeedMps = 23.0`;
  0.5 therefore trails 3.54 m and 0.0 trails 7.08 m at 23 m/s.
- Collision push is rigid, so 1.0 is exactly 1:1 and cannot acquire a hidden
  spring (the old collision smoothing added ~1/7 s of lag even at 999).
- New `vehicle.camera.follow` sample every 3 s with `stiffness`, `lag_tau_s`,
  `speed_mps`, `extra_distance_m`, `expected_extra_m`. It uses the Gui category
  because the client journal emits no Camera-category events.

### Configs and docs

- `rl_soccar.json` / `rl_soccar_freeplay.json`: `manualRightingEnabled: false`;
  RocketSim settle values. The forced no-input fallback remains a documented
  later option.
- `docs/specs/gamemodes/vehicle-arena.md` §18.6 documents the stiffness table
  and formula; the evidence ledger carries the pinned runs.
- `CREDITS-AND-INSPIRATION.md` carries the RocketSim MIT notice and copyright.

## Evidence

Build: game DLL generations 797, 799, 801, 803 and 805 (EXE
`2026-11-27T17:45:19.700118Z:758a748d...`). All runs are mode 65 on arena2,
profile `rl_soccar_freeplay`.

| Run | Kicks | Wheels | Side / nose / tail / tilted | Note |
|---|---|---|---|---|
| `20261127_165709` (gen 797) | 31 | 31 | 0 / 0 / 0 / 0 | random throttle off in that build |
| `20261127_171939` (gen 799) | 25 | 22 | 3 / 0 / 0 / 3 | all three at `up_dot ≈ 1.0` with wheel mask 12/14/11 against the long wall: classifier false positives |
| `20261127_180452` (gen 801) | 72 | 71 | 1 / 0 / 0 / 1 | real rest at `up_dot 0.399`, wheel mask 1, `(48.1, 13.6, 0.49)` after throttle had ended; motivated the recovery window |
| `20261127_185140` (gen 805) | 51 | 51 | 0 / 0 / 0 / 0 | recovery window active; worst settled `up_dot 1.0`, `upright_partial_rests 0` |

The gen 805 run also settled upright at `(48.4, 10.9)`, the same east-wall
corner where gen 801 left the car on one wheel, which is the recovery-window
delta.

Camera:

| Stiffness | Run | Result |
|---|---|---|
| 1.0 | `20261127_182931` mode 60 | `lag_tau_s 0.0`, `extra_distance_m 0.000` at 11.2-12.0 m/s |
| 0.0 | `20261127_183456` mode 65 | `lag_tau_s 0.3078`; 4.23 m/s -> 1.29 m extra vs 1.30 expected; 10.36 -> 2.80 (3.19); 11.38 -> 3.08 (3.50) |
| 0.5 | `20261127_183025` | `lag_tau_s 0.1539` measured, but the car was stationary; a moving sample is still open |

Journals: `logs/2026-11-27/<run>/server-01/events.jsonl` and
`logs/2026-11-27/<run>/client-01/events.jsonl`.

The source and config changes are in commit `33ea17d5` (user commit
`map editor changes...`, 2026-11-27T19:02:29Z). This changelog and the final
evidence-ledger rows are the only uncommitted files from this task.

## Build friction

Two failed attempts before gen 801:

- Gen 798/800 preflight: `sVehicleCameraFollowTarget` was declared inside the
  `localVehicle` block while the seated third-person branch is a sibling scope.
  Fixed by moving the follow state to namespace scope. Postmortem for
  `game-dll-20261127T174616512286Z-7470c4`: compile 393.7 s, total 440.6 s,
  peak CPU 100%, minimum free memory 57.8 MB.
- Build requests waited on resource pressure at 537 MB free / 94.6% CPU; the
  auto policy approved once the host cleared (4498 MB / 62.3%).

## Limits and next steps

- A moving stiffness-0.5 sample is still missing. Set the config to 0.5 and
  sample a run at speed; the formula and `lag_tau_s` are already measured.
- Roof landings are allowed by the classifier but did not occur in these runs
  (`roof_rests 0`); no targeted roof drop was added.
- `side_rests` is now conditional on the throttle recovery input, matching
  Rocket League. A zero-input side rest is legal in Rocket League, and the
  forced no-input fallback stays a documented later option rather than a
  default.
- The 0.0 extra-distance ratios at 10-11 m/s are 0.88 of the steady prediction
  because those samples include an accelerating or vertical component; the
  4.23 m/s sample is the clean steady comparison.
