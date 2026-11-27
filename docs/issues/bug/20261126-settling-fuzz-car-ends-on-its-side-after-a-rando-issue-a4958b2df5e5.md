# issue-a4958b2df5e5: Settling fuzz: car ends on its side after a random tumble instead of tending to wheels

## Human report (verbatim, immutable)

> either land on roof, or on wheels, the inbetween it tends toward wheels


## Agent suggestion

### 2026-11-26T06:10:27.003Z — cpu-a-b-verification

Independent verification by cpu-a-b-verification (2026-11-26): re-read logs/2026-11-26/20261126_054959/server-01/events.jsonl after the probe kept running past the 100 s sample. Current window has 26 vehicle.drop.fuzz kicks, 43 vehicle.landing samples, only 6 with up_dot_world_up>0.95 (14% wheels), median up_dot approx -0.21, min -0.90, 3 roof-ish (up_dot<-0.8), and 0 vehicle.rest / vehicle.rest.tilted events. This confirms the current pm6 rad/s spin probe measures full tumbles onto a flat side (a stable face on a 0.84 wide x 0.43 tall body), not the edge-to-wheels tendency from the user quote. Smallest next check: add a narrowed fuzz variant (smaller spin, e.g. +/-0.8 rad/s, with edge/corner-biased initial tilt) and rerun the same pinned launch to test the >=70% wheels / zero tilted-rest acceptance, keeping the violent-tumble probe as a separate case. Left unowned; needs a build + runtime-proof owner.

## Agent suggestion

### 2026-11-26T06:11:24.626Z — issue-review-sweeper

Independent review (issue-review-sweeper) of the pinned run 20261126_054959 server journal (logs/2026-11-26/20261126_054959/server-01/events.jsonl).

1) Stored evidence is a partial window, not the full pinned run. I reproduce the stored numbers exactly from the FIRST 19 vehicle.landing events (6/19 wheels = 31.6% -> 32%, median up_dot = -0.04), but the full journal for that run contains 43 landings and 26 vehicle.drop.fuzz kicks: 6 wheels (14%), 4 roof (up_dot < -0.7), 33 side/intermediate (tilt 30-150 deg), median up_dot -0.204, max tilt 154.8 deg. The 32%/-0.04 pair is therefore optimistic; the full-run result is worse. Evidence should state the counting window, or use the full run.

2) The proof expectation is structurally unobservable as written. vehicle.rest / vehicle.rest.tilted is emitted only on a sleep transition, but src/vehicles/vehicle-system.cpp:3517 forces sleeping=false whenever up.z <= 0.90, so a car resting on its side (up.z ~ 0) never sleeps and never emits either event. vehicle.settle fires only for up.z > 0.95 (line ~3532). Net effect: "no vehicle.rest.tilted" is vacuous, and a side/corner rest is invisible in the journal. The only usable signal today is the first-touchdown vehicle.landing pose, which measures the tumble, not the settled rest.

Smallest useful next check (owner decision; not implemented by this review):
- Derive a sustained-rest classifier from vehicle.state samples (speed<0.2 and angular_speed<0.2 for >=60 ticks) that reports wheels(up_dot>0.7)/roof(<-0.7)/side, or emit a tilted-rest event on sustained low motion regardless of sleep.
- Keep one violent fuzz arm (measured spin 1.5-8.7 rad/s) but add an edge-biased low-spin arm (e.g. <=1 rad/s spin, small initial tilt) to isolate the edge->wheels tendency the report describes.
- Re-pin and record full-run counts for the exact reproduction window.

Open clarification (do not silently narrow): the current acceptance allows a violent tumble to end on a flat side, while the user quote ("either land on roof, or on wheels, the inbetween it tends toward wheels"; "cannot land or be stuck on edge ever at all") may treat side rests as failure. That is a physics/design decision; record it as a clarification rather than assuming.

No code was changed and no proof is claimed by this independent review.

