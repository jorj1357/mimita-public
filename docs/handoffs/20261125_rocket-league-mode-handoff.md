# Rocket League mode — handoff (2026-11-25, refreshed 2026-11-26T00:05Z)

> This replaces the 2026-11-25 22:17 version. That version's headline
> ("the car cannot jump, air roll, or rotate") is **stale**. `rocket-league-input`
> proved rotation/jump/air roll on generation 656, fixed the wire format,
> fixed `onlinemodes.json`, and disabled the free-play launch gate.
> `root-resource-loop` then rate-limited the telemetry on generation 657.
> Verify everything below; do not trust the numbers blindly.

## Verify these first

```
python -c "import json;d=json.load(open('build/game/current.json'));print(d['generation'],d['status'])"
python devscripts/dev-loop.py --status
python devscripts/evidence-watch.py list --json
```

Latest game generation: **657** (`build/game/generation-657/game-g657.dll`,
revision `56c4855e…`). Generation 656 was the last build before the telemetry
rate-limit. The shared dev-loop runtime may still be on an older DLL until its
next launch; check `runtime_build_id` vs `latest_build_id` in `--status`.

## CURRENT STATE (proven this session)

Live proof run: `logs/2026-11-25/20261125_233748/` (server+client, mode 60
`vehicle_freeplay`, generation 656). It rotated into
`server-01/events-0009.jsonl`, `events-0010.jsonl`, `events.jsonl`; scan **all
segments**, not just `events.jsonl`.

| Behaviour | Evidence | Status |
|---|---|---|
| Vehicle profile applied | `vehicle.jump` carries `"profile":"rl_soccar_freeplay"`; scenario `rocket_jump_airroll_v1` setup waits on `vehicle.profile.applied` | PROVEN |
| Ground jump | 6 `vehicle.jump` (Δv 2.9167 m/s) + 6 `vehicle.jump.apex` in run 233748; run 233342 passed `rocket_jump_airroll_v1` | PROVEN |
| Air roll / air control | 1300 airborne `vehicle.state` samples with `|roll_input|>0.05`; e.g. `roll=-1.0 yaw=-1.0 air_control=true support=none up_dot` falling 0.86→0.85; `min up_dot_world_up=-0.9971`; `max angular_speed=5.5` (cap) | PROVEN |
| Rotation crosses the wire | `vehicle.input.server` max abs pitch/yaw/roll all 1.0 (90 rows with roll>0.05); run 233039 passed `rocket_input_rotation_v1` | PROVEN |
| Flip/dodge rotation | 5 `vehicle.flip` (e.g. up `[-0.53,0.65,0.54]`, `angular_velocity [-3.31,-2.43,0.41]`) | PROVEN (as flip; see dodge gap) |
| Auto-flip from inverted rest | 2 `vehicle.autoflip` | PROVEN |
| Settles upright on wheels | 17 `vehicle.settle`; vehicle 2 rests at speed 0.0006 m/s, `up_dot=1.0`, `grounded=4`, `angular_speed=0.0047` | PROVEN |
| Ball cam toggle | run 233213 `vehicle.ballcam` on/off/on | PROVEN |
| `onlinemodes.json` ordering | `vehicle_freeplay` appended last, never inserted | FIXED |
| Free-play launch gate | `dev-launch-modes.json` mode 60 `require_mode_start=false` | FIXED for the dashboard/dev-loop launch path |
| Trigger axes at rest | Live run 233748 at-rest pad axes are `-1.0`, `vehicle.input.local` throttle 0.0; but run 230730 saw 0.578. Per-device detection still missing | OPEN (`issue-4b39c4193cd7`) |
| Dodge (`vehicle.dodge`) | 0 events in run 233748 | UNPROVEN |
| Flip reset (`vehicle.flip.reset`) | 0 events | UNPROVEN |
| Powerslide effect | No `vehicle.powerslide` event exists; only `vehicle.state.powerslide/handbrake` | UNPROVEN |
| Ceiling/wall drive + off-surface air jump | Not exercised | UNPROVEN |
| Journal telemetry rate | generation 657: `vehicle.state` 9.25 Hz/vehicle (was ~120), `vehicle.bounce` 9.26 Hz (was ~20) | PROVEN (run 20261125_235847, pinned gen657) |

