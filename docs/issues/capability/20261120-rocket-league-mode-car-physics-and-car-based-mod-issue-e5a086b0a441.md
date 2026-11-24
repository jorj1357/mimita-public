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
