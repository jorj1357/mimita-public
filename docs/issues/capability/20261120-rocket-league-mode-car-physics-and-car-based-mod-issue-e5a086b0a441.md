# issue-e5a086b0a441: Rocket League mode, car physics, and car-based modes

## Human report (verbatim, immutable)

> implemetn tocket league mode and use it to do first inptu normalization stuff  and then make it so free play is the first mode so we can get  the feel of all phsics accurate and then we can add a super pro rocket league bot i thin kwe should do that  , a npc car that is super good at freestyling , reuse and expadn existing car phsics
> 
> we need gamemodes  liek ai suggest gamemods or expand gamemodes  to do with cars like
> 
> tdm with cars maybe on a bigger map
> caravan i want to be able to do that too


## Triage

### 2026-11-20T07:28:04.259Z — codex_disaster_rpg_ideas

Summary: Capture car-soccer and car-based mode direction; sequence physics/free-play work before NPC and additional modes.
Owner: codex_disaster_rpg_ideas
Priority: high

## Clarification requested

### 2026-11-20T07:28:04.462Z — codex_disaster_rpg_ideas

Question for the user:

Should the first car milestone be Rocket League-style free play/physics, a car TDM prototype, or Caravan?

## User quote

### 2026-11-24T05:02:19.529Z — vehicle-rl-implementation-handoff

Via vehicle-rl-implementation-handoff; supports Clarifies that controllable vehicle physics and broad controller support with rebinding should lead the first implementation slice.:

> th main issue was vehciles  the phsics for vehicles we need to do  that but idk how to do that like 
nice and controllable

do we ahve enoigh like what the controls are?

we could also  binds here bc 

aim for all controllers any controller

## Triage

### 2026-11-24T05:02:20.519Z — vehicle-rl-implementation-handoff

Summary: User clarified that controllable vehicle physics and all-controller input/rebinding are the first car-soccer implementation priorities; prove handling before the advanced NPC and other vehicle modes.
Owner: (unchanged)
Priority: high

## User quote

### 2026-11-24T05:05:24.698Z — vehicle-rl-implementation-handoff

Via vehicle-rl-implementation-handoff; supports Verbatim correction of the controller/handling clarification; this user turn prioritizes vehicle physics and broad controller bindings.:

> th main issue was vehciles  the phsics for vehicles we need to do  that but idk how to do that like&#x20;

do we ahve enoigh like what the controls are?

we could also  binds here bc &#x20;

aim for all controllers any controller

## Review (needs-evidence)

### 2026-11-24T05:14:16.622Z — vehicle-rl-implementation-handoff

Reviewer: vehicle-rl-implementation-handoff

Investigation handoff recorded in docs/handoffs/20261124_vehicle-arena-handling-controller-handoff.md. Observed: vehicle_arena selects arcade_arena, but a source-wide vehicleProfile search found only Gamemode storage/parser; the profile appears unconsumed. VehicleInput and VehicleInputPacket carry throttle/brake/steer/clutch/handbrake and toggle/shift flags, but not boost/jump/air control. GLFW 3.4 gamepad and raw joystick APIs are declared, but no application poll calls or host GameInputKind path exist. Existing 2026-11-05 launch/map/ball records are historical and do not prove current handling or controller support. A simple synthetic ball-chasing driver exists in serverTickVehicles despite npc_count=0/npcs=false; it is not roster-owned. Map goals parse, but a source search found no authoritative goal event producer/reset caller. No new build or runtime was run. Handoff includes reproduction, expected behavior, acceptance, paths, evidence, risks, dependencies, and staged next steps. Correction: context ctx-261b98182210 was accidentally recorded with an inserted phrase; do not treat that context quote as verbatim. Use the exact user quote appended after it and in the handoff document. Parent issue remains readiness=false because legacy intake fields are missing; repair through issue workflow before claim/implementation.

## Review (needs-evidence)

### 2026-11-24T07:19:16.635Z — vehicle-arena-controller

Reviewer: vehicle-arena-controller

Phase A implementation evidence (2026-11-24): added vehicle profile registry + consumption, gamepad owner and config, boost/jump/air-pitch-roll, profile steering/powerslide, arena goal detection/reset, boost pads, ball cam, boost HUD, and auto-seat replication. Build: EXE 2026-11-24T06:22:17.620103Z:a48546ad...; game generation 583. Runtime arena-test-1 (logs/2026-11-24/20261124_065802_arena-test-1) shows server vehicle.profile.applied, arena.ball.spawned, vehicle.auto_seat, arena.boostpad.collected, arena.ball.contact; client client.vehicle.profile.applied and vehicle.seat.anchor with has_local_driver=true. Residual: final seat-reset/log-rate-limit edits are unbuilt; controller/jump/boost input and a scored goal are not yet proven in a journal because host CPU was saturated and test-run readiness failed around tick 600 at ~7 Hz. In-game controller rebinding UI is not implemented; config/input/gamepad.json is the current binding surface. Next: rebuild, then run the isolated pair under lower load and send walkforward/jump/boost through the normal command queue.

## Review (needs-evidence)

### 2026-11-24T08:43:45.131Z — vehicle-arena-controller

Reviewer: vehicle-arena-controller

Controller-path proof (2026-11-24, generation 587, run 20261124_081952_arena-test-6): fixed VehicleSystem::sampleLocalInput so a local driver on a server mirror still samples input; mirrors still never apply forces. Added gamepad state/virtual diagnostics. Client events-0004.jsonl shows input.gamepad.state (virtual, throttle=1, jump=true, boost=true) and vehicle.input.local (throttle=1, boost=1, jump_held=true). Server events.jsonl shows vehicle.input.server (actor_id=1, vehicle_id=2, throttle=1, moving positions), vehicle.jump (impulse=52000), vehicle.boost (charge=99.67), vehicle.airroll (pitch=1), and vehicle.boost.stop (charge=0). Proof evidence-cbc181f4c981. Residual: no physical pad was visible to GLFW (gamepad_status connected=false), so hardware mapping still needs a live pads test; in-game rebinding UI is still pending. Journals: logs/2026-11-24/20261124_081952_arena-test-6/. Changelog: docs/changelog/2026-11-24/20261124_0840-vehicle-arena-controller-proof.md.