## Agent discussion and suggestions

### 2026-11-26T06:11:33.470Z — issue-review-sweeper

Message: `msg-13a00ff6d98f` (review)

Subject: Review: settling fuzz evidence + proof gap

Independent review appended to the issue for run 20261126_054959. Two findings: (1) the stored evidence (19 landings, 32% wheels, median up_dot -0.04) reproduces only as the first-19-landing window; the full pinned journal has 43 landings / 26 fuzz kicks -> 6 wheels (14%), 4 roof, 33 side, median -0.204. (2) The proof expectation is vacuous: vehicle.rest.tilted cannot fire for a side rest because vehicle-system.cpp:3517 forces sleeping=false at up.z<=0.90, and vehicle.settle requires up.z>0.95, so side rests emit nothing. Proposed smallest next check: a sustained-rest classifier from vehicle.state (speed<0.2, angular_speed<0.2 for >=60 ticks) classifying wheels/roof/side, plus one edge-biased low-spin fuzz arm (<=1 rad/s) alongside the violent arm. Do you agree on that as the next check, and should side rests count as failure or only edge/corner balance?

## Agent suggestion

### 2026-11-26T06:11:49.473Z — mode3-latency-continuation

Review (mode3-latency-continuation) of run 20261126_054959 server-01 journal — corroborates the diagnosis and sharpens it:
- Wrong population: the first 7 kicks start upright (up_before=1.00 at ticks 240/480/791/1087/1679/1943/2265). From tick 2521 on, 19 of 26 kicks start with the car already on its side (up_before ≈ -0.20..-0.43). So the probe keeps re-launching a sideways car and mostly measures the flat-side stable face, not edge->wheels. Of 43 landings: 6 wheels (up>0.95), 11 side (|up|<0.2), 26 chassis-only (wheels_contact=0); median up_dot -0.20.
- No reset/recovery: after the car first reaches the side (~tick 2521) it stays there (up_before -0.21) for the rest of the run, so the probe can never show recovery.
- Genuine intermediate snapshot: landing tick 2401 at up_dot 0.520 (tilt 58.7°) with 2 wheels touching; within 6 ticks it was airborne again (support=none) and up_dot fell 0.52 -> 0.10 -> -0.04 while residual angular speed ~1.5-3.1 rad/s carried it past the passive tipping boundary onto the side. With COM z=-0.12 the bottom-edge tipping boundary for the 0.84 x 0.431 box is ~73°, so a clean 58.7° edge rest should tip to wheels; the bounce + spin is what defeated it.
- Acceptance mismatch: "zero tilted rests" cannot coexist with "a violent tumble is allowed to end on a side", because the <=0.95 tilted gate counts a flat side (up≈0) as tilted. Classify rest as wheels (up>0.95), roof (up<-0.95), side (|up|<0.25), unstable (0.25..0.95); gate only on unstable.

Smallest next check (no gameplay change yet): make MIMITA_VEHICLE_FUZZ reset to an upright resting pose before each kick (or skip the kick unless up_before>0.90) and add a second bounded population — edge-biased drop, up.z 0.3-0.7, |angular|<=1.5 rad/s — expecting wheels>70% and zero unstable rests. Only tune COM/friction/attitude assist if the bounded population still fails.

Not edited here: src/vehicles/vehicle-system.cpp and config/vehicles/octane_rl.json already carry another agent's uncommitted physics/probe work (mtime 2026-11-26 01:05Z) and this wake is review-only. Issue left unclaimed (next.action=claim_issue).

## Agent suggestion

### 2026-11-27T17:34:38.889Z — rocket-rl-settle

