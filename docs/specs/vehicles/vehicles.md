2026 11 03 1627 jorj1357 jornal
Vehicles
Gamemode: 
Need for madness?
Cops vs robbers?
Demolition derby ?
Car tdm?
Wait this is sick
Each person has their own car
And its a huge big ass map
And u can drift and shoot at people?
Or it should be ur car ahs teh weapons i like that more
ROCKET LEAGUE
Rocket lauge.
Rocket league.
We neee dto do rkct elauge 
Vehicles are entities, physical objects, 
All ahve mass, center of mass, etc,
Attachments
Realistic suspension and gear switching and RPM and stuff 
Make it like beamng


# Vehicles

**Status:** active specification; implementation is partial and phased
(2026-11-03)
**Parent specification:** `docs/specs/moving-physical-objects/moving-physical-objects.md`
**Related:** `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`,
`docs/specs/destructible-world/destructible-world.md`,
`docs/specs/networking/networking.md`, `docs/specs/entity-editor/entity-editor.md`,
`docs/specs/weapons/weapons.md`, `docs/specs/gamemodes/vehicle-arena.md`,
`docs/specs/vehicles/vehicles-skateboard.md`,
`docs/specs/vehicles/vehicles-bmx.md`

## 0. Current direction (2026-11-03)

The first playable target is an **online, weaponized car**:

1. a car exists and has readable wheels,
2. a player enters the driver seat with the shared F interaction,
3. the car drives, steers, brakes, and recovers,
4. the same vehicle is server-owned and observed by multiple clients,
5. the seated driver aims and fires existing weapons through the normal weapon
   muzzle and collision path,
6. a reusable template can be built from primitive parts and tuned in JSON,
7. NPCs can drive the same vehicles through the same input surface,
8. a later arena mode adds a ball, goals, and scoring.

The car must be fun to drive before visual polish, the builder UI, NPC driving,
or the arena are treated as complete. The order is a dependency order, not a
claim that every intermediate step must ship separately.

The existing journal at the top of this file is preserved as original intake.
It is historical context, not an additional requirement.

### 0.1 First-playable acceptance

- A car is placed from the map editor and can be spawned from that authored
  placement.
- Looking at the driver seat and pressing F enters it; F exits.
- The driver can accelerate, steer, brake, reverse, and recover.
- Wheels are visible in the world and visible in the map editor.
- The car can be tuned from `config/vehicles/*.json` and the behavior changes
  without code edits.
- Server authority, weapon use from the seat, NPC driving, and arena rules are
  separate acceptance gates described in the phase table.

### 0.2 Deferred capabilities

- aircraft and flight controllers,
- motorcycles, skateboards, scooters, and BMX,
- vehicle-mounted weapons,
- arbitrary vertex/edge/face mesh editing,
- soft-body deformation and BeamNG-style continuum simulation,
- advanced per-part destruction and detached wheels.

Deferred items stay in the general library design, but are not first-playable
requirements.

## 1. Purpose

Vehicles are a **general physical library**, not a mode-specific feature.

The same primitives must support:

- cars and trucks with different mass and density,
- payload carts and drivable payload vehicles,
- motorcycles, skateboards, scooters, and BMX bikes (deferred),
- future air vehicles such as jets and helicopters (deferred),
- doors, drawbridges, turntables, and moving machinery,
- ragdoll limbs and grabs (through the same constraint layer).

The long-term rule from `moving-physical-objects.md` applies here:

> one physical world, one set of fundamental rules, many emergent behaviors.

A vehicle must not introduce `carPhysics()`, `truckPhysics()`, or
`if (vehicle)` branches. A vehicle is an assembly of general bodies, collision
shapes, constraints, and data-driven controllers.

The original wishlist that motivated this work is preserved in section 19.

---

## 2. Goals

1. A convincing sim-lite driving feel at the fixed 60 Hz gameplay tick:
   suspension, weight transfer, tire grip, engine inertia, turbo spool, and
   gear changes.
2. Data-driven tuning. A neutral sedan is the first template. Car and truck
   presets differ by JSON mass, density, suspension, tire, and engine values,
   not by code.
3. General authoring. Cars are defined in `config/vehicles/*.json`, placed from
   the map editor, and built from reusable primitive parts.
4. General constraints. Hinge, slider/spring, point, and motor constraints are
   first-class and reusable outside vehicles.
5. Server authority and replication from the first playable slice.
6. Console-complete: every important vehicle and constraint action is reachable
   through the terminal using the same shared actions as the GUI and input.
7. NPC drivers use the same vehicle input and physics surface as players.

## 3. Non-goals

- Literal BeamNG soft-body continuum simulation. The first target is a
  high-quality sim-lite that can later be upgraded to full rigid wheels or soft
  body without changing the ownership boundaries.