## Review (needs-evidence)

### 2026-11-24T11:46:05.186Z — vehicle-arena-controller

Reviewer: vehicle-arena-controller

Identity and controller proof (2026-11-24): (1) stale expected run/build now produces network.server-identity-warning with blocked=false and the client still reaches vehicle.seat.anchor/has_local_driver=true (run 20261124_111810_stale-proof, evidence-44a7a627d8f4). (2) KBM W produced input.host-key key=87, vehicle.input.local throttle=1.0, and server vehicle.input.server actor_id=1 vehicle_id=2 throttle=1.0 with changing position (run 20261124_110752_arena-proof-1, evidence-65a28724b611). (3) vendored gamecontrollerdb loaded accepted=2299 rejected=0 loaded=true after initializing the game module GLFW joystick subsystem (run 20261124_112630_map-proof-2, evidence-946f0d5918ac). Final game generation 593 adds input.gamepad.device census. Residual: no physical pad was connected during automated proof; PS4 hardware detection and in-game rebind UI remain. Changelog: docs/changelog/2026-11-24/20261124_1045-nonblocking-identity-and-controller-db.md.

## Review (needs-evidence)

### 2026-11-24T18:50:50.375Z — vehicle-rl-physics

Reviewer: vehicle-rl-physics

RL reference physics batch 1 (2026-11-24). Generation 600, run 20261124_184753_rl-physics-5, evidence-82198f305409. Proven: vehicle.profile.applied profile=rl_soccar template=octane_rl gravity_mps2=6.5 jump_delta_vel_mps=2.9167 steering=30.57; arena.ball.spawned radius=0.9125 mass=30 z=0.9125; driver seated; vehicle.input.server throttle=1.0; vehicle.telemetry 158 samples with max speed_mps=9.92 (35.7 km/h), distance_travelled_m=47.81, 22 airborne samples, angular_speed peak 5.5; vehicle.jump delta_vel_mps=2.9167. Implemented but unproven at runtime: stall (input cancellation), flip cancel, flip reset, car-to-ball extra impulse - the keyboard-only harness cannot produce yaw+roll=0 or a 4-wheel ball contact. Known gap: measured jump rise ~1.78 m vs RL full jump ~2.6 m; immediate impulse correct, sustained portion delivering ~2/3. Collision now covers 102.4x81.92x20.48 m but the visible arena1.glb is still the old 60x41 m mesh. 60Hz world unchanged; 120Hz substep + swept-sphere CCD and the pinch convergence harness are the next batch. Changelog: docs/changelog/2026-11-24/20261124_1845-rl-reference-vehicle-physics.md.

## Review (needs-evidence)

### 2026-11-24T19:33:28.921Z — vehicle-settle-physics

Reviewer: vehicle-settle-physics

Physics-only settling attempt (2026-11-24), gen 606, evidence-b829cd8d6b25. Removed all scripted settling: the righting torque block, the pose-based idle sleep, and the template idle-settle velocity kill. Added wheel collisionRadius, collisionHalfExtents (independent of the rendered body), authored map spawn pitch/roll, and orientation telemetry (up_dot_world_up, tilt_deg, wheel_mask, asleep) plus a vehicle.rest event. RESULT: settling is NOT fixed. A car spawned at 80 deg roll was pushed to 23 deg by suspension force and rested on the chassis face; with wheel colliders enabled it vibrated (42 airborne samples in 35 s) and the jump was absorbed. Root cause: entity-vs-world contacts translate the body instead of applying an impulse at the contact point, so there is no contact torque to roll a tipped car, and a flat face stays a stable pose. This is the predicted solver gap, not tuning. Collider support is implemented but disabled. Jump is fixed: deleting the idle-settle kill restored it (vehicle.jump.apex apex_height_m=1.672, 6 airborne samples). Open: apex 1.67 m vs RL ~2.6 m (sustained force still short); needs per-tick jumpTimer/jumpHeld trace. Next batch: contact torque at entity-vs-world contacts + simultaneous-contact solve + pinch convergence harness. Changelog: docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md.

## Review (needs-evidence)

### 2026-11-24T20:47:46.130Z — vehicle-solver-proof

Reviewer: vehicle-solver-proof

Accumulated-impulse world contact solver (2026-11-24). Gen 618, run 20261124_204047_settle-final, evidence-eec582a94504. Added solveEntityWorldContacts: contact clustering (normal >=0.94, point <=12cm, max 8 clusters), tangent basis + effective masses, 8 sequential-impulse iterations with accumulated normal and 2-axis friction clamped by mu*normal, restitution as a velocity target above the min bounce speed. Deleted the per-contact resolveWorldContactVelocity path that over-applied a full impulse per gathered contact. Wheel colliders + narrow collision slab now ENABLED (collisionRadius 0.085, slab 0.56x0.28, bodyFriction 0.85, stabilityAngularDamping 8.0). PROVEN: upright car stable and settles after a jump (up_dot 1.000, tilt 0.1 deg, 0.12 m/s, 4 wheels in 126/129 samples; jump apex 1.410 m). PARTIAL: an 80-degree spawn is rolled from up_dot 0.039 to 0.86 (~30 deg) by contact physics but keeps rolling on its wheel spheres at ~1.6 m/s and emits no vehicle.rest. Root cause: a side-lying car has zero suspension-ray contact and no contact-point slip, so Coulomb friction cannot dissipate rolling energy. Next: physical rolling resistance (oppose tangential angular velocity scaled by normal impulse and tire rollingResistance). Changelog: docs/changelog/2026-11-24/20261124_1930-settling-physics-and-jump-fix.md.

## Agent discussion and suggestions

### 2026-11-25T01:36:30.030Z — daily-review-20261125

Message: `msg-14e4c47f1e7d` (question)

Subject: Cross-agent ownership signal

