# Vehicle Arena Handling and Any-Controller Handoff

**Status:** Cold implementation handoff from a read-only investigation. No
gameplay source, config, or specifications were changed in this investigation.
**As of:** 2026-11-24 UTC.
**Audience:** The next implementation agent working on car handling, input, and
Vehicle Arena.
**Parent issue:** [`issue-e5a086b0a441`](../issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md)

## Purpose and boundaries

Turn the user's clarified priority into an implementable first slice: make the
existing shared vehicles feel predictable and controllable in Vehicle Arena,
then support common controllers with editable bindings, and validate the real
path. The user wants this mode to become a repeatable place to investigate
vehicle networking. Treat the simple one-human-versus-one-NPC match as a
playable follow-up to handling; the advanced freestyle bot, 3v3 roster tools,
and claims that MiMITA networking is better than another game are later work.

This handoff does not choose final physics constants, change a gameplay spec,
claim a runtime pass, or authorize a new physics engine. Keep one shared vehicle
solver and the existing server authority. Do not copy Rocket League or RocketSim
force constants directly; the Vehicle Arena spec explicitly says their units
and scales are not MiMITA's.

User's clarified priority (verbatim user context is also linked on the parent
issue):

> th main issue was vehciles  the phsics for vehicles we need to do  that but idk how to do that like&#x20;
>
> do we ahve enoigh like what the controls are?
>
> we could also  binds here bc &#x20;
>
> aim for all controllers any controller

## Current evidence

Claims are labeled so the implementation agent can separate source facts from
runtime proof.