- External physics engines. MiMITA keeps one in-house collision world so server
  authority, prediction, and `PhysicalEntity` replication stay consistent.
- Full destruction in the first slice. Hooks ship first; CSG deformation and
  detached wheels follow.

## 3.1 External physics library decision

Do not add an external rigid-body engine for the current vehicle work. The
existing physical-entity system, fixed-tick simulation, cached collision
broadphase, constraint layer, and destructible-world owner are intended to be
the shared physics foundation. A second engine would need to share contacts,
authority, prediction, and replay with those systems; using it only for
vehicles would split the world and complicate collisions with players, maps,
weapons, and debris.

Reconsider this decision only if measured vehicle requirements cannot be met
by extending the shared owners. Any reconsideration begins with a bounded
technical comparison of the missing capability, integration cost,
determinism, networking, performance, and migration effect on other physics
users. A library is not adopted based on vehicle feel alone or as an
unmeasured shortcut.

---

## 4. General library rule

Vehicle physics must reuse the existing owners before adding new logic.

| Concern | Existing owner | Vehicle use |
|---|---|---|
| Moving object transform, mass, velocity, impulse, sleep | `PhysicalEntitySystem` / `PhysicalEntity` | chassis and detached parts |
| World collision broadphase | `World::collisionMesh`, chunks, `AabbTree` | chassis and wheel contacts |
| Fixed gameplay tick | `simulateTick` and `PhysicalEntitySystem::advanceKinematics` | vehicle simulation |
| Destruction | `ImpactSystem` / `DestructibleGeometry` | chassis damage and fracture |
| Replication | `PhysicalEntity` spawn/state/despawn | chassis and parts |
| Input | `InputCommandSystem`, `InputFrame`, `InputState` | vehicle controls |
| Logging | `StructuredLogger` | vehicle diagnostics |

New code is justified only for the missing primitives:

- analytic cylinder contact,
- generalized constraints,
- suspension and tire forces,
- engine/gearbox/turbo,
- seat occupancy and driving control source.

---

## 5. Physics primitives

### 5.1 Analytic cylinder

A wheel is an analytic cylinder, never an authored triangle mesh. Rendered wheel
geometry may be any mesh, but physics contact uses the cylinder definition:

```text
center
axis
radius
halfLength
```

The cylinder contact path must use the cached world broadphase and must not
allocate per query in the hot tick. Driving over triangle seams must not inject
bounce or snag.

### 5.2 Generalized constraints

Constraints are first-class records, not per-feature code. Minimum surface:

```text
Constraint
{
    id
    type            Fixed | Point | Hinge | Slider | Motor
    entityA
    entityB         (0 = world / static anchor)
    localAnchorA
    localAnchorB
    localAxisA
    localAxisB
    limitsEnabled
    minLimit
    maxLimit
    springStiffness
    springDamping
    restLength
    motorSpeed
    motorMaxForce
    breakForce
    breakTorque
    enabled
}
```

Required behavior:

- **Fixed (weld)**: preserve the authored relative transform between two
  bodies, constraining all three translation and all three rotation axes.
  Optional break force/torque allows a weld to become detached through the
  shared constraint owner.
- **Point**: two anchors coincide; linear only.
- **Hinge**: anchors coincide; relative rotation is free about the hinge axis and
  locked on the other two axes. Optional angular limits and motor.
- **Slider**: anchors stay on a shared axis line; free along the axis, locked on
  the other two. Optional spring-damper and linear limits. This is the general
  suspension primitive.
- **Motor**: angular drive about the hinge axis with a maximum force.

Constraints must wake sleeping bodies, use stable IDs, and be removable while
running. Runtime-created constraints replicate reliably; authored constraints
load from map JSON on both client and server.

### 5.3 Inertia

Vehicles must use the `PhysicalEntity` mass, density, center of mass, and
diagonal inertia already derived by `refreshEntityMassProperties` and
`MeshMassProperties`. Do not create a second inertia source. Moving to a full
3x3 tensor is a later physics-core change and must serve props, ragdoll, and
vehicles together.

---

## 6. Vehicle assembly

A vehicle is one chassis `PhysicalEntity` plus `N` suspension/wheel units:

```text
Vehicle
├── parts[]              (box, slope, sphere, cylinder primitives)
├── attachments[]        (named local frames: seats, wheels, equipment)
├── suspensionUnit[0..N] (unsprung mass, spring/damper, analytic cylinder)
├── drivetrain           (engine, turbo, clutch, gearbox, differential)
├── seats[]              (attachment reference, occupant, pose)
└── damage               (part health, detachable parts; later slice)
```

Parts contribute render geometry, collision triangles, and mass/center of mass.
A part may be marked visual-only. Spheres and cylinders are tessellated for the
triangle collision path; they do not introduce a second collision world.