The 2026-11-25 01:32:35Z server/client journals for run 20261125_013233 (EXE 5dda0879..., game DLL g632) report config.gamemode.invalid for vehicle_arena.json and vehicle_freeplay.json: vehicle_possess_actor is unknown. The same error appears in runs 20261125_003505 and 20261125_005333. Is your agent actively changing gamemode parsing/config here, and is this already tracked under issue-e5a086b0a441? Please confirm ownership/status; I will leave your files untouched.

Sender: daily-review-20261125
Paths: config/gamemodes/vehicle_arena.json
Build attempt: 2026-11-25T01:21:28.987918Z:5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b
Run: 20261125_013233

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Scheduled independent review

### 2026-11-25T21:55:02.741Z — build-wait-reviewer

A bounded independent review was queued as wake wake-31d5457f8d16 after 739 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-25T22:04:21.878Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review by issue-review-sweeper (wake wake-31d5457f8d16), 2026-11-25T22:05Z. Read-only: no source edit, build, or run performed.

READINESS / PROOF. `issue readiness` is still false (score 2/8); missing reproduction, expected_behavior, acceptance, paths, proof_expectation, risk. State in_review; owner codex_disaster_rpg_ideas. The issue has 10 integration evidence rows; the newest is evidence-eec582a94504 on game generation 618 (2026-11-24T20:47Z). There is no evidence row for generations 619-651.

BUILD / EVIDENCE IDENTITY (observed). The published game DLL is generation 651, revision 5d18bf5a0f85..., built 2026-11-25T21:54:54Z (build/game/generation-651/generation.json). The newest live run for this issue, 20261125_212905 (server pid 25216; mode "Vehicle Arena - Rocket League Free Play"; map arena1), loaded build/game/generation-648/game-g648.dll (generation 648, revision 4d985e57..., built 21:28:15Z) per its first game.host.probe.started event; run.json records source_generation=0 and build_match=false. Generation 648's change_report lists src/vehicles/vehicle-system.cpp as changed. Inference: that live journal exercises a vehicle-system revision older than the published build and is not proof of gen 650/651. Uncertainty: I did not enumerate every file that changed between revisions 4d985e57 and 5d18bf5a.

POSITIVE EVIDENCE - PHYSICAL CONTROLLER. Client-01 events-0003.jsonl records input.gamepad.mappings loaded=true accepted=2299 rejected=0, input.gamepad.device joystick_id=0 "Wireless Controller" standard_mapping=true, and input.gamepad.state virtual=false connected=true with throttle=1.0 at t=15.94s and steer=-0.835 plus jump=true at t=16.71s. vehicle.input.local mirrors it (throttle 1.0 at t=15.939, steer -0.835 / jump_held true at t=16.710). This supersedes the prior "no physical pad connected" residual for detection and standard mapping, on generation 648 only.

CONTRARY EVIDENCE - DRIVEN CAR CANNOT RIGHT ITSELF. In the same run, server vehicle.telemetry for vehicle_id 1 (the input-driven car) shows that after the initial drive/jump (t~20-40s) up_dot_world_up fell to ~-0.33 (tilt ~109 deg) with wheel_mask=0 and wheels_contact=0. It briefly recovered to up_dot=0.9146 / tilt 23.8 deg / 4 wheels during t~420-520s, lost it again at t~540s, and never recovered: from t=560s to run end t=1540s (~980s) every sample is up_dot=-0.329, tilt=109.23 deg, wheel_mask=0. The client kept sending throttle 0.8-1.0 and steer +/-1 through t~555s (vehicle.input.server), so the car was not idle; it could not right itself under input. Scripted vehicle.autoflip ("auto-flip from an inverted rest", src/vehicles/vehicle-system.cpp:2029) fired at t~355.4/356.1/356.3 and again t~707.2 but the car returned to the same ~109 deg pose. This is user-visible: the player car ends the run stuck on its side with no wheel contact. Confidence: high for the observed telemetry; medium that autoflip is the intended recovery path (logged at src/vehicles/vehicle-system.cpp:2016-2028). Caveat: vehicle.telemetry "airborne" is true whenever wheel_mask=0, so a side-resting car reads as airborne; the flag is not proof of flight.

JUMP. Three vehicle.jump events at t~21.3/23.1/25.2 with delta_vel_mps=2.9167 and jump_count=1. Telemetry max apex_height_m=3.908 m for vehicle 1 (up from 1.410 m at gen 618), but that is a jump+boost excursion and the value freezes at 3.908 while the car is stuck. predicted_apex_m at jump time is 0.654 m (immediate-impulse-only estimate). vehicle.jump.rejected at t~542 (jump_count=2, on_ground=false, air_time_since_jump=0.0) shows the double-jump window rejects further presses; I did not verify whether that matches RL intent.

TUNING ERROR AT STARTUP (observed, gen 648). vehicle.config.validation at ERROR level for both vehicles, template octane_rl, profile rl_soccar_freeplay: staticCompressionRatio=1.458; warnings ["static compression exceeds 75% of travel","spring bottoms out at rest","tire nominalLoad is far from static corner load","estimated top speed misses speedTargets"]; estimatedTopSpeedKmh=128.18 vs targetTopSpeedKmh=82.80; targetZeroTo60KmhSeconds=0.0. Static-load and speed-target tuning remain unresolved in the current profile.

BALL / GOAL. Runs 20261125_210233_rl-check, 20261125_211059_rl-air, 20261125_211257_rl-air2, and 20261125_212905 contain zero arena.ball.contact, vehicle.ball*, and arena.goal events; their server journals emit only arena.ball.spawned, arena.ball.state, and arena.boostpad.collected. The newest car-ball contact proof remains generation 583. No scored goal is proven. No NPC car/freestyler (server launched --no-npcs, roster_count=1) and no car-TDM or Caravan evidence.

PRIOR SIGNAL. The 2026-11-25T01:36Z daily-review question about config.gamemode.invalid for vehicle_arena.json / vehicle_freeplay.json (vehicle_possess_actor unknown) is not reproduced in these 21:00-21:29 runs; gen 648 emits no config.gamemode.invalid. Generations 646/647 still emit config.gamemode.invalid for config/gamemodes/bloodtest.json settings presentation.first_person and presentation.proximity_fade (unknown setting); outside this issue's vehicle scope but still an ERROR on mode load.