| Claim | Status | Evidence and limit |
|---|---|---|
| Vehicle Arena, its map, and an arcade profile are configured. | **Observed** | [`vehicle_arena.json`](../../config/gamemodes/vehicle_arena.json) selects `arena1` and `vehicle_profile: arcade_arena`; [`arena1.json`](../../config/maps/arena1.json) contains a ball, goals, kickoffs, and two authored `car_sedan` vehicles. `config/onlinemodes.json` has a Vehicle Arena entry. This proves configuration exists, not that a fresh GUI selection or complete match works. |
| The named arcade profile currently changes runtime behavior. | **Not found; likely missing** | `Gamemode::vehicleProfile` is populated by [`gamemode.cpp`](../../src/gamemode/gamemode.cpp), but a repository source search found no consumer of `vehicleProfile`. The 2026-11-05 Vehicle Arena changelog explicitly says the profile file “is not consumed by the vehicle system yet.” Verify this with a fresh runtime/config event before tuning profile values. |
| The listed arena actions are all carried through the shared vehicle input and packet. | **Observed gap** | [`VehicleInput`](../../src/vehicles/vehicle-types.h) and [`VehicleInputPacket`](../../src/network/schema/packet-vehicle.h) carry throttle, brake, steer, clutch, handbrake, engine/mode toggles, and shift flags. They have no boost, jump, or air-control fields. [`VehicleSystem::readLocalInput`](../../src/vehicles/vehicle-system.cpp) samples existing keyboard commands and hard-coded keys. The profile's `bindings` and arcade actions were not found in source consumers. Do not add a parallel controller-only solver or duplicate action state. |
| Existing key rebinding covers controllers and the arena action set. | **Observed gap** | [`config/input/binds.json`](../../config/input/binds.json) and `InputCommandSystem::loadBinds` map action names to keyboard key names only. Vehicle profile binding names are not read in source. There is no in-game controller bind/calibration surface in the paths inspected. |
| MiMITA already has a usable controller API dependency. | **Observed API; integration unproven** | The vendored [`GLFW header`](../../include/GLFW/glfw3.h) is GLFW 3.4 and declares standardized gamepad state and raw joystick APIs. No application call sites for `glfwGetGamepadState`, `glfwJoystickIsGamepad`, or raw axis/button polling were found. This should avoid an extra library for mapped devices; confirm the linked runtime version before use. “Any controller” needs a raw-device/manual-bind fallback for devices without a standard mapping. |
| The hot game module receives gamepad input from the resident host. | **Observed gap** | [`GameInputKind`](../../src/host/abi/game-host-abi.h) currently covers key, character, scroll, drop, cursor, and mouse-button events. The host-dispatched module handler has no gamepad case. The game DLL also has a separate GLFW copy; do not assume polling it sees the resident window's initialized joystick state. Establish the owning GLFW instance and safe host-to-module path first. |
| A car and ball can be started on the configured arena map. | **Historical runtime evidence only** | [`20261105_210536-vehicle-arena-phase0-editor-proof.md`](../changelog/2026-11-05/20261105_210536-vehicle-arena-phase0-editor-proof.md) records launch mode 50 on `arena1`, two connected players, arena config load, and a later server ball spawn (`20261105_221144`). These runs are old and do not prove current source, driving feel, goal scoring, controller input, or network reconciliation. |
| The NPC feature is a team-rostered NPC driver. | **Observed gap** | [`serverTickVehicles`](../../src/network/vehicle-networking.cpp) seats one synthetic driver ID into the free car nearest orange kickoff, then steers/throttles toward the ball. It uses `VehicleInput`, but it is a simple ball chaser, not a normal team-assigned NPC actor. The mode config sets both teams to capacity 1 with `npcs: false`; the vehicle code adds its driver independently. The dev profile also declares `npc_count: 0`. Thus roster and launch settings do not own this NPC behavior yet. |
| The full scoring loop is implemented. | **Unverified; treat as incomplete** | `arena1.json` authors goals, and [`ArenaBall::reset`](../../src/gamemode/arena-ball.cpp) exists. The source search found map parsing/validation, ball spawn, and `arena.ball.contact` diagnostics, but no authoritative goal-detection/reset caller or `arena.goal.detected` / `arena.goal.reset` producer. A map goal definition or ball spawn is not proof of scoring. Trace this path before claiming a playable match. |
| Local car control is predicted and reconciled online. | **Spec target; implementation not shown by current evidence** | The Vehicle Arena/networking specs require local car prediction and car-ball reconciliation. `VehicleSystem` deliberately treats `serverMirror` chassis transforms as server-owned and does not apply local suspension/tire/engine forces. `mpSendVehicleInput` sends the sampled local input in a `VehicleInputPacket` every 15 ms; server simulation remains authoritative. The feature record still lists arena prediction/reconciliation as pending. Measure current response; treat prediction as a separate implementation phase. |
| A person has accepted the arcade handling. | **Not yet proven** | [`docs/features/vehicles/vehicles.md`](../features/vehicles/vehicles.md) records human acceptance as pending. The feature record's “no arena ball/goal/match runtime” bullets are stale relative to later source and the 2026-11-05 changelog; refresh that record only with current evidence, preserving the distinction between source, runtime, and human feel. |

### Important checkout state

At investigation time the checkout was on branch
`checkpoint/one-checkout-condensation-20261111`, HEAD `186eebd2`, with many
pre-existing modifications. Relevant dirty paths included
`src/input/input-commands.cpp`, `src/input/input-frame.h`,
`src/input/input-poll.cpp`, `src/gamemode/gamemode.cpp`,
`src/gamemode/gamemode.h`, `src/gamemode/gamemode-runtime-resolver.cpp`,
`src/gui/gui-main.cpp`, and `src/network/server-gamemode.cpp`. These are not
this handoff's changes. Re-run `git status`, inspect the exact diffs, sync the
coordination board, and acquire edit leases before touching any shared path.
The `src/host/abi/game-host-abi.h` path was clean in the observed status, but
that can change before implementation.

The parent issue is now `triaged` with the user's handling/controller
clarification appended, but its coordination record is **not ready to claim**:
it still lacks reproduction, expected behavior, acceptance, paths, evidence,
proof expectation, and risk fields. The handoff below supplies those details;
the implementation owner should repair/complete the issue record through the
supported issue workflow before taking behavior work. The recorded owner is
`codex_disaster_rpg_ideas`; do not silently overwrite that ownership.