Wheels are **not** ordinary body parts. They stay on the analytic cylinder and
suspension path so grip, spin, and steering remain stable. Wheels are visible
and editable as named attachments in the authoring surface.

Wheel count is data. Four wheels is a sedan template, not an engine rule. The
same assembly supports two wheels (bike), six wheels (truck), or thrusters
(air vehicle) by changing template data and the per-unit controller.

In the first slice a wheel is an **unsprung suspension unit with scalar spin**,
not a separately replicated rigid body. This is the recommended stable model and
still gives weight transfer because tire and spring forces are applied to the
chassis at the contact points. A later slice may promote wheels to full rigid
bodies using the same hinge/slider constraints.

---

## 7. Vehicle template JSON

Templates live in `config/vehicles/*.json` and are hot reloadable.

```json
{
  "id": "car_sedan",
  "source": "template",
  "massKg": 1400,
  "density": 120,
  "centerOfMass": [0.0, 0.0, -0.10],
  "length": 5.0,
  "width": 3.0,
  "height": 1.0,
  "drag": 0.32,
  "downforce": 0.0,
  "interactionRadius": 15.0,
  "bodyTexture": "assets/textureshq/240sx.png",
  "wheelTexture": "assets/textureshq/dark1.png",
  "parts": [
    {
      "id": "body",
      "shape": "box",
      "position": [0.0, 0.0, 0.0],
      "size": [5.0, 3.0, 1.0],
      "rotationEuler": [0.0, 0.0, 0.0],
      "material": "car_body",
      "visualOnly": false
    },
    {
      "id": "nose_slope",
      "shape": "slope",
      "position": [2.2, 0.0, 0.35],
      "size": [0.8, 3.0, 0.4],
      "rotationEuler": [0.0, 0.0, 0.0],
      "material": "car_body",
      "visualOnly": false
    }
  ],
  "attachments": [
    {
      "id": "seat_driver",
      "position": [-0.38, 0.25, 0.10],
      "rotationEuler": [0.0, 0.0, 0.0],
      "kind": "seat"
    }
  ],
  "wheels": [
    {
      "hub": [1.5, 1.2, -0.3],
      "radius": 0.3,
      "width": 0.25,
      "suspensionAxis": [0, 0, -1],
      "spinAxis": [0, 1, 0],
      "steerAxis": [0, 0, 1],
      "restLength": 0.2,
      "travel": 0.15,
      "springRate": 180000,
      "damperRate": 18000,
      "antiRoll": 40000,
      "unsprungMass": 38,
      "steer": true,
      "drive": false,
      "brake": true
    }
  ],
  "tire": {
    "mu": 1.35,
    "loadSensitivity": 0.00018,
    "nominalLoad": 24500,
    "peakSlipRatio": 0.12,
    "peakSlipAngle": 0.12,
    "frictionCircle": true,
    "lowSpeed": 0.6
  },
  "engine": {
    "inertia": 0.22,
    "idleRpm": 850,
    "redlineRpm": 6800,
    "frictionTorque": 18,
    "torqueCurve": [[800, 120], [2500, 240], [4500, 300], [6500, 260]],
    "turbo": { "maxBoost": 0.9, "spoolRate": 2.4, "lag": 0.18 }
  },
  "gearbox": {
    "ratios": [-3.4, 3.6, 2.1, 1.45, 1.05, 0.82, 0.68],
    "finalDrive": 3.7,
    "shiftTime": 0.18,
    "clutchCapacity": 420
  },
  "differential": {
    "type": "lsd",
    "frontRearSplit": 0.0
  },
  "steering": {
    "maxAngleDeg": 32,
    "speedFalloff": 0.55,
    "returnRate": 3.5
  },
  "brakes": {
    "maxTorque": 5200,
    "handbrakeTorque": 3000
  },
  "seats": [
    { "name": "driver", "anchor": [-0.38, 0.25, 0.10], "pose": "sit" },
    { "name": "passenger", "anchor": [0.38, 0.25, 0.10], "pose": "sit" }
  ]
}
```

Validation clamps non-finite or negative values, requires at least two wheels for
a driveable template, requires at least one chassis part when `parts` is present,
rejects unknown primitive shapes, and retains the last valid values on reload
failure. The legacy `length`/`width`/`height` box remains the fallback when a
template has no `parts` array.

### 7.1 Primitive parts

The builder and the map editor share one primitive model:

| Shape | Authoring fields | Collision | Rendering |
|---|---|---|---|
| `box` | position, size, rotation | box triangles | textured box |
| `slope` | position, size, rotation, slope axis | wedge triangles | textured wedge |
| `sphere` | position, radius | tessellated sphere triangles | smooth sphere |
| `cylinder` | position, radius, height, axis | tessellated cylinder triangles | smooth cylinder |

Each part has a stable id and an optional `visualOnly` flag. Parts contribute
mass and center of mass unless `visualOnly` is true. Body-part rendering may
merge coplanar surfaces, but collision always uses the generated triangles.