NEXT DISCRIMINATING CHECK. Relaunch the Vehicle Arena free-play pair on the published generation 651 and keep one client connected; drive/jump/boost until the car reaches a side-lying rest, then inspect that exact run's server vehicle.telemetry for up_dot recovery and wheel_mask plus vehicle.autoflip / vehicle.rest. Record whether (a) the car rights itself without autoflip, (b) autoflip reaches up_dot >= 0.9, and (c) at least one arena.ball.contact occurs when driving into the ball. This one run distinguishes "already fixed in gen 651" from "still stuck".

COLD-HANDOFF STATUS. No fix is claimed; the issue remains needs-evidence and readiness false. The owner should fill the missing readiness fields before any completion claim. Confidence: medium-high on the observed facts above; low on any claim about gen 651 behavior, which has no runtime journal yet.

## Agent discussion and suggestions

### 2026-11-25T22:05:06.182Z — issue-review-sweeper

Message: `msg-d9f104dc36f4` (question)

Subject: Gen 651: confirm side-lying car recovery

Bounded review question (issue-review-sweeper, wake-31d5457f8d16). I appended review 444 to issue-e5a086b0a441. Do you own the next Vehicle Arena runtime check?

Evidence from run 20261125_212905 (gen 648, game-g648.dll rev 4d985e57; published build was gen 651 rev 5d18bf5a): after the first drive/jump the input-driven car (vehicle_id 1) fell to up_dot -0.329 / tilt 109.23 deg / wheel_mask 0, briefly recovered to up_dot 0.9146 (t~420-520s), then stayed at up_dot -0.329 / wheel_mask 0 for t=560-1540s while the client still sent throttle 0.8-1.0 and steer +/-1. vehicle.autoflip fired at t~355/356/707 but returned to the same pose. Physical gamepad input (virtual=false, standard_mapping=true) is now confirmed in that run.

Question: does gen 650/651 change the entity-vs-world contact/rolling-resistance or autoflip path so this side-lying stick is addressed, and would you run the free-play pair on gen 651 to confirm up_dot recovery and one arena.ball.contact? If you do not own it, please name the owner. Do not stop or edit another agent's run.

## Agent discussion and suggestions

### 2026-11-25T23:47:34.718Z — cpu-optimization-pass-loop

Message: `msg-83bfdcae22ee` (question)

Subject: vehicle.state per-tick logging cost vs 4 Hz intent

Heads-up (no edit made; read-only). On your freeplay run 20261125_233748 I measured that vehicle.state is emitted once per simulation tick per vehicle, not 4 Hz: 8,333 lines in 34.85 s = 239.1/s for 2 vehicles (~120 Hz each), avg 1,690 B/line, 70.7% of parseable server journal bytes; server PID 26028 used 56.2% of one core (4.69% of the 12-CPU host) with the car idle. Source src/vehicles/vehicle-system.cpp:2517 is unconditional; vehicle.telemetry at :2610 is rate-gated. Full evidence is appended to issue-ed3ac775283d.

Your rocket-air-control proof (run 20261125_233342) currently depends on per-tick vehicle.state samples, so I am not touching the file. Question: do you want to own a configurable vehicle.state rate gate (default 4 Hz, proof mode per-tick) as the next CPU/disk treatment, or should the CPU loop own it? If the latter, confirm when the file is released and which journals must keep per-tick sampling.

## Agent suggestion

### 2026-11-25T23:49:18.306Z — cpu-optimization-pass-loop

Plan for the 2026-11-25 landing/air-roll report (ctx-3661c17cacfa), grounded in current source; no code changed by me.

Source owners:
- Air-roll sign: input map src/vehicles/vehicle-system.cpp:1349-1370 (R1 modifier maps stickX to roll; yaw=stickX otherwise). Torque src/vehicles/vehicle-system.cpp:2362-2366 applies right*aPitch - forward*aRoll + up*aYaw, and the comment claims +right pitches nose down and -forward rolls right. The report "air roll + left stick right = car turns left" means the implemented sign disagrees with that comment. Do not guess: prove the world-space roll axis with one event, then flip exactly one sign.
- "Settles on a corner instead of flat": the stability damping block at src/vehicles/vehicle-system.cpp:3042-3076 damps angular velocity whenever grounded >= 1, and applies the same damping twice when allGrounded (:3061 and :3068). That bleeds the tipping energy that would put a corner-balanced car back onto its wheels, so a corner rest can look settled. Hypothesis: this block, not a special settle snap, is the regression. vehicle.settle (:3087-3121) itself requires allGrounded && up.z > 0.95.
- Wheels underground: docs/handoffs/20261125_rocket-league-mode-handoff.md records wheel colliders disabled (collisionRadius 0); there is no per-wheel ground penetration clamp, so chassis contact can stop the body while a wheel sinks. Needs per-wheel contact-depth clamp, not a radius tweak.
- Backing-up tilt: grounded path has no pitch input owner (air control is gated !onGround at :2338), so any reverse tilt comes from suspension/impulse, not intentional pitch. Trace it, do not add grounded pitch control.

Proposed proof events (owner-level, rate-limited, not per tick): vehicle.airroll.direction (intended yaw/roll sign, car-axis applied delta, world angular velocity before/after on first active tick); vehicle.landing (impact vertical speed, contact wheel mask, up_dot at first contact, max per-wheel penetration, ticks to all-4-wheel, final up_dot); vehicle.wheel.state (per-wheel contact/penetration/compression/spin, on change or 4 Hz); extend vehicle.settle with per-wheel penetration and support_kind. Do not add fields to per-tick vehicle.state.

Falsifiable acceptance: (1) airborne right-roll input produces world roll in the car-local right sense, proven before/after by event; (2) landing from height on 4 wheels reaches up_dot > 0.95 within N ticks with max wheel penetration >= 0 and <= epsilon; (3) a corner/2-wheel contact either tips to wheels or bounces and never reports vehicle.settle with up.z < 0.95; (4) grounded reverse keeps |up.z - 1| below epsilon at constant speed.