### Why the old "no jump" diagnosis was wrong

A mixed quote-tolerant parser/variable failure made the earlier `rg` counts
unreliable, and the earlier run predated the wire-format fix. The authoritative
check is a JSON parse across every journal segment. Do that, not raw `rg`.

Jump availability (source `src/vehicles/vehicle-system.cpp` ~1930-2200):
- all four wheels grounded → `jumpCount=0` → ground jump + one air jump;
- airborne after a ground jump → second press within 1.25 s is a dodge/flip
  (magnitude ≥ `dodgeDeadzone`) or a double jump;
- all four wheels touching **any** surface (including the ball) re-arms the
  flip; ball contact emits `vehicle.flip.reset`;
- `infiniteJumps` defaults **false** (`vehicle-profile.h`), read from
  `jump.infiniteJumps`; free play leaves it unset, so it is off.

## Telemetry / journal-volume fix (generation 657)

The old build emitted `vehicle.state` unconditionally on every 120 Hz entity
tick: 96,239 events = **71.4% of 227 MB** in a 6-minute server journal. That
rotated evidence away every ~2 minutes and buried proofs.

Change: `vehicle.state` now emits at `tick % 12` (~10 Hz) and
`vehicle.bounce` uses `bounceTick % 12` (spec says 10 Hz; the 120 Hz entity step
had doubled it to 20 Hz). Proof: isolated run `20261125_235847`
(`logs/2026-11-25/20261125_235847/`), server pinned to `game-g657.dll`
sha256 `26e61006…`, measured 401 `vehicle.state` per vehicle over 43.33 s =
**9.25 Hz/vehicle**, and `vehicle.bounce` 9.26 Hz/vehicle.

The isolated test-run path still fails mode readiness
(`client did not observe active mode after 2405 server ticks`). That is a
separate readiness path from the fixed dashboard launch mode; see
`issue-b5a35be7b0e0`.

## Open defects, priority order

1. **Dodge unproven** (`vehicle.dodge` = 0). The live input never re-pressed
   jump while airborne with directional input, so the dodge branch was never
   entered. Add/execute a scenario: jump, release, then press jump again within
   1.25 s while holding a stick direction; assert `vehicle.dodge` plus a
   `stall` sample with opposing yaw/roll.
2. **Powerslide unproven**. There is no `vehicle.powerslide` event. Prove it
   from `vehicle.state` (`powerslide=true`, `handbrake` rising) plus reduced
   lateral grip while R1 (`gamepad.json` powerslide = button 5) is held.
3. **Flip reset unproven** (all four wheels on `arena-ball`).
4. **Ceiling/wall drive and off-surface air jump** unproven.
5. **Free-play isolated test-run readiness** still fails; dashboard mode-60
   launch is fixed.
6. **Server access violation `0xC0000005`** (`exit_code 3221225477`), once,
   shortly after a client joined. Intermittent. Next: enable crash-handler
   minidump and read the faulting module, or bisect the five 2026-11-25
   server-side changes (sphere-pair contact normal, swept-sphere CCD,
   rolling-resistance loop, `vehicle.state`, 120 Hz fixed step).
7. **Landing penetration at speed / wheels catching geometry**: wheel colliders
   were disabled (`collisionRadius: 0`) and need per-wheel contact clamping, not
   a radius tweak.
8. **HUD bind readout**: show every RL bind, live value, and gamepad button.
9. **Trigger-axis convention** per device (`issue-4b39c4193cd7`).

## Shipped and unproven / shipped and proven