### 7.2 Attachments and seats

Attachments are named local frames. The first kinds are `seat`, `wheel`, and
`equipment`. A seat may reference an attachment or keep its own anchor for
legacy templates. Attachments use the shared constraint layer for future
detachable or articulated parts; the first builder only stores them.

Wheels remain analytic suspension units. The authoring surface must show each
wheel hub, radius, width, spin axis, steer axis, and suspension axis so a creator
can see and edit them without turning them into body parts.

---

## 8. Suspension and weight transfer

Each unit computes, per fixed tick:

1. suspension length from the chassis pose and wheel contact sweep,
2. spring force from compression and the configured rate,
3. damper force from compression velocity,
4. bump-stop force outside travel,
5. anti-roll force across an axle,
6. unsprung wheel mass and spin state.

Forces are applied to the chassis at the wheel contact/hub point with their
lever arm, so pitch, roll, squat, and dive emerge from the rigid-body response.
Do not script weight-transfer animations.

Acceptance:

- braking produces nose dive,
- acceleration produces squat,
- cornering produces body roll,
- a heavier template accelerates and stops more slowly under the same engine,
- a parked car settles and sleeps without jitter,
- no bounce or snag when crossing triangle seams.

---

## 9. Tires and grip

Tire force uses:

- normal load from suspension,
- longitudinal slip ratio,
- lateral slip angle,
- load-sensitive friction coefficient,
- a combined friction circle,
- a low-speed regularization band to avoid jitter at rest.

The result is a force at the contact patch plus a reaction torque on the wheel
spin. Grip is a JSON value: lower grip for drift, higher grip for race tires.
Emergency braking and clutch kicks must lose traction because the tire model
saturates, not because a special mode is enabled.

---

## 10. Drivetrain

Sim-lite, at the fixed tick:

- engine RPM and torque curve,
- engine inertia and friction,
- turbo boost with spool/lag,
- clutch engagement and capacity,
- PRNDL-style selector: `R`, `N`, `1`-`6`,
- automatic and manual modes,
- final drive and per-gear ratios,
- open/locked/limited-slip differential,
- engine torque reaction applied to the chassis.

A truck and a car differ only by template values. A heavier chassis accelerates
more slowly because the same force acts on more mass; a stronger engine changes
the force, not the rules.

---

## 11. Controls (authoritative)

`F` is the generic interaction key. A valid seat target wins over reviving.

### 11.1 On foot

| Input | Behavior |
|---|---|
| `F` aiming at a seat | Enter that seat |
| `F` not aiming at a seat | Existing revive behavior |

### 11.2 Seated driver

| Input | Behavior |
|---|---|
| `V` | Toggle engine on/off |
| `B` | Toggle `AUTOMATIC` / `MANUAL` |
| `W` | Throttle |
| `S` | Brake (automatic: brake, then reverse) |
| `A` / `D` | Steer left/right; self-centers and reduces max angle with speed |
| `Left Shift` | Clutch (manual) |
| `X` | Shift up (manual, clutch held) |
| `Z` | Shift down (manual, clutch held) |
| `Space` | Emergency brake |
| `G` | Ragdoll; releases the seat lock |
| `F` | Exit the car |
| Mouse | Aim/fire through the physical aim body |

Driving suppresses `jump`, `dash`, `down_dash`, `freeze`, `crouch`, and melee.
Weapon aim, fire, and reload stay available.

### 11.3 Seated passenger

No locomotion and no driving input. Look, weapon aim/fire, `G` to ragdoll
(unseats), and `F` to leave.

#### Mode-selected control profiles

Vehicle physics is shared, while the active gamemode selects a named control
profile through its mode data. The profile maps device inputs to generic
vehicle actions such as throttle, brake, steer, lean, jump, and trick. It does
not select a separate physics implementation.

The standard car profile starts in manual transmission, matching the desired
simulation-style default. Automatic transmission remains an explicit option.
Modes may select an arcade profile for a vehicle without changing its physical
template or adding mode-ID branches to the vehicle system.

The first trick-vehicle profile is shared by skateboard and BMX: W accelerates,
S brakes and then reverses, A/D steer and lean, and holding Space while
grounded charges a crouch/jump that releases into a higher jump. Airborne A/D
controls rotation about world Z (up). Board and bike templates tune balance,
traction, gravity, and jump response independently. Trick scoring and any
temporary Need for Madness boost belong to mode rules, not the shared vehicle
physics owner.

### 11.4 Modes

- **Automatic:** `W` forward, `S` brake then reverse, automatic gear selection.
- **Manual:** `Left Shift` clutch, `X`/`Z` shift only while the clutch is held,
  selector `R, N, 1, 2, 3, 4, 5, 6`. `S` is brake only.