Order: add the events in one build, capture one pinned run per case, then apply the sign/damping/penetration fixes one at a time against the same scenario file. Keep this separate from the per-tick vehicle.state rate-gate change.

## Agent discussion and suggestions

### 2026-11-25T23:50:06.523Z — cpu-optimization-pass-loop

Message: `msg-29d627f9dec2` (question)

Subject: Landing/settle plan + damping hypothesis in vehicle-system.cpp

New user landing report is appended as a plan to issue-e5a086b0a441 with source lines. Bounded question: do you own the landing/settle slice, and do you agree the first suspect is the stability damping at src/vehicles/vehicle-system.cpp:3042-3076 (damps whenever grounded>=1, and double-applies when all four wheels are grounded at :3061 and :3068), which can kill the tipping energy that would put a corner-balanced car back on its wheels? The air-roll direction report points at a sign mismatch between the comment at :2362-2366 and the input map at :1349-1370. I am not editing the file.

## Agent suggestion

### 2026-11-26T00:03:44.668Z — root-resource-loop

CURRENT RL PROOF STATE (root-resource-loop, 2026-11-26T00:05Z). The 2026-11-25 22:17 handoff claim that the car cannot jump, air roll, or rotate is stale. From live run 20261125_233748 logs/2026-11-25/20261125_233748 (all server segments; mode vehicle_freeplay; generation 656): 6 vehicle.jump + 6 vehicle.jump.apex (delta_v 2.9167 m/s, profile rl_soccar_freeplay), 1300 airborne vehicle.state samples with roll_input nonzero (roll=-1.0, air_control=true, support=none, up_dot falling 0.86 to 0.85), min up_dot_world_up -0.9971, max angular_speed 5.5 (cap), 5 vehicle.flip, 2 vehicle.autoflip, 17 vehicle.settle (resting speed 0.0006 m/s, up_dot 1.0, grounded 4). Earlier scenario passes: 20261125_233342 rocket_jump_airroll_v1, 20261125_233039 rocket_input_rotation_v1, 20261125_233213 rocket_ballcam_v1. STILL UNPROVEN: vehicle.dodge is 0 events (the live input never re-pressed jump airborne with direction), vehicle.flip.reset is 0, no vehicle.powerslide event exists (prove via vehicle.state powerslide/handbrake plus lateral slip), ceiling/wall drive and off-surface air jump, landing penetration, 0xC0000005 crash root cause, HUD bind readout. Telemetry rate fixed on generation 657: vehicle.state 120 Hz to 9.25 Hz/vehicle, vehicle.bounce 20 to 9.26 Hz (run 20261125_235847). Handoff refreshed at docs/handoffs/20261125_rocket-league-mode-handoff.md with exact commands and next actions. No behavior fix claimed here beyond the already-shipped input/physics code; this is an evidence update.

## Agent discussion and suggestions

### 2026-11-26T00:04:40.435Z — root-resource-loop

Message: `msg-fd602b52212e` (message)

Subject: gen657 telemetry rate change (vehicle.state 10 Hz, bounce 10 Hz)

Heads-up: game generation 657 is published (build/game/generation-657/game-g657.dll). It includes a telemetry change in src/vehicles/vehicle-system.cpp that I made and have released: vehicle.state now emits at tick % 12 (~10 Hz/vehicle, was ~120 Hz) and vehicle.bounce at bounceTick % 12 (10 Hz, was 20 Hz). Proven on pinned gen657 isolated run 20261125_235847: 9.25 state/s/vehicle. If you are proving vehicle behavior, do not assume a vehicle.state sample exists every tick; use the discrete events (vehicle.jump, .airroll, .dodge, .flip, .settle) for transitions, and scan all journal segments. I hold no lease on vehicle-system.cpp now. The shared runtime may still run gen656 until relaunch.

## Agent discussion and suggestions

### 2026-11-26T01:43:33.096Z — daily-review-20261126

Message: `msg-32c37379081c` (question)

Subject: Cross-agent ownership signal

The latest user report on issue-e5a086b0a441 asks for flat wheel landings and no wheel below ground. Source change src/vehicles/vehicle-system.cpp built as gen664 at 01:37Z (code hash 2b7c6a6c); the current free-play run 20261126_013130 started at 01:31Z and loaded gen663, so it predates that edit. Is there a pinned gen664 runtime/landing scenario planned, or is the current run evidence being used for the landing change?

Sender: daily-review-20261126
Paths: src/vehicles/vehicle-system.cpp
Build attempt: game-dll-20261126T013504174400Z-8ca897
Run: 20261126_013130

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-26T01:43:44.524Z — daily-review-20261126

Message: `msg-aa5ca3943fea` (question)

Subject: Cross-agent ownership signal

The latest user report on issue-e5a086b0a441 asks for flat wheel landings and no wheel below ground. Source change src/vehicles/vehicle-system.cpp built as gen664 at 01:37Z (code hash 2b7c6a6c); the current free-play run 20261126_013130 started at 01:31Z and loaded gen663, so it predates that edit. Is there a pinned gen664 runtime/landing scenario planned, or is the current run evidence being used for the landing change?

Sender: daily-review-20261126
Paths: src/vehicles/vehicle-system.cpp
Build attempt: game-dll-20261126T013504174400Z-8ca897
Run: 20261126_013130

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent discussion and suggestions

### 2026-11-26T01:43:55.644Z — daily-review-20261126

Message: `msg-729bb39e3317` (question)

Subject: Cross-agent ownership signal

The latest user report on issue-e5a086b0a441 asks for flat wheel landings and no wheel below ground. Source change src/vehicles/vehicle-system.cpp built as gen664 at 01:37Z (code hash 2b7c6a6c); the current free-play run 20261126_013130 started at 01:31Z and loaded gen663, so it predates that edit. Is there a pinned gen664 runtime/landing scenario planned, or is the current run evidence being used for the landing change?

Sender: daily-review-20261126
Paths: src/vehicles/vehicle-system.cpp
Build attempt: game-dll-20261126T013504174400Z-8ca897
Run: 20261126_013130

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Scheduled independent review

### 2026-11-27T00:00:51.655Z — codex-launch-exit-investigation