## Existing control contract and proposed default pad layout

The current Vehicle Arena spec already names the action vocabulary:

| Action | Ground | Air | Suggested controller default |
|---|---|---|---|
| Throttle / brake | Forward / brake-reverse | Pitch forward / backward | RT / LT |
| Steer | Left / right | Roll left / right; no yaw per spec | Left stick X / Y |
| Powerslide / air roll | Reduce grip / hold air roll | Hold to enable air roll | X / Square |
| Boost | Boost | Boost | B / Circle |
| Jump / second jump | Jump, then double-jump/flip when eligible | Same | A / Cross |
| Ball cam | Toggle ball/car camera | Same | Y / Triangle |

Use GLFW's standardized gamepad button labels internally, and show glyphs or
labels suited to the detected layout. These are defaults only: users must be
able to rebind actions. Ground and air should reuse the same logical action
names with the mode changing how an action is interpreted, as the existing
Vehicle Arena spec describes. Preserve keyboard and mouse input as another
binding source.

“Any controller” should mean: known/common gamepads use GLFW's standard mapping;
unmapped joysticks can be calibrated and bound by raw axis/button; disconnects
neutralize held actions; deadzones and trigger ranges are configurable. Do not
promise every physical device has a built-in mapping. Test representative
Xbox-style, PlayStation-style, and Nintendo-style pads plus one unmapped device
when hardware is available. This investigation did not inspect attached-device
inventory or test hardware.

## Recommended implementation order

### 0. Repair issue context and capture a real baseline

1. Read this handoff, the parent issue brief, current shared board/leases, and
   the exact dirty diffs. The issue readiness gap above must be visible to the
   implementer; do not make a duplicate issue to bypass it.
2. Check `python devscripts/dev-loop.py --status --json`; reuse the root
   dev-loop. The existing `devscripts/dev-profiles/vehicle-arena.json` uses
   launch mode 50, `arena1`, mode/gamemode `vehicle_arena`, weapon set 1,
   `npc_count: 0`, and allows two processes. The server's current synthetic
   vehicle driver is independent of that NPC count, so record actual live
   actor/vehicle state instead of trusting the launch field.
3. Start an isolated run only through the root dev-loop, pin artifact, source
   generation, run ID, and per-process journals. Exercise the vehicle in the
   real executable and record its actual input, authoritative transform,
   speed, steering, wheel contact, and ball contact. No fresh build or runtime
   was launched by this handoff investigation.
4. Verify which mode-selection flow the current GUI exposes. `config/onlinemodes.json`
   has a Vehicle Arena entry and dev profile 50 exists, but the 2026-11-05
   evidence used a dev launch, not current GUI-selection acceptance.

### 1. Make the arcade profile real before tuning it

Trace `vehicle_profile` from mode data to the active shared vehicle/input
owners. Load and validate the named profile once through the existing config
owner; emit a bounded structured event such as `vehicle.profile.applied` with
mode, profile ID, and effective values so the runtime journal proves which
settings were active. Keep template mechanics (`VehicleSpec`) separate from
mode-selected arcade actions and scales. Do not add a second car solver or
`if (vehicle_arena)` physics branch.

Trace the full action path: keyboard/controller sampling -> logical actions ->
fixed 60 Hz vehicle tick -> input packet/authority -> authoritative vehicle
state -> client presentation. The current `VehicleInput`/`VehicleInputPacket`
does not carry boost, jump, or air-control actions. Reuse the existing shared
action owner where possible; if wire fields must change, update the packet
schema/ledger and client/server together. A controller-only local action that
never reaches the server is not online support.

### 2. Route controller input through one owner and make it rebindable