- Holding clutch and throttle (`W`) at high RPM is a clutch kick. Traction loss
  must come from the clutch/tire physics, not a scripted mode.
- Slamming the emergency brake locks the rear wheels and must produce slip.

---

## 12. Seats, ragdoll, and weapons

Seat entry sets the actor's origin to `seat position + riderOffset` and applies
the seat pose while seated and not ragdolled. The locomotion kernel is bypassed.
The first playable acceptance gate is the driver seat. Occupancy is modeled
generically so a passenger seat can be added without a second seat owner.

`riderOffset` is a local-space seat field in `config/vehicles/<template>.json`.
The same anchor (anchor + riderOffset) is used by the client origin, the server
`ServerPlayer.pos`, the camera, and the exit position. The actor's collision
root stays at that origin so the seated actor keeps its own world collisions,
but local-player network reconciliation, pose containment, and actor-pair
position writes are skipped while seated or they re-insert the on-foot capsule
offset and produce a repeating seat snap.

The sitting pose reuses the authored `crouch`/`crouchIn` clip and holds it while
seated. If no crouch clip exists, the existing deterministic pose fallback is
used. Leaving the seat clears the held crouch through the normal animation
state input.

Weapons are held and fired by the seated actor through the existing weapon
system. The weapon muzzle, aim body, projectile/hitscan trace, world collision,
damage, and feedback paths remain the normal ones; a seated actor must not gain
a car-center firing origin. Vehicle-mounted weapons are deferred.

When the occupant ragdolls:

- the seat lock releases,
- driving input stops,
- the ragdoll simulates and collides/bounces inside the vehicle,
- the occupant may still aim/fire if the physical aim body is active,
- `F` exits.

The seated pose uses the existing animation/pose layer. If a `sit` clip is
missing, fall back to a deterministic seated pose overlay. Leaving-car animation
is a TODO; the first slice may use the existing exit/idle transition.

---

## 13. HUD

While seated, the HUD shows the vehicle state:

- engine `ON` / `OFF`,
- mode `AUTO` / `MANUAL`,
- gear `R`, `N`, `1`-`6`,
- engine RPM,
- speed,
- clutch, brake, and handbrake indicators.

An interaction prompt (`F` + seat name) appears when aiming at an enterable seat.
All drawings reuse the existing HUD text/render path; no new UI framework.

---

## 14. Networking

- The server owns vehicle instances, the chassis `PhysicalEntity`, seat
  occupancy, constraints, and drivetrain state.
- The server runs the shared fixed tick for vehicles. The client does not run an
  authoritative vehicle solver.
- Spawn/despawn and seat enter/exit are reliable.
- The client sends vehicle actions through the shared input/action surface; the
  server validates range, seat availability, lifecycle state, and occupancy.
- Chassis motion uses the existing physical-entity state replication. Vehicle
  state (template identity, occupants/seats, engine, gear, RPM, boost, speed,
  wheel spin/steer/compression) uses a compact unreliable packet at the
  established vehicle cadence.
- Clients interpolate the chassis and visual wheel state; local input may be
  predicted for responsiveness, but the server state is the correction target.
- While seated, the server owns the actor origin and bypasses normal movement
  validation/correction for that actor.
- Remote occupants appear seated through the replicated seat/occupant state and
  authoritative actor transform.
- A client must reject a vehicle spawn whose template is missing and log the
  mismatch; it must not invent a fallback vehicle state.

This section strengthens the existing physical-entity replication owner. It
does not create a second vehicle networking system.

### 14.1 Implemented online slice (2026-11-04)

The first online slice uses these compact packet families:

- `VehicleInputPacket` (client to server, unreliable): sequence, vehicle id,
  throttle, brake, steer, clutch, handbrake, and one-shot engine/mode/shift
  flags.
- `VehicleSeatRequestPacket` (client to server): request serial, vehicle id,
  seat, and enter/exit action. The client retries until a reliable answer.
- `VehicleSpawnEventPacket` and `VehicleDespawnEventPacket` (reliable):
  template identity, vehicle id, chassis network id, initial transform, and
  occupants.
- `VehicleSeatEventPacket` (reliable): vehicle id, actor id, seat, and
  enter/exit/reject. Rejection clears the client's pending request.
- `VehicleStatePacket` (unreliable): engine/gear/RPM/axes plus wheel spin,
  steer, and compression.

The dedicated server owns an authored map vehicle list. A stable hash of that
list decides when to spawn or replace server vehicles, so unrelated map config
revisions do not respawn every car. The server advances only
`PhysicalEntity::serverSimulated` bodies through the shared fixed tick.
Reliable spawn/despawn/seat events are queued per player; the existing
physical-entity replication carries chassis transforms.