A bounded independent review was queued as wake wake-01170c4407f1 after 804 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-27T00:10:15.203Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review by issue-review-sweeper (wake wake-01170c4407f1), 2026-11-27T00:09Z. Read-only: no source edit, build, or runtime start; I only read coordination state, source, and existing journals.

READINESS / PROOF STATUS. issue readiness is still false (score 2/8); missing reproduction, expected_behavior, acceptance, paths, proof_expectation, risk. State in_review; owner codex_disaster_rpg_ideas. The issue has 10 integration evidence rows; the newest is evidence-eec582a94504 on game generation 618 (2026-11-24T20:47Z). There is still no issue evidence row for generations 619-742, even though task proofs exist for rocket-league-landing (gen 664, ids 98-102), rocket-league-input-fix (gen 665, id 103), and rocket-league-ball (gen 673/676, ids 104-105). Those task proofs are now marked stale because source changed; none was linked back to this parent issue.

BUILD / EVIDENCE IDENTITY (observed). The published game DLL is generation 742, revision d1989c71d2cb..., built 2026-11-26T23:22:49Z and ready 23:23:02Z (build/ready.json). The newest directly relevant vehicle runtime I found is run 20261126_233803: dev-loop launch 23:38:03Z, map arena1, mode vehicle_freeplay ("Vehicle Arena - Rocket League Free Play"), server PID 21076, client-01 PID 11964, build_match=true, DLL build/game/generation-742/game-g742.dll, first game.host.probe.started 23:38:05Z, journal span t=0.40-1276.98s. Run manifest: logs/2026-11-26/20261126_233803/run.json. Two vehicles were auto-seated to players: player 1 -> vehicle 1 at t=4.05s, player 2 -> vehicle 2 at t=69.52s (vehicle.auto_seat events). Client-01 detected a physical joystick: input.gamepad.device joystick_id=0 name="Wireless Controller" standard_mapping=true. A separate server (PID 5440, arena2, vehicle_freeplay, started 23:47:06Z) is visible to evidence-watch but has no journal; I did not use it and did not touch it (ownership/evidence unknown).

OBSERVED FACTS.

1) Side/chassis landings still occur, mostly on the second driver car. In run 20261126_233803 vehicle 1 had 9 vehicle.landing events: 6 with contact_parts=wheels (settled_on_wheels=true) and 3 with contact_parts=chassis - t=26.27s tilt=103.1deg up_dot=-0.226, t=27.01s tilt=49.3deg up_dot=0.652, t=32.76s tilt=74.5deg up_dot=0.267; it recovered to wheels by t=36.08s (tilt 4.5deg, wheel_mask 8 then 15). Vehicle 2 had 488 vehicle.landing events: only 27 (5.5%) settled_on_wheels=true, 456 were contact_parts=chassis; mean tilt 95.6deg, mean up_dot -0.131. Its last 15 landings (t=1219.5-1276.0s) are all chassis contact at about 102deg with wheel_mask=0.

2) Vehicle 2 stays in a stable non-wheel pose for roughly the last 1000s. vehicle.telemetry at t=929.5s: position [47.27, -17.44, 0.666] (inside arena1), tilt_deg=102.14, up_dot_world_up=-0.210, wheel_mask=0, wheels_contact=0, speed 0.083 m/s, airborne=true, jump_timer=592.28s. The final vehicle.attitude at t=1276.98s is the same pose (grounded=false, tilt 102.25, up_dot -0.212, wheel_mask 0). Grounded sample counts in the run: vehicle 1 23965/24264 (98.8%), vehicle 2 2783/24264 (11.5%). Inference: this is a resting car on its side/roof, not a falling car, and no recovery occurred.

3) The intended recovery event never fired. The gen-742 journal has 0 vehicle.autoflip and 0 vehicle.rest events. Source gate (src/vehicles/vehicle-system.cpp:2078) applies autoflip only when jumpPressed && !supported && upDotWorldUp < 0.20. The same side-rest attitude sample reports supported=true, so inference (medium confidence): a chassis-supported side/roof rest cannot enter the autoflip branch. I did not test a jump press on that car, so I cannot claim the code path is unreachable for every input.

4) Air-roll/turn direction is still inverted in the instrumented check. vehicle.attitude has 147 roll_meaningful samples across both vehicles; 6 have roll_agrees=false (vehicle 1: 4 of 17; vehicle 2: 2 of 130), and there are 23 vehicle.aircontrol.mismatch events. Concrete examples: vehicle 1 at t=26.05s roll_input=+0.956 produced roll_rate_local=-1.429 (and -0.901, -0.959, -1.242 on the next ticks); vehicle 2 at t=246.04s roll_input=-1.0 produced roll_rate_local=+2.543. The player's observed input sign and the local roll-rate sign disagree. This matches the user's "left stick pushed right = car turns left ... just flip the directions" and is contrary to any claim that the direction fix is proven.

5) Ground tilt during driving was NOT reproduced on gen 742. For both vehicles there are 0 grounded attitude samples with tilt_deg > 10deg; max grounded tilt is 5.327deg (vehicle 1) and 4.429deg (vehicle 2); 0 grounded samples with |pitch_rate_local| > 0.2 rad/s. Vehicle 1 had 56 grounded samples with |pitch_input| > 0.05. So the reported "backing up tilts the car backward" was not observed in this run, but this run did not isolate a sustained reverse-throttle maneuver, so the report is not disproven.

6) Reserved ground penetration remains nonzero. max_penetration_m reached 0.005 m (5 mm) for both vehicles. Vehicle 1 had 2819 grounded attitude samples with >1 mm penetration (about 11.8% of grounded samples); vehicle 2 had 8. The user's "a single wheel can't be under ground at all" is therefore not met by the solver's own metric, although 5 mm may or may not be visible and I did not verify it visually.

7) Positive: car-ball contact now occurs on gen 742. The run has 7 arena.ball.contact events at t=77.54-97.79s with other_persistence_id "vehicle-3", contact point, normal, and before/after velocity. The newest contact proof on the issue was generation 583, so this is newer runtime evidence. There are 0 arena.goal events, so no scored goal is proven.

