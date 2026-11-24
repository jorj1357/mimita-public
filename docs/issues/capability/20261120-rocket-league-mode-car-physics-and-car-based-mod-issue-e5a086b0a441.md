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