A client mirror is marked `serverMirror`: it never applies suspension, tire, or
drivetrain forces. Local driver input is sent to the server and reflected back
through the state packet. A seated actor's server origin is the seat, and
normal locomotion is skipped while seated.

---

## 15. Map editor authoring

The map editor and the vehicle builder share the same transform, selection,
snapping, undo, and save tools. They own different data:

- the **map editor** places a vehicle instance in the map and spawns it,
- the **vehicle builder** edits a reusable template in
  `config/vehicles/<id>.json`.

First map-editor slice:

- add a `vehicle` placement that references a template,
- select/move it with the existing translate tools,
- spawn the selected placement into the running world,
- persist it in the map's `vehicles` array,
- show the template's wheels in the editor so hub, radius, and width are
  inspectable,
- edit wheel hub/radius/width through the editor command surface.

Full visual assembly (spawn parts, parent, rotate, live physics preview) is the
next slice on the same data model. Arbitrary vertex/edge/face editing is not in
scope.

Constraints are stored in a `constraints` array. Vehicles are stored in a
`vehicles` array. Runtime-only actors remain outside the editor, as specified in
`entity-editor.md`.

---

## 16. Destruction hooks (later slice)

- Per-part health and detachable wheel/axle flags.
- Detaching a wheel spawns a wheel `PhysicalEntity` with inherited velocity.
- Chassis deformation uses the existing CSG `ImpactSystem`.
- The current gap where `health`/`fractureThreshold` do not gate breaking must be
  fixed in the destruction owner, not in vehicle code.
- Repair/reattach is out of scope until requested.

---

## 17. Console commands

| Command | Purpose |
|---|---|
| `vehicle_list` | List spawned vehicles and state |
| `vehicle_spawn <template> [x y z yaw]` | Spawn a template at the camera or given pose |
| `vehicle_despawn [id]` | Remove a vehicle |
| `vehicle_enter [seat]` | Enter the nearest/aimed seat |
| `vehicle_exit` | Leave the current seat |
| `vehicle_reload` | Hot-reload templates and reapply |
| `vehicle_tune <key> <value>` | Live per-instance tuning |
| `vehicle_debug [suspension|tires|forces|off]` | Debug overlay |
| `vehicle_reset [id]` | Reset pose and motion |
| `vehicle_teleport <x y z> [yaw]` | Move the occupied vehicle |
| `mapeditor add vehicle [template] [id]` | Add an authored vehicle placement |
| `mapeditor spawn_vehicle [id]` | Spawn the selected authored vehicle |
| `mapeditor vehicle_wheel <index> hub <x> <y> <z>` | Edit the selected template's wheel hub |
| `mapeditor vehicle_wheel <index> radius <m>` | Edit the selected template's wheel radius |
| `mapeditor vehicle_wheel <index> width <m>` | Edit the selected template's wheel width |
| `constraint_create <type> <a> <b> ...` | Create a constraint |
| `constraint_list` | List constraints |
| `constraint_delete <id>` | Remove a constraint |
| `constraint_demo [hinge|spring]` | Spawn the two-block demo |

Commands call the same shared actions as the input and GUI paths. They must not
mutate server-owned state directly from a client.

---

## 18. Ownership and phases

| Path | Responsibility |
|---|---|
| `src/physics/collision-shapes.*` | analytic cylinder contact |
| `src/physics/constraints/constraint.h` | constraint record |
| `src/physics/constraints/constraint-system.*` | create/remove/solve, fixed tick |
| `src/vehicles/vehicle-types.h` | template and runtime state structs |
| `src/vehicles/vehicle-config.*` | JSON registry and hot reload |
| `src/vehicles/vehicle-system.*` | instances, spawn/despawn, tick |
| `src/vehicles/vehicle-suspension.cpp` | spring/damper/anti-roll |
| `src/vehicles/vehicle-tire.cpp` | slip and friction |
| `src/vehicles/vehicle-drivetrain.cpp` | engine/turbo/gears |
| `src/vehicles/vehicle-input.cpp` | input contract to shared actions |
| `src/vehicles/vehicle-seat.cpp` | occupancy, pose, enter/exit |
| `src/vehicles/vehicle-replication.*` | network state |
| `src/vehicles/vehicle-hud.cpp` | vehicle HUD and prompt |
| `src/vehicles/vehicle-commands.cpp` | console surface |
| `src/vehicles/vehicle-authoring.*` | map editor placement |

Implementation order (updated 2026-11-03):

1. Spec, shared primitive/template contract, and map-authored vehicle placement.
2. Local car vertical slice: spawn, F enter, drive, exit, wheels visible.
3. Server authority, occupancy, input, weapon use from the seat, and replication.
4. Primitive builder, reusable templates, and visual body authoring.
5. NPC driver profiles through the shared vehicle input surface.
6. Vehicle arena mode: ball, goals, scoring, and exact reference tuning.