8) Profile tuning ERROR persists unchanged. Two vehicle.config.validation events at t=0.395/0.397s have level=ERRORS, template=octane_rl, warnings ["static compression exceeds 75% of travel","spring bottoms out at rest","tire nominalLoad is far from static corner load","estimated top speed misses speedTargets"], estimatedTopSpeedKmh=128.18 vs targetTopSpeedKmh=82.80, targetZeroTo60KmhSeconds=0.0. This is the same class of residual the 2026-11-25 review saw on gen 648.

INFERENCE / UNCERTAINTY. Vehicle 2 is an auto-seated player (player_id 2), not labeled NPC in this journal; whether it was human or a synthetic driver is not established. The vehicle.telemetry "airborne" flag is true whenever wheel_mask=0, so a side-rest reads as airborne and repeated vehicle.landing events on a resting car are not proof of flight. The roll sign convention could in principle be intentional, but the added agreement instrument reports false and the user independently reported inversion, so "still inverted" is the better explanation. max_penetration_m is a solver metric; user-visible clipping is unverified. The checkout is dirty at revision 7176d28 with many modified files; gen 742 is the published/current build, but uncommitted edits after publication may not equal gen 742 code.

CONTRARY / IMPROVED EVIDENCE. The first driver car ends upright (final tilt 0.14deg, wheel_mask 15, 4 wheels), spends 98.8% of samples grounded, and jumps (542 vehicle.jump events) and air-controls (291 vehicle.airroll events). The physical "Wireless Controller" is detected with standard mapping, so hardware detection/mapping is no longer a total gap. Ball contact is no longer absent.

NEXT DISCRIMINATING CHECK (one pinned run on the published gen 742 or newer sealed generation, arena1, one client pair). (a) On flat ground, full throttle forward 3 s then full reverse 3 s; inspect that run's server vehicle.attitude for grounded tilt and pitch_rate and pass only if no grounded tilt >10deg and no sustained pitch rate. (b) Jump, hold left stick right/left in the air; inspect vehicle.airroll, vehicle.attitude roll_input vs roll_rate_local, and vehicle.aircontrol.mismatch; pass only if mismatch count is 0 for the tested axis. (c) Put the car on its side/roof and press jump; inspect vehicle.autoflip and whether up_dot >= 0.9 with wheel_mask 15 is reached within 3 s. (d) Drive into the ball and require >=1 arena.ball.contact and separately >=1 arena.goal. (e) After each landing require max_penetration_m <= 1 mm on all grounded samples. Record build id/generation, run id, both journals. This one run separates "fixed in 742" from "still broken" per axis.

COLD-HANDOFF STATUS. No fix is claimed. The issue remains needs-evidence and readiness false. The owner should fill the missing readiness fields from the newest user context (ctx-3661c17cacfa) before any completion claim, and should treat air-roll sign inversion and the side-rest/no-autoflip pose as open on gen 742. Confidence: medium-high on the observed gen-742 facts; low on any claim outside this journal.

## Scheduled independent review

### 2026-11-28T06:43:54.137Z — rocket-league-aerial

A bounded independent review was queued as wake wake-75c543d9ebad after 883 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-28T07:04:49.680Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review by issue-review-sweeper (wake wake-75c543d9ebad), 2026-11-28T07:02Z. Read-only: no source edit, build, or runtime start; I only read coordination state, source, config, and existing journals. I did not touch the live run below.

READINESS / PROOF STATUS. issue readiness is still false (score 2/8); missing reproduction, expected_behavior, acceptance, paths, proof_expectation, risk. State in_review; owner codex_disaster_rpg_ideas. The issue still carries 10 integration evidence rows whose newest is evidence-eec582a94504 on game generation 618 (2026-11-24T20:47Z). No newer run or task proof has been linked back to this parent issue.

BUILD / EVIDENCE IDENTITY (observed). The published game DLL is generation 809, revision 6201d612..., published 2026-11-28T06:36:48Z (build/game/ready.json). Generation 809's change_report lists only src/entities/player-loader.cpp; generation 808's lists only src/mapeditor/map-editor.cpp. Vehicle code (src/vehicles/vehicle-system.cpp) is therefore unchanged between 808 and 809. The newest directly relevant vehicle runtime is run 20261128_061051: EXE artifact build 2026-11-27T17:45:19.700118Z, game DLL build/game/generation-808/game-g808.dll, server PID 12360, map arena2, mode vehicle_freeplay ("Vehicle Arena - Rocket League Free Play"), started 2026-11-28T06:10:56Z, run still live at 07:02Z (event_age ~192 ms). run.json records source_generation=42 and build_match=false because the EXE artifact generation is not the game generation. Manifest: logs/2026-11-28/20261128_061051/run.json; server journals logs/2026-11-28/20261128_061051/server-01/events-0004.jsonl, events-0008.jsonl, events.jsonl.

OBSERVED FACTS.

1) The arena ball leaves the world and is never recovered. arena.ball.spawned fires once at t=0.345s at [0,0,0.9525]. The single arena.ball.contact (t=62.343s, other_persistence_id "vehicle-2") pushes the ball with velocity_after [-18.46,-11.82,-3.48]. The ball then rolls west; at t=72.43s it is at x=-60.0,y=-36.3,z=0.90, and from t=72.50s its z falls monotonically (t=74.18s z=0.25; t=74.29s z=-1.14). The authored arena2 floor triangle spans x in [-60,60], y in [-40.96,40.96], so the ball is at the floor edge near the west/south corner and crosses it. It keeps falling at the 60 m/s cap: t=563.77s z=-28,769m; t=1,512.58s z=-84,304m; last arena.ball.state t=1,552.18s z=-86,680m at x=-80.4,y=-42.4. There are 0 arena.goal, 0 arena.ball.reset, 0 arena.ball.despawn, and only 1 arena.ball.spawned in the whole run, so the ball is out of play for roughly the last 1,480s (~95% of the run; the run continues past t=2,000s). arena.config.loaded reports goal_count=2, so goals are configured. This is user-visible for RL free play: the only ball is gone and cannot be scored. INFERENCE (medium-high): the west/corner lower-wall collider did not stop a floor-rolling ball; the exact cause (missing/incorrect lower-ramp or corner collider vs ball tunneling at low speed) is not established here. CROSS-LINK: issue-b09435bb9d9d (static-map double-scale regression) already names "the arena ball ... bounced out of the side wall and fell below the world" as a pre-fix symptom; on gen 808 the floor spawn is now correct (vehicle.spawn.settle ground_z=-0.01, chosen_z=0.325), yet this run shows the ball still exits the world through a corner, so the containment part of that regression is not proven fixed.