Shipped (code in tree, uncommitted): normalized `InputFrame`; PS4 bindings in
`config/input/gamepad.json` (Cross jump, Circle=air-roll-right,
Square=air-roll-left, Triangle=ball cam, L1 boost, R1 powerslide, R3 look-behind,
L2/R2 triggers); `VehicleInputPacket` carries pitch/yaw/roll (protocol 53);
per-axis air torque `α=A·u−D·ω`, `|ω|≤5.5`; jump Δv 2.917 + sustain 14.58/0.2 s +
adhesion 3.25/3 ticks; dodge/flip with flip-cancel and Z damping; flip reset via
wheel-ball contact; auto-flip; accumulated-impulse contact solver; swept-sphere
CCD; 1/120 entity step; possession hide; free-play gamemode; vehicle camera
(ball cam, swivel, look-behind); local prediction; `vehicle.state` telemetry;
and the events list in `docs/specs/vehicles/vehicles.md` §24 plus
`vehicle.jump.*`, `vehicle.airroll`, `vehicle.dodge`, `vehicle.flip*`,
`vehicle.autoflip`, `vehicle.bounce`, `vehicle.settle`.

Proven in journals: R2 reaches the vehicle; drive to 20 m/s; profile apply;
rotation on the wire; jump; air roll both directions with orientation change;
angular-speed cap; ball cam; upright settling; long-session server survival;
telemetry rate (gen657).

Unproven: dodge, powerslide effect, flip reset, ceiling/wall drive, landing
penetration behaviour, prediction error, NPC scoring, possession visibility,
crash root cause.

## Journals worth reading

- `logs/2026-11-25/20261125_233748/` — jump/air-roll/flip/settle telemetry
  (scan all `server-01/events*.jsonl` segments).
- `logs/2026-11-25/20261125_235847/` — gen657 telemetry-rate proof (isolated).
- `logs/2026-11-25/20261125_233342/` — `rocket_jump_airroll_v1` pass.
- `logs/2026-11-25/20261125_233039/` — `rocket_input_rotation_v1` pass.
- `logs/2026-11-25/20261125_233213/` — `rocket_ballcam_v1` pass.
- `logs/2026-11-25/20261125_221353_long-proof/` — pre-fix no-jump diagnosis.

Scenarios: `devscripts/scenarios/rocket_jump_airroll_v1.json`,
`rocket_input_rotation_v1.json`, `rocket_ballcam_v1.json`.

## Next actions, in order

1. Launch on generation 657 (dashboard mode 60 or a fresh run) and confirm the
   steady-state `vehicle.state` rate is ~10 Hz/vehicle and the journal does not
   rotate every 2 minutes.
2. Dodge proof scenario (jump → release → second press + direction within
   1.25 s). Expect `vehicle.dodge`, `stall` behaviour, and `vehicle.flip`.
3. Powerslide proof from `vehicle.state` (`powerslide`, `handbrake`, lateral
   slip) while holding R1.
4. Flip-reset proof: all 4 wheels on `arena-ball` → `vehicle.flip.reset`.
5. Ceiling/wall drive then detach → one air jump available; prove with
   `vehicle.state` `support` transitions and a `vehicle.dodge`/jump.
6. HUD bind readout (all binds + live values + buttons).
7. Crash-handler minidump for `0xC0000005`.
8. Per-wheel contact clamp + `vehicle.wheel.state`; then landing proof.
9. Remaining: prediction proof, arena curves/lighting, boost meter, nametags,
   touch markers, sounds, hitbox/ghost debug, spec expansion in
   `docs/specs/gamemodes/vehicle-arena.md`.

## Rules that still apply

- Build only through the root dev-loop; never a compiler or a second loop.
- Do not edit a file leased by another agent; `agent-coord.py edit begin` first.
- Separate build evidence from runtime evidence. `vehicle.state` at 10 Hz is a
  sample; discrete events (`vehicle.jump`, `.airroll`, `.dodge`, `.flip`) are
  the primary proof for transitions.
- Parse JSON across **all** journal segments; do not trust raw `rg` counts on a
  rotating journal.
- Repair configs with the comment-tolerant parser, not `json.load`.