Each phase reports build evidence, runtime `events.jsonl` evidence, and human
acceptance separately. Build success never counts as gameplay acceptance.

---

## 19. Original vision (preserved)

The original notes that motivated this work:

- MiMITA needs vehicles because drivable vehicles make a game better.
- Vehicles must be general so every mode can use them.
- Air vehicles: jets and helicopters.
- Skateboards, BMX bikes, and scooters with tricks.
- A payload map with a drivable payload; possibly a caravan mode.
- Drivable vehicles and fully simulated wheels/suspension were previously listed
  as out of scope for Payload v1 and remain future work.

See also `docs/specs/vehicles/vehicles-skateboard.md` for skateboard and BMX
controls, tuning, and trick-vehicle acceptance.

---

## 20. Open TODOs

These are noted, not edited here:

- `docs/specs/vehicles/vehicles.md` was previously a 30-line wishlist (this
  document replaces it).
- `docs/specs/vehicles/vehicles-skateboard.md` is still a 14-line wishlist.
- `docs/specs/moving-physical-objects/moving-physical-objects.md:1`, `:18`,
  `:31` ask for a grab tool, an explanation of moving objects, and whether the
  map editor is needed.
- `docs/specs/destructible-world/destructible-world.md:1` asks for a vision
  expansion.
- `docs/architecture/code-ownership/code-ownership.md:3`, `:259` ask for
  file-placement and function-generalization guidance.

---

## 21. General interaction prompt (2026-10-27)

Interaction is a general world-anchored capability, not a car-only path.

- Every interactable registers one target with a stable id, owner key, label,
  prompt origin, interaction origin, radius, priority, enabled flag, and action.
- The prompt origin comes from the object itself (for a vehicle, the chassis
  origin plus a readable roof offset). It is not a hardcoded screen position.
- The interaction origin is range-checked around the player. Every radius is
  clamped to at least **1 m**; a car template defaults to **15 m**.
- The prompt is a camera-facing screen-space billboard projected from the
  object origin. It is deliberately large: key cap, dark background, bright
  border, and a minimum on-screen font scale.
- Text is general: `Press F to <label>`. Vehicle labels are
  `enter driver seat`, `enter passenger seat`, and `exit vehicle`.
- A valid target consumes F and wins over revive. If no target is selected,
  revive keeps the old behavior.
- While seated the occupant's target is sticky so the exit prompt cannot
  flicker as the car moves.
- Prompt shown/hidden is logged on target change, not per frame.

### 21.1 Interaction range and how to change it

The vehicle F-prompt radius is the template field
`interactionRadius` in `config/vehicles/<id>.json`. The current
`car_sedan.json` uses **15 m** (five times the old 3 m default).

```json
{
  "id": "car_sedan",
  "interactionRadius": 15.0
}
```

- `VehicleSpec::interactionRadius` defaults to `15.0f` when a template omits
  the field.
- `VehicleConfigRegistry` accepts values from `1.0` to `50.0` metres.
- The client uses the value directly for the interaction target radius.
- The authoritative server allows a seat request at
  `interactionRadius + 2 m` (network margin), so changing the template changes
  both the prompt and the server acceptance range.
- `InteractionSystem` still enforces a 1 m minimum and only draws the prompt
  within 30 m.
- Pressing F prefers the driver seat. If the driver seat is occupied by another
  actor, the interaction chooses the first free seat.

To make a car easier or harder to enter, edit that one JSON field and run
`vehicle_reload`, or restart the server/client so the template reloads.

## 22. Car dimensions and visible wheels (2026-10-27)

The current first template is a deliberately simple block:

- length **5 m** (X), width **3 m** (Y), height **1 m** (Z);
- wheel radius **0.55 m**, width 0.38 m;
- wheel hub positions `x ±1.5`, `y ±1.2`, `z -0.25`;
- seat `riderOffset` **1.11 m** upward (capsule-center origin; this is seat cushion + capsule half-height).
- body texture `assets/textureshq/240sx.png`;
- wheel texture `assets/textureshq/dark1.png`.

Wheel size is edited in `config/vehicles/car_sedan.json` under `wheels[]`:
`hub`, `radius`, `width`, `restLength`, and `travel`. `vehicle.wheel.spec` logs
the loaded geometry (and the `edit_file` path) once per spawn. The ride height
used by `vehicle_spawn` is derived from those same fields, so changing the wheel
size does not require editing the terminal command or the map spawn by hand.
The dedicated server also ray-settles authored map spawns so every wheel starts
with its resting contact patch just above the surface.

Wheels are drawn as smooth textured cylinders (32 segments by default), not
authored Blender triangles. Visual spin and steering axes are per-wheel JSON
fields (`spinAxis`, `steerAxis`) and are hot reloadable so all axes can be
tested live. Physics rolling remains correct independently of the visual axis
choice. The wheel model is chassis orientation × steer × axle alignment × spin.