Settling soak evidence (2026-11-27). Gen 797 run 20261127_165709: 31 kicks, wheels 31, side 0, nose 0, tail 0, tilted 0, worst settled up_dot 1.0 (random air-roll/dodge/boost, throttle window active, RocketSim auto-roll in). Gen 799 run 20261127_171939: 25 kicks, wheels 22, roof 0, nose 0, tail 0, but 3 tilted/side reports at settled up_dot 0.99998/1.0 with wheel masks 12/14/11 at |y| 40.6-40.8. Those are upright cars held against the long wall with 2-3 wheels in contact, not side rests; the probe treated any wheel_mask != 15 as tilted. Source fix (build pending): pose is judged by up_dot first (>0.95 upright, <-0.90 roof, else side), and partial-wheel upright rests are reported as upright_partial_rests instead of failures. vehicle.autoroll confirms throttle=1.0 plus the force/torque on incomplete-contact rests. Journals: logs/2026-11-27/20261127_165709/server-01/events.jsonl and logs/2026-11-27/20261127_171939/server-01/events.jsonl.

## Agent suggestion

### 2026-11-27T17:35:08.128Z — rocket-rl-settle

Acceptance note: the earlier narrowed-fuzz acceptance ('a violent tumble is allowed to end on a side') does not match the current task requirement. The user requires Rocket League behaviour: a random tumble with throttle input must end on wheels or roof, never side/front/back. The probe therefore keeps the violent kick plus random air-roll/dodge/boost, always applies throttle in the late window, and fails on any settled pose with up_dot <= 0.95 that is not a roof (up_dot < -0.90). A car may legally come to rest only on four wheels or on its roof.

## Agent suggestion

### 2026-11-27T18:02:47.608Z — rocket-rl-settle

Build postmortem (gen 800 attempt game-dll-20261127T174616512286Z-7470c4): changed_source_preflight failed on src/engine/engine-tick-camera.cpp at 387.3 s (compile phase 393.7 s, total 440.6 s); peak host CPU 100%, minimum free memory 57.8 MB. Measured cause: my follow-state static was declared inside the localVehicle block, but the seated third-person branch is a sibling scope, so the TU did not compile. Fixed by moving sVehicleCameraFollowTarget/Initialised/LogTimer to namespace scope and null-guarding localVehicle in the speed sample. Secondary cause: concurrent-agent resource pressure (10-way compile against ~58 MB free). Next step: one agent-build of engine-tick-camera.cpp plus the already-fixed vehicle-system.cpp, then the definitive soak.

## Agent suggestion

### 2026-11-27T18:54:59.685Z — rocket-rl-settle

Camera stiffness now has runtime evidence (gen 803). Run 20261127_182931 (stiffness 1.0, mode 60): client vehicle.camera.follow reports lag_tau_s=0.0 and extra_distance_m=0.000 at 11.2, 11.3, 11.9 and 12.0 m/s. Run 20261127_183456 (stiffness 0.0, mode 65 fuzz): lag_tau_s=0.3078 and six moving samples; the clean steady sample is speed 4.23 m/s -> extra 1.29 m against expected 1.30 m (speed*tau). Other 0.0 samples: 10.36->2.80 (3.19), 11.38->3.08 (3.50). Stiffness 0.5 reports lag_tau_s=0.1539 at rest but still needs one moving sample. Config is restored to the 1.0 default.

## Agent suggestion

### 2026-11-27T19:10:17.746Z — rocket-rl-settle

DEFINITIVE SOAK RESULT (gen 805, run 20261127_185140): 51 kicks, 51 wheels, side_rests 0, nose_rests 0, tail_rests 0, tilted_rests 0, upright_partial_rests 0, worst settled up_dot 1.0. The throttle recovery window is active (2 s from each new non-upright wheel contact), and the car settled upright at (48.36,10.9) in the same east-wall corner where gen 801 left it on one wheel at up_dot 0.399. Journal: logs/2026-11-27/20261127_185140/server-01/events.jsonl. Acceptance 'only wheels or roof, never side/front/back' is met for throttle recovery; a no-input side rest remains RL-legal and the forced fallback stays a later option.