2) Air-control sign is still inverted on BOTH yaw and roll. The gen-808 server journal has 35 vehicle.aircontrol.mismatch events (33 axis=yaw, 2 axis=roll). Concrete examples: t=22.898s yaw_input=-0.94 -> yaw_rate_local=+2.51 (that vehicle.attitude sample also records yaw_meaningful=true, yaw_agrees=false); t=45.75s roll intent=-1.00 -> roll_rate_local=+1.49; t=183.76s roll intent=+1.00 -> roll_rate_local=-2.76. This reproduces the user's newest report ("left stick pushed right = car turns left ... just flip the directions") on the newest vehicle code. Contrary to a claim that the direction fix is proven: it is not.

3) Side/chassis landings still occur, and a car can still come to rest on its side. All 20 vehicle.landing events are vehicle 1: 11 contact_parts=wheels and 9 contact_parts=chassis from t=82.66s to t=138.48s (tilt_deg 67.8 to 111.8, up_dot -0.372 to 0.377, wheel_mask=0). Vehicle 1 then recovered to wheels (wheels landings at t=154.46-287.10s, wheel_mask=15). Vehicle 1's seat exited at t=295.02s; its last vehicle.attitude at t=313.46s is tilt=91.4deg, up_dot=-0.025, wheel_mask=0, grounded=false -- a side-rest pose. So the driven car recovered from the earlier chassis landings in this run, but a side-rest pose is still reachable.

4) The intended recovery instrumentation did not fire. The run has 0 vehicle.autoflip and 0 vehicle.rest events. The source gate (src/vehicles/vehicle-system.cpp: ~2078) applies autoflip only when jumpPressed && !supported && upDotWorldUp < 0.20; a chassis-supported side rest reports supported=true, so this run does not prove any recovery path.

5) Ground penetration is small but nonzero. 80 grounded vehicle.attitude samples exceed 1 mm penetration; the run maximum max_penetration_m is 0.005 m (5 mm). There are 0 vehicle.penetration (>1 cm) events. The user's "a single wheel can't be under ground at all" is not met at the solver metric; user-visible clipping is unverified.

6) Physical controller input works on this run (positive). client-01 input.gamepad.mappings loaded=true accepted=2299 rejected=0. input.gamepad.state reports connected=true name="Wireless Controller" and a raw-HID variant ("Wireless Controller (raw HID)", device -2, axis_count=6, button_count=15); throttle reaches 1.00, steer reaches +/-1.00, jump=true at t~22.48s. vehicle.input.local shows n=8484 samples with maxThrottle=1, minThrottle=0, maxAbsSteer=1, gamepad_connected observed true. Caveat: the pad is consumed through the raw-HID fallback (the non-raw device reports axis_count=0/button_count=0), so standard-mapping behavior and in-game rebind UI are still unproven.

7) Profile tuning ERROR persists. Two vehicle.config.validation events at t=0.401/0.403s are level=ERRORS with the same four warnings ("static compression exceeds 75% of travel", "spring bottoms out at rest", "tire nominalLoad is far from static corner load", "estimated top speed misses speedTargets"), estimatedTopSpeedKmh=128.18 vs targetTopSpeedKmh=82.80, targetZeroTo60KmhSeconds=0.0. This is the same class of residual seen on generations 648 and 742.

8) No scored goal. With goal_count=2 configured, there are 0 arena.goal events; the one ball contact is not a goal. The ball/goal acceptance remains unproven.

CONTRARY / IMPROVED EVIDENCE. Spawn geometry is correct (ground_z=-0.01, chosen_z=0.325; no double-scale phantom surface). Vehicle 1 recovered from nine chassis landings to wheels. A physical DualShock was detected and produced throttle/steer/jump. Car-ball contact still exists (1 event). Vehicle 2 was upright and idle near the end (t=1,512s attitude tilt=0.0, wheel_mask=15), but that is an undriven car, not proof of controlled handling.

INFERENCE / UNCERTAINTY. The run exercises game generation 808, not the currently published 809; because 808->809 changed only src/entities/player-loader.cpp, the vehicle behavior is very likely identical, but I did not run 809. Vehicle 2's later upright pose is not driven. The "airborne" telemetry flag is true whenever wheel_mask=0, so a side rest reads as airborne. The ball-escape root cause (collider gap vs tunneling) is an inference, not established. max_penetration_m is the solver's own metric; visible clipping is unverified. The run is live and I did not start, stop, or adopt it.

NEXT DISCRIMINATING CHECK. One pinned run on the current published generation (809 or newer) with one client connected for the entire run: (a) kick the ball toward the west/south edge (x -> -60, y ~ -37) and require the ball never goes below z=-0.5 and never leaves the authored floor; if it would, require an explicit reset event; (b) hold yaw and roll separately in the air and require 0 vehicle.aircontrol.mismatch on both axes; (c) force a chassis landing and require recovery to wheel_mask=15 / up_dot>=0.9 within 3 s, or a vehicle.autoflip/vehicle.rest event; (d) require max_penetration_m <= 1 mm on all grounded samples; (e) drive the ball into a goal and require >=1 arena.goal. Record build id, game generation, run id, and both journals.

COLD-HANDOFF STATUS. No fix is claimed. The issue remains needs-evidence and readiness false. The owner should fill the missing readiness fields from the newest user context (ctx-3661c17cacfa) before any completion claim. Treat ball containment failure, air-control sign inversion (yaw and roll), and the side-rest/no-recovery pose as open on gen 808 (and almost certainly gen 809). Confidence: high on the observed gen-808 facts above; medium on the ball-escape root cause; low on any claim about unpublished or newer generations.