Every wheel reports its world center, spin axis, steer axis, spin velocity,
steering angle, suspension length, compression, load, slip, and world velocity.

## 23. Void death and runtime lifecycle (2026-10-27)

- A vehicle below the configured void kill plane is destroyed.
- If occupied, the occupant is unseated, placed at the seat's void position, and
  killed through the normal void-death path. Respawn then proceeds normally.
- If empty, the vehicle is simply destroyed.
- The seat origin must not be re-asserted after death, void death, or vehicle
  removal. This is the fix for the stuck-at-ground/respawn loop.
- `vehicle.void` is logged with vehicle id, entity id, z, killZ, and driver flag.

Runtime-only objects are process state, not map data:

- `clearRuntimeObjects(reason)` clears vehicles, constraints, persistent
  physics, corpses, and physical entities in dependency-safe order.
- It is called on listen-server start/stop, map change, return to menu, network
  shutdown, and client map switch.
- Generated entity meshes are released during the physical-entity clear.
- Vehicles and constraints are never serialized into `config/maps/*.json`.
- `runtime.objects.cleared` records counts and reason.

## 24. Vehicle telemetry and logging (2026-10-27)

Each fixed tick records:

- chassis origin, spawn origin, distance from spawn, linear/angular velocity,
  and orientation;
- mass, weight, density, and gravity scale;
- engine, mode, PRNDL gear, RPM, torque, boost, throttle/brake/steer/clutch/
  handbrake;
- per-wheel world center, spin/steer axes, spin velocity, steering angle,
  suspension length, compression, load, slip ratio/angle, grounded flag, and
  world velocity.

`vehicle.tick.summary` is emitted at most once per second per vehicle.
`vehicle.bounce` samples chassis origin/velocity/orientation, grounded count,
and per-wheel hub, contact point/normal, contact triangle id, suspension,
compression, load, and slip at 10 Hz so bounce is numerically provable.
`vehicle.wheel.spec` logs the loaded wheel geometry and edit file once per spawn.
`vehicle.seat.anchor` logs the seated origin, previous position, snap distance,
and crouch flag. `vehicle.input.local` and `vehicle.input.server` log the input
axes/edges at the client and after server application.

`vehicle.performance` records vehicle count, awake count, update milliseconds,
and physical-entity count. Spawn, enter, exit, tune, void, and audio transitions
are Important events. JSON construction is gated so a high-frequency summary
cannot allocate unbounded work.

## 25. RPM engine sound (2026-10-27)

- The engine sound is scheduled in the fixed 60 Hz vehicle tick, never the render
  frame.
- The period lerps from **0.8 s** at 800 RPM to **0.02 s** at 8900 RPM, clamped
  to at least one fixed tick.
- The sample plays from `startSeconds=0` to `endSeconds=period`, then retriggers
  with an owner key per vehicle.
- Stopping the engine, despawning, void death, and runtime cleanup stop the
  owner.
- The sound path is template data; the first slice uses a placeholder existing
  WAV until a dedicated engine asset exists.

## 26. Vehicle performance rules (2026-10-27)

- One broadphase gather per vehicle per fixed tick covers all wheel rays.
- Entity lookup is an id→index map, not a linear scan.
- The seated player skips actor-vs-occupied-vehicle contacts and player pushes.
- Wheel/force work is skipped immediately for a doomed or removed vehicle.
- Vehicles sleep/park when there is no engine or motion input.
- `Vehicle::Update`, `Vehicle::Wheel`, `Vehicle::Render`, and `Constraint::Solve`
  have profiler scopes; `vehicle.performance` makes the cost observable.
- Do not trade correctness for a frame-rate-dependent system.

## 27. Scenario and tuning (2026-10-27)

- `devscripts/scenarios/vehicle_drive_v1.json` proves spawn, enter, engine
  toggle, forward/turn, gear change, reverse, engine off, exit, unoccupied void
  destruction, occupied void death, and origin movement from A to B.
- The scenario runner supports numeric `event_field_at_least` /
  `event_field_at_most` assertions in addition to exact-field and count checks.
- `vehicle_input` sets a scripted override so a seated local driver can be
  driven by a scenario while still exercising the seat path.
- `vehicle_tune <id> <mass|spring|damper|grip|torque|drag> <value>` changes live
  per-instance behavior. Template JSON remains the persistent source.
- `vehicle_teleport` and `runtime_clear` exist for scenarios and diagnostics.

## 28. Constraint/attachment debug visibility (2026-10-27)

- Constraint and attachment anchors, hinge axes, slider travel/limits, spring
  rest marks, and labels are drawn only when `entity_visibility` is on.
- There is no separate constraint visibility toggle; the existing authoring gate
  is the single owner.
- The debug renderer is read-only and does not change solver behavior.