First prove which resident GLFW 3.4 instance owns joystick polling. Prefer that
owner for `glfwJoystickIsGamepad` / `glfwGetGamepadState` and raw joystick
fallback; forward normalized logical actions through the existing host/module
input boundary. Do not poll a second GLFW copy or create a second hardware
input source without evidence it works. If the host ABI must change, read
`docs/specs/hotreload/hotreload.md`, inspect capability/version negotiation,
and run `python devscripts/dependency_graph.py --affected
src/host/abi/game-host-abi.h` before editing; follow header hygiene and record
fanout. Coordinate the host/input owners first.

Extend the existing versioned keyboard bind config/UI with gamepad actions,
axis direction, deadzone, trigger calibration, and button capture. Preserve
existing keyboard binds and account overrides. Add bounded diagnostics for
device name/mapping identity, logical action values, connection changes, and
input source; never log unmanaged debug files. Poll at the host frame boundary,
but consume/edge-detect gameplay commands at the established fixed simulation
tick so held jump, toggles, and disconnects do not duplicate actions.

### 3. Tune handling one variable at a time

The current [`car_sedan.json`](../../config/vehicles/car_sedan.json) is a
general sedan template (5 m x 3 m x 1 m, 2,600 kg, 0.62 m wheel radius,
420,000 N/m springs, tire `mu=2.0`, 45 degree template steering). The arcade
profile asks for 35 degree max steering, speed falloff 0.45, and return rate
6.0, plus boost/jump/air/wall values. Since the profile currently has no
consumer, do not infer those profile values are active or tune against them
until a runtime event proves the selected profile.

Record a repeatable human driving route on `arena1` and use it for before/after
comparisons through the real 60 Hz path. Start with measurable baselines for:

- steering response/yaw rate at low, medium, and high speed, including
  straightening and countersteer;
- acceleration and stopping distance from a recorded speed;
- grip, powerslide onset, and return to stable heading;
- jump height/hold response, aerial pitch/roll authority, and recovery time;
- boost acceleration and depletion; wall contact, slide, and recovery;
- car-ball hit point, incoming velocity, pre/post ball velocity, and first-hit
  tick for normal and contested approaches.

Change one tuning value per candidate and capture raw values, units, sample
count, run IDs, generation/SHA, and journals. The hypothesis should name one
effect (for example, reduced high-speed steering oscillation without losing
low-speed turn response) and be judged against the baseline. Numeric acceptance
targets for “nice and controllable” are **not yet agreed**; get the baseline
first, then ask the user to accept visible feel separately. Use the existing
`vehicle_tune` scale command only for bounded diagnosis; do not treat temporary
tuning as persisted profile proof. Do not substitute an AI-authored route for
human handling acceptance.

### 4. Make the 1v1 NPC setup consistent, then grow rosters

The current synthetic vehicle driver is enough to exercise the shared
`VehicleInput` surface initially, but it is not a team-aware soccer NPC. Make
mode data/launch settings the single owner of team capacities and NPC roster:
the requested solo start should be one human versus one orange NPC car; a
0-NPC practice option should remain available for clean handling measurement.
Remove the current mismatch where mode config says `npcs:false` and launch
profile says `npc_count:0` but `serverTickVehicles` still creates a synthetic
driver for every `vehicle_arena` run. Do not build a “pro freestyling” bot in
this first slice.

Only after 1v1 spawn/seat/drive/disconnect is proven, add server-authoritative
team roster operations such as `3v3`, `npc add blue|orange`, and
`npc remove blue|orange [id]`. Reuse existing roster and NPC lifecycle owners;
the generic `npc_spawn` / `npc_delete_all` commands are not team roster tools.

### 5. Validate online handling and networking separately

The arena spec requires two clients to agree on authoritative ball outcomes,
including a contested 50/50, no duplicate impulse/goal, and proper reset. Use
the routed multi-client and bad-connection workflows after the single-client
handling gate passes. Collect at least:

- local `vehicle.input.local`, server `vehicle.input.server`, server-applied
  vehicle state, and each client-visible transform with timestamps/ticks;
