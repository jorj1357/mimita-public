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