- action-to-server-accept and action-to-visible-response latency (p50/p95),
  RTT/jitter/loss preset, and correction distance/time;
- one car-ball contact ID/tick/point and pre/post velocity on server and both
  clients; no second confirmed impulse;
- goal/reset count and resulting ball/match state once goal scoring has a
  proven owner.

The present path is server-authoritative and sends vehicle input every 15 ms;
client car prediction/replay is specified but not established here. Measure
that baseline before designing or claiming prediction. A clean local match
does not prove robustness under loss; use `bad-connection-simulation.md` and
report each impairment preset separately. Do not claim the game has better
networking than Rocket League from these results alone; report MiMITA's measured
behavior and comparison conditions.

## Reproduction and acceptance checklist

**Starting configuration:** `vehicle-arena` dev profile, launch mode 50,
`arena1`, `vehicle_arena`, `arcade_arena`, one human client. Verify exact
effective identity in the current dev-loop status and the run manifest. For
NPC-on use, record the real seated driver IDs and teams; the current launch
profile's `npc_count` does not prove the synthetic car driver is absent.

**Source acceptance:**

- the mode's `vehicle_profile` is loaded and a journal event proves effective
  profile ID and values;
- keyboard and a mapped controller drive the same logical action surface;
- an unmapped controller can be bound/calibrated without source edits;
- throttle/brake are analog where supported, steering has a stable deadzone,
  and unplug/replug returns inputs to neutral;
- boost, jump, air control, powerslide, and ball cam reach the shared vehicle
  owner and (online) server packet/state path;
- no parallel arena vehicle physics solver or duplicate action state exists.

**Runtime acceptance:**

- pinned current executable/build + game generation + run journals show
  control input reaching the fixed-tick vehicle owner and resulting motion;
- human playtest accepts low/high-speed steering, braking, slide, jump/air
  control, boost, and recovery;
- a human can select/start Vehicle Arena through the intended user flow and
  play one human vs one NPC; a separate 0-NPC handling run remains possible;
- a later two-client run agrees on server-confirmed ball contact and scoring
  under clean and controlled impaired connections.

Build, runtime, and human acceptance are separate claims. For gameplay work,
use the root `devscripts/dev-loop.py` only; follow the repository's mode-3
batch build and pinned-run rules. For network/action/header changes, read the
router-selected networking, input, hot-reload, packet-ledger, and header
hygiene docs before edits. Record a build note before the first DLL compiler
input. Do not add synthetic tests as the first proof of gameplay.

## Paths and routed reading

- [Vehicle system and 60 Hz input contract](../specs/vehicles/vehicles.md)
- [Vehicle Arena controls, tuning baseline, and multiplayer acceptance](../specs/gamemodes/vehicle-arena.md)
- [Shared gamemode lifecycle](../specs/gamemodes/gamemodes.md)
- [Network prediction and reconciliation](../specs/networking/networking.md)
- [Vehicle input packet wire contract](../../src/network/schema/packet-vehicle.h)
- [Controlled bad-connection presets](../specs/networking/bad-connection-simulation.md)
- [Real-executable runtime scenario workflow](../workflows/runtime-scenario-validation.md)
- [Measurement/hypothesis workflow](../workflows/measurement-hypothesis-driven-improvement.md)
- [Multi-client test workflow](../workflows/multi-client-dev-testing.md)
- [Current feature record (known stale statements above)](../features/vehicles/vehicles.md)
- [Existing parent capability issue](../issues/capability/20261120-rocket-league-mode-car-physics-and-car-based-mod-issue-e5a086b0a441.md)

## Investigation outcome

- Repository/source/spec/changelog review completed; no code/config/spec edits,
  build, new test, or game runtime were performed.
- Existing evidence is dated 2026-11-05 and does not prove current handling,
  controller support, NPC team behavior, scoring, prediction, or human feel.
- The current shared checkout is dirty. The next agent must recheck source,
  coordination ownership, dev-loop artifact state, and all paths before editing.
