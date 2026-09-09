9 7 2026  1704 jorj todo - explain the destructible world vision i have, how it relates to collisons, how to  balance constraints such as

1. shoot world = within like 100ms, instant client rpedicted hole (or just  that is the hole for real?) same with explosions, like u can destroy a big circle in the world/surface
2. destruct parts of the world underneath other parts = gravity, which means other objects ontop of that object  

this should relate to the  spec for phsical objects here C:\mimita-priv-v8\docs\specs\moving-physical-objects\moving-physical-objects.md

# Destructible World

**Created:** 2026-09-09  
**Status:** Specification / architecture  
**Primary config:** `C:\mimita-priv-v8\config\destructible-world.json`

**Related specifications:**

- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/procedural-infinite-world/procedural-infinite-world.md`
- networking specification
- effects specification
- player/body physics and future biological destruction specifications

---

# 1. End goal

The world must behave according to a small number of general physical rules instead of feature-specific scripted behaviors.

A bullet, rocket, falling concrete slab, player moving at extreme velocity, collapsing building, moving platform, piece of debris, or future object must interact with matter through the same fundamental systems.

There must not be special rules such as:

```text
if weapon == rocket:
    break_wall()

if object == falling_bridge:
    run_bridge_collapse_animation()

if player_speed > 1000:
    make_crater()
```

Instead:

```text
mass
velocity
material
shape
collision
impulse
energy
geometry
support
```

produce the result.

The intended long-term engine property is:

> Extreme results come from extreme inputs to ordinary rules.

A sufficiently energetic physical object can therefore destroy a structure even if no developer explicitly programmed that specific interaction.

---

# 2. Fundamental principles

## 2.1 General systems over special cases

All physical objects use the same underlying systems wherever technically possible.

Examples:

- bullets
- rockets
- players
- ragdolls
- crates
- platforms
- walls
- building fragments
- glass
- concrete
- future vehicles
- procedural structures

may differ in configuration and material properties, but should not require fundamentally separate physics implementations.

If normal gameplay produces an undesirable result, prefer changing physical/material/configuration values rather than adding a special-case code path.

Example:

Bad:

```text
floor breaks too easily
→ hardcode bulletsCannotBreakFloor=true
```

Preferred:

```text
floor breaks too easily
→ increase material thickness / toughness / fracture resistance
```

The floor must still be breakable if sufficient energy is applied.

---

# 3. Units

MiMITA physical simulation uses SI / metric units.

Canonical units:

```text
distance             meter (m)
time                 second (s)
mass                 kilogram (kg)
velocity             meters/second (m/s)
acceleration         meters/second² (m/s²)
force                newton (N)
impulse              newton-second (N·s)
energy               joule (J)
density              kilograms/meter³ (kg/m³)
pressure             pascal (Pa)
torque               newton-meter (N·m)
angular velocity     radians/second
temperature          kelvin where applicable later
```

Any exceptions must be explicitly documented.

Game-facing configuration may expose convenient alternate units, but runtime physics must normalize them into the canonical system.

---

# 4. Geometry model

## 4.1 MiMITA remains fundamentally mesh compatible

Imported geometry may continue to consist of:

```text
point / vertex
edge
face
triangle
```

Triangles remain the smallest conventional rendered polygon primitive.

Blender therefore remains fully compatible with the system.

However:

> Destruction must NOT operate by deleting entire source triangles.

A large Blender wall may contain only two triangles on one face.

A bullet striking the wall must not destroy one of those entire triangles.

Triangles describe a surface approximation.

They are not the unit of destruction.

---

# 5. Continuous mathematical geometry + triangle rendering

The destruction system must distinguish between:

1. **mathematical geometry**
2. **render geometry**

For example:

```text
SubtractSphere(
    center = [12.0, 4.0, -8.0],
    radius = 0.08
)
```

describes a mathematically perfect sphere.

The sphere itself does not contain triangles.

It has effectively continuous curvature.

When the engine must display the resulting surface, it generates enough triangles to approximate that mathematical surface to the required accuracy.

Therefore:

```text
EXACT REPRESENTATION

sphere:
    center
    radius
```

can produce:

```text
LOW DETAIL:
   24 triangles

MEDIUM DETAIL:
   300 triangles

HIGH DETAIL:
   20,000 triangles
```

without changing the actual destruction event.

The mathematical world state remains the same.

Only its current approximation changes.

This is essential for:

- LOD
- low-power devices
- enormous worlds
- persistent worlds
- deterministic networking
- adaptive collision resolution

---

# 6. Geometry layers

A destructible object conceptually contains several related representations.

## 6.1 Source geometry

Original authored geometry imported from Blender or produced procedurally.

Example:

```text
building_018.wall_04
```

This contains:

- vertices
- edges
- faces
- triangles
- material assignments
- object transform
- authoring metadata

---

## 6.2 Destruction state

Instead of permanently rewriting the source asset for every hit, destruction can initially be represented as mathematical operations.

Example:

```text
BaseGeometry
-
Sphere A
-
Capsule B
-
Sphere C
```

Conceptually:

```text
CurrentGeometry =
    BaseGeometry
    + AddOperations
    - SubtractOperations
```

This is similar in spirit to constructive solid geometry.

Supported primitive operations should begin with:

```text
subtractSphere
subtractCapsule
subtractBox
subtractCylinder

addSphere
addCapsule
addBox
```

Future operations may include:

```text
convex primitive
plane cut
sweep
custom implicit field
procedural function
```

---

# 7. Local remeshing

When destruction affects part of an object, only the affected region should require geometry reconstruction.

Example:

```text
100 meter wall
        ↓
bullet makes 1 cm hole
```

The engine must NOT rebuild the entire wall if avoidable.

Instead:

```text
find affected spatial region
↓
update local destruction representation
↓
regenerate only nearby surface geometry
↓
update only nearby collision representation
```

This is referred to as **local remeshing**.

The exact algorithm may change over time.

The specification defines the behavior, not one mandatory implementation.

Possible implementations include:

- CSG clipping
- sparse volumetric representation
- signed-distance fields
- adaptive voxel structures
- octrees
- dual contouring
- marching cubes
- locally generated triangle meshes
- hybrid approaches

The architecture must allow replacement of the internal algorithm without changing gameplay behavior.

---

# 8. Sparse representation

Empty space and completely unchanged space should cost as close to zero memory/CPU as practical.

A massive object must not automatically require tiny destruction cells everywhere.

Instead, detailed representation should primarily exist where necessary:

```text
unchanged wall:
    original representation

bullet-hole area:
    detailed destruction representation

100 km away:
    coarse / unloaded representation
```

Spatial resolution may increase locally around:

- recent destruction
- nearby players
- collision-critical surfaces
- active moving objects

and decrease elsewhere.

---

# 9. Deterministic irregular destruction

A revolver hole should not necessarily resemble a perfectly machine-cut circle.

The underlying destructive primitive may be modified by deterministic seeded variation.

Example:

```text
DestructionOperation {
    shape: sphere
    center: [...]
    radius: 0.023
    irregularity: 0.14
    seed: 918273
}
```

The seed must cause all deterministic clients/server instances to derive the same logical shape.

This permits irregular:

- chips
- cracks
- rough crater boundaries
- fragmentation

without networking every individual vertex.

A configuration value may disable irregularity and produce mathematically perfect geometry.

Large attacks may deliberately use perfect forms.

Example:

> A city-scale attack may produce a gigantic perfect spherical subtraction through buildings and terrain.

That is valid and desirable.

---

# 10. Blender authoring

For v1, Blender is the primary map authoring tool.

Object names should NOT be the primary metadata system.

Preferred approach:

**Blender Custom Properties.**

Example properties:

```text
mimita.destructible = true
mimita.material = "concrete.standard"
mimita.structural = true
mimita.indestructible = false
mimita.persistent = true
```

Additional properties may include:

```text
density
thickness override
structural role
spawn behavior
animation/controller ID
object ID
procedural ID
LOD policy
persistence policy
```

Export/import tooling converts Blender metadata into MiMITA runtime map data.

---

# 11. Future map editor

The runtime representation must not depend fundamentally on Blender.

Blender is only one frontend for producing the data.

Eventually:

```text
Blender
     \
      → MiMITA map representation
     /
MiMITA editor
```

The in-engine editor should eventually manipulate the same underlying structures directly.

There must not be a future requirement to redesign every map format merely to stop using Blender.

---

# 12. Materials

Materials are universal engine resources.

Examples:

```text
concrete
steel
glass
wood
flesh
bone
rubber
soil
stone
water
```

Objects reference materials instead of duplicating material behavior.

Example:

```json
{
  "material": "concrete.standard"
}
```

Material definitions may eventually contain:

```text
density
elastic modulus
yield strength
compressive strength
tensile strength
shear strength
fracture toughness
hardness
ductility
friction
restitution
thermal properties
penetration resistance
fracture behavior
sound behavior
debris behavior
visual response
```

Not all properties need to exist in v1.

The representation must allow them to be added.

---

# 13. Material inheritance

When an object separates into multiple pieces, resulting pieces inherit relevant physical properties from their source.

Example:

```text
solid steel wall
↓
cut out steel section
↓
detached steel section
```

The detached section retains:

```text
steel material
steel density
appropriate calculated mass
surface properties
collision behavior
existing damage
existing decals/effects where applicable
```

Therefore:

- steel fragments are heavy
- glass fragments are relatively light
- concrete fragments have concrete density
- wood fragments behave like wood

No arbitrary generic "debris material" replaces gameplay-relevant physical matter.

Cosmetic debris may remain separate.

---

# 14. Mass

Where possible:

\[
mass = density \times volume
\]

If a destructible object loses volume, its mass should update accordingly.

If a section becomes detached, its mass is derived from:

```text
remaining volume × material density
```

Multi-material objects may sum mass across material regions.

---

# 15. Physical impact pipeline

All sufficiently relevant collisions should feed the same generalized physical interaction system.

Inputs include:

```text
object A mass
object B mass
relative velocity
contact point
contact normal
contact area / shape
materials
angular velocity
geometry
```

Useful quantities include:

\[
p=mv
\]

momentum,

and:

\[
E_k=\frac12mv^2
\]

kinetic energy.

Neither quantity alone completely determines real-world fracture behavior, so the long-term system may additionally account for:

```text
impulse
pressure
contact area
stress
strain
material failure
duration of contact
geometry
```

v1 may use approximations.

---

# 16. No weapon-specific destruction

Weapons must eventually support physical projectile behavior independently from hitscan convenience.

`weapons.md` should define that a weapon may have modes such as:

```text
hitscan
physical projectile
```

or potentially both for testing/configuration.

A projectile defines physical properties rather than "wall damage."

Example:

```json
{
  "massKg": 0.008,
  "velocityMps": 350,
  "diameterMeters": 0.009,
  "material": "lead"
}
```

The environment response then emerges from collision/material rules.

The destruction system should not contain rules such as:

```text
revolverHoleRadius = X
rocketHoleRadius = Y
```

except where explicit arcade overrides are intentionally configured.

The preferred model is:

```text
physical input
+
material properties
=
physical response
```

---

# 17. Penetration

Penetration is continuous rather than binary weapon-specific logic.

Projectile traversal should reduce available energy/momentum according to interaction with material.

Conceptually:

```text
enter material
↓
material resists projectile
↓
projectile loses kinetic energy
↓
material may fracture/displace
↓
if projectile retains sufficient motion:
    continue through
else:
    stop / embed / deflect
```

Thus:

```text
very low-energy projectile + steel
→ effectively nothing

high-energy projectile + steel
→ deformation / penetration

extremely energetic object + steel
→ severe destruction
```

No fundamental special case is required.

---

# 18. Physical objects can cause destruction

Weapon projectiles are not special.

A falling crate, collapsing skyscraper section, player, vehicle, rock, or any future object can produce destruction through the same collision pipeline.

Example:

```text
2,000 kg slab
falls
accelerates
hits wooden floor
↓
collision physics
↓
material stress exceeds resistance
↓
floor fractures
```

No `if fallingConcreteSlab` path exists.

---

# 19. Destruction operations

After determining material response, the simulation may produce one or more geometry operations.

Examples:

```text
subtractSphere
subtractCapsule
fractureRegion
separateConnectedComponent
deformRegion
```

v1 should focus primarily on subtraction.

Example:

```text
ImpactResponse:
    subtractCapsule(
        start = entryPoint,
        end = penetrationEnd,
        radius = calculatedRadius
    )
```

A bullet can therefore produce a narrow channel through matter rather than simply deleting a surface polygon.

---

# 20. Detached connected regions

After geometry changes, the engine must identify newly disconnected pieces where relevant.

Example:

```text
wall
↓
player cuts around circular section
↓
center section no longer connected to remaining wall
```

The disconnected section becomes an independent physical object.

Its geometry, material, mass, transform, velocity, and relevant state are derived from the parent object.

This creates the transition:

```text
STATIC STRUCTURAL GEOMETRY
        ↓ separation
MOVING PHYSICAL OBJECT
```

The moving physical objects specification then owns its dynamic simulation.

---

# 21. Structural support v1

v1 does not need complete structural engineering.

Initial rule:

> Structural geometry that no longer has a valid supporting connection may transition into dynamic physical simulation.

Objects maintain or derive connectivity relationships.

Conceptually:

```text
foundation
   |
 pillar
   |
 floor
   |
 machine
```

If the pillar is destroyed:

```text
foundation

pillar X

floor has no structural path to support
```

The floor becomes unsupported.

It wakes and falls.

---

# 22. Support graph

Structural elements may be represented as a graph.

Nodes:

```text
structural region/object
```

Edges:

```text
supporting physical connection
```

Anchor nodes represent:

```text
indestructible world
foundation
configured structural anchors
```

A structural island with no path to an anchor is unsupported.

v1 may use connectivity alone.

Future versions may model actual load limits.

---

# 23. Contact support and balance

Dynamic physical objects resting on other objects should not require structural graph membership.

Normal collision contacts provide support.

Example:

```text
floor
 ↑
table
 ↑
crate
```

Destroy floor:

```text
floor contact disappears
↓
table loses support
↓
table wakes immediately
↓
crate's supporting table begins moving
↓
crate responds through ordinary physics
```

The destruction step and physics wake-up must be coordinated so objects do not remain floating for visible frames.

---

# 24. Center of mass

Later structural/balance calculations should use center of mass.

A rigid object may remain balanced when its projected center of mass lies inside its effective support region.

Conceptually:

```text
       COM
        |
        v
 ┌─────────────┐
 │    slab     │
 └──────┬──────┘
        │
      pillar
```

may remain stable.

But:

```text
       COM
        |
        v
 ┌─────────────┐
 │    slab     │
 └─┬───────────┘
   │
pillar
```

may produce torque and fall.

v1 may postpone accurate support polygons and simply rely on rigid-body collision once a structural component becomes dynamic.

---

# 25. Structural simulation roadmap

Long-term structural simulation may include:

```text
compression
tension
shear
bending
torsion
buckling
fatigue
fracture propagation
load distribution
```

Example:

A bridge does not merely fail because a beam becomes disconnected.

Eventually:

```text
beam remains connected
↓
neighbor removed
↓
remaining beam receives greater load
↓
stress exceeds material strength
↓
beam bends/fractures
↓
load redistributes
↓
progressive collapse
```

This is a later refinement.

The architecture must not prevent it.

---

# 26. Client prediction

Local destruction caused by the local player should appear as quickly as technically possible.

Desired sequence:

```text
local input
↓
local simulation determines impact
↓
immediately predict destruction
↓
immediately update visible geometry
↓
immediately update local collision
↓
spawn local cosmetic effects
↓
send claim/request to authoritative server
```

The player should not wait for network round-trip latency to see their shot affect the world.

Target perceived response:

```text
same frame whenever possible
```

and otherwise within the earliest available frame.

---

# 27. Server authority

The server remains authoritative in the initial architecture.

Clients may predict.

Clients do not gain permanent authority to invent arbitrary world geometry.

The server validates:

```text
who caused event
when
object state
position
impact
projectile state
allowed energy/momentum
target state
```

and produces/accepts an authoritative destruction result.

---

# 28. Replicate causes, not meshes

Networking should avoid transmitting thousands of generated vertices.

Preferred authoritative event:

```json
{
  "eventId": 918273,
  "tick": 481002,
  "targetId": 882,
  "operation": "subtractSphere",
  "center": [12.4, 8.1, -3.2],
  "radius": 0.42,
  "seed": 81827
}
```

Every simulation can reconstruct the corresponding result.

Do NOT normally transmit:

```text
vertex 1 moved...
vertex 2 moved...
vertex 3 moved...
...
vertex 30,000 moved...
```

---

# 29. Reconciliation

When predicted and authoritative destruction agree:

```text
do nothing visible
```

When they differ:

```text
correct local authoritative state
```

Corrections should avoid noticeable popping where feasible.

Possible techniques include:

- replacing operation IDs
- local remesh correction
- blending visual surfaces
- prioritizing authoritative collision immediately while smoothing visuals

Gameplay correctness takes priority over hiding a disagreement.

---

# 30. Cosmetic debris

Visual debris is client-local unless explicitly promoted to gameplay physics.

When authoritative destruction occurs:

```text
logical destruction
↓
each client independently spawns cosmetic debris
```

Cosmetic debris may vary according to machine capability.

Example:

```text
high end:
80 pieces

low end:
8 pieces

extremely constrained:
0 pieces
```

The authoritative world remains identical.

Cosmetic debris must not:

- determine player collision
- determine damage
- block bullets
- alter authoritative state

unless specifically promoted to a real physical object.

---

# 31. Gameplay fragments

Disconnected real world chunks are different from visual debris.

Example:

```text
1 m² chunk of steel wall becomes disconnected
```

That is a real physical object.

It must:

- have mass
- collide
- fall
- potentially damage things
- potentially cause additional destruction
- be networked
- persist according to policy

This distinction must remain explicit.

---

# 32. Moving platforms and animation

Animation is controlled by the engine.

Blender may provide:

- animation reference
- keyframes
- paths
- transforms
- timing

but imported animation should represent **desired motion**, not unquestionable teleportation.

Example:

```text
desiredTransform(t)
```

The engine attempts to move the physical body toward this state using configured constraints/controllers.

Potential configuration:

```json
{
  "positionStrength": 50000.0,
  "positionDamping": 800.0,
  "rotationStrength": 10000.0,
  "rotationDamping": 400.0,
  "maxForceN": 1000000.0,
  "maxTorqueNm": 200000.0
}
```

These values must be hot reloadable where practical.

---

# 33. Physically driven animation targets

The animation controller can be viewed as:

```text
current physical state
↓
desired animated state
↓
calculate corrective force/torque
↓
physics simulation
```

Therefore external physics can interact with animated objects.

A moving platform may:

- carry players
- be grabbed during ragdoll mode
- receive bullet holes
- receive blood
- collide with objects
- be partially destroyed
- detach
- become ordinary physics

without requiring unique systems.

---

# 34. Infinite factory example

The architecture must support maps containing repeating/generated moving machinery.

Example:

```text
factory generator
↓
spawn platform
↓
platform follows physical animation controller
↓
player grabs platform
↓
platform receives damage
↓
platform is destroyed
↓
remaining pieces become physical objects
↓
factory later generates replacement platform
```

The destruction system does not care that it was part of an animation.

The animation system does not override destruction.

---

# 35. Persistence policy

Persistence is configurable per world/server/gamemode.

Potential policies:

```text
none
round
match
session
server-lifetime
persistent-world
permanent
```

The architecture must support extremely long-lived worlds.

Conceptual goal:

> A player may create or modify something, leave, and much later return to find that persistent state still represented.

The engine should not assume worlds periodically reset.

---

# 36. Long-duration persistence architecture

It is not practical to replay an ever-growing raw event log forever.

Therefore persistent world state must support compaction.

Conceptually:

```text
base generated state
+
recent modifications
```

Periodically:

```text
old modifications
↓
compact into regional snapshot
↓
discard redundant historical reconstruction data
```

while preserving the same current state.

Thus:

```text
year 1:
seed + 500 events

later:
seed + compacted chunk snapshot + 20 recent events
```

rather than:

```text
seed + trillions of operations replayed at startup
```

---

# 37. Event history versus current state

If complete historical replay is desired, archive history separately.

Runtime gameplay should primarily care about:

```text
current authoritative state
```

not every historical action required to derive it.

This permits extremely old worlds without linearly increasing simulation cost.

---

# 38. Procedural worlds

Procedural generation integrates directly with persistence.

Canonical concept:

\[
World = Generate(seed) + PersistentDifferences
\]

All parties sharing the same deterministic generation rules and seed can independently produce the same untouched world.

The network therefore mainly communicates:

```text
seed
generation version
authoritative modifications
```

rather than every unchanged building.

---

# 39. Spatial identity

Generated objects must have deterministic stable IDs.

Example:

```text
worldSeed
region coordinate
generator version
local object index
```

may derive:

```text
PersistentObjectID
```

A building generated again 10 years later must receive the same identity if its generating inputs have not changed.

That allows saved differences to apply correctly.

---

# 40. Generated-region queueing

World generation must not stall the main gameplay simulation.

Regions may enter states such as:

```text
requested
queued
generating
collision-ready
render-ready
fully-ready
unloaded
```

Work should be decomposed into bounded jobs.

Gameplay-critical work is prioritized.

Optional detail can arrive later.

---

# 41. High-speed traversal

The architecture must eventually support extremely rapid movement through procedural space.

Examples:

```text
500 m/s
1,000 m/s
100,000 m/s
999,999 m/s
```

Future relativistic simulation may impose additional rules approaching light speed.

World generation and networking therefore cannot assume players move slowly between adjacent fixed map cells.

Generation should anticipate motion using:

```text
position
velocity
acceleration
view direction
known trajectory
```

to prioritize likely-needed regions.

---

# 42. Large-scale destruction

Destruction cost should not necessarily scale directly with apparent visual size.

A city-sized spherical attack can be represented compactly:

```text
subtractSphere(
    center,
    radius = 5000 m
)
```

The operation itself is O(1)-sized state.

Only spatial regions that need concrete evaluation should process its intersection.

A region very far from the sphere does nothing.

A region entirely inside it may potentially become:

```text
fully removed
```

without generating a detailed cut surface.

Only boundary regions need detailed reconstruction.

This is an important optimization.

---

# 43. Spatial acceleration

All geometry/destruction queries must eventually use spatial acceleration.

Candidates include:

```text
BVH
octree
sparse grid
spatial hash
chunk hierarchy
```

The engine must not scan every world object for every impact.

A query such as:

```text
sphere centered X radius R
```

should rapidly identify only intersecting regions.

---

# 44. Simulation budgeting

A central design requirement is predictable frame cost.

The engine should use explicit work budgets.

Potential budgets:

```text
physics
destruction
remeshing
structural analysis
network processing
procedural generation
persistence
effects
```

Heavy operations should be split into incremental jobs when they do not need to finish atomically.

Example:

```text
huge explosion
↓
gameplay-critical nearby collision resolved first
↓
remaining structural work queued
↓
visual refinement occurs incrementally
↓
far regions process later
```

---

# 45. Main-thread protection

Expensive operations should move off the main simulation/render thread where safe.

Candidates:

```text
mesh generation
LOD building
persistence compression
procedural generation
spatial rebuilds
non-critical fracture analysis
```

The main simulation must consume finished immutable/validated results at safe synchronization points.

Background work must not introduce races or nondeterministic authoritative outcomes.

---

# 46. Performance target

The long-term aspiration is extremely low and highly consistent frame times.

Target direction:

```text
~1 ms/frame-class simulation/render workloads where hardware and workload permit
```

However, "≤1 ms on every frame on every device regardless of object count" cannot be a literal physical guarantee: finite hardware has finite compute and memory.

Therefore the enforceable architectural requirement is:

> Work performed per frame must remain bounded, and increasing world complexity must primarily increase deferred/background/storage work rather than unbounded foreground frame cost.

When the workload exceeds available compute, degrade:

```text
visual detail
simulation resolution
update frequency
faraway processing
cosmetic work
```

before allowing catastrophic frame-time spikes.

Authoritative gameplay correctness remains protected.

---

# 47. LOD applies to simulation too

LOD is not only graphical.

Possible levels:

### LOD 0
Nearby / gameplay critical.

```text
high-resolution geometry
frequent rigid-body updates
precise contacts
detailed destruction
```

### LOD 1

```text
simplified collision
reduced update frequency
coarser fracture representation
```

### LOD 2

```text
aggregate rigid body
very coarse collision
event-driven updates
```

### LOD 3

```text
persistent data only
not actively simulated
```

The logical result must remain recoverable when the object becomes relevant again.

---

# 48. Sleeping

Objects that do not need active simulation must sleep.

A persistent world containing ten billion objects must not execute physics on ten billion objects every frame.

Most objects should cost effectively nothing while inactive.

Wake triggers include:

```text
nearby impact
support removal
player interaction
external force
collision
destruction
script/controller event
```

---

# 49. Region/chunk streaming

The world must be spatially partitioned.

A chunk/region may contain:

```text
generated base state
persistent modifications
active objects
sleeping objects
destruction data
structural metadata
```

Only relevant regions need active runtime representations.

Unloaded regions remain serialized.

---

# 50. Conservation laws

The engine should move increasingly toward conservation-based physical behavior.

Important quantities include:

```text
mass
momentum
angular momentum
energy
charge later if applicable
```

Interactions should avoid arbitrary creation or deletion of these values except where explicitly representing:

- external forces
- engines
- explosions/energy sources
- game abstractions
- numerical correction

This provides a consistent foundation for extreme-scale interactions.

---

# 51. Extreme player impact example

Given:

```text
player mass = 70 kg
velocity = 1,000 m/s
```

kinetic energy is approximately:

\[
E_k = \frac12(70)(1000^2)
\]

\[
E_k = 35,000,000J
\]

The engine should not ask:

```text
Is this an anime attack?
```

It evaluates:

```text
collision geometry
material
velocity
mass
contact
energy transfer
```

Potential emergent result:

```text
player enters building
↓
façade material exceeds failure threshold
↓
material removed/fractured
↓
player loses momentum
↓
player continues deeper
↓
additional collisions occur
↓
structural support disappears
↓
pieces detach
↓
detached pieces become moving physical objects
↓
those objects cause further collisions
```

The exact outcome emerges from physics/configuration.

---

# 52. Player destructibility

The architecture should eventually permit player bodies to use compatible destructible geometry.

A player may contain physical/material regions such as:

```text
skin
muscle
bone
blood
organs
brain
```

However, the initial gameplay simulation may remain simplified.

Example:

```text
shot intersects head
↓
geometry/material interaction
↓
significant critical head volume destroyed
↓
gameplay health system determines lethal state
```

Future systems may model:

```text
blood volume
blood loss rate
organ functionality
limb structural integrity
bone damage
```

without requiring weapon-specific kill rules.

---

# 53. Arcade presentation versus physical foundation

The simulation can have physically motivated underlying behavior while presenting simple gameplay information.

Players do not need to see:

```text
brain tissue remaining: 71.23%
blood pressure...
```

unless desired.

The game can simply present:

```text
damage
death
limb loss
stagger
```

while the engine uses more general calculations underneath.

---

# 54. Hot reload

`config/destructible-world.json` should contain tuning parameters that can safely change during development.

Possible categories:

```json
{
  "destruction": {
    "enabled": true,
    "predictionEnabled": true,
    "irregularityEnabled": true
  },

  "remeshing": {
    "maxJobsPerFrame": 4,
    "targetErrorMeters": 0.002
  },

  "structural": {
    "enabled": true,
    "connectivityOnlyV1": true
  },

  "visualDebris": {
    "enabled": true,
    "maxPiecesPerEvent": 32
  },

  "budgets": {
    "destructionMs": 0.15,
    "structuralMs": 0.10,
    "meshCommitMs": 0.10
  }
}
```

These are examples, not final values.

Physical material data should ideally live in generalized material configuration rather than destructible-world-specific configuration.

---

# 55. Determinism

Whenever possible, authoritative mathematical operations should be deterministic from explicit inputs.

Do not depend on:

```text
wall clock time
undefined iteration order
platform-specific random()
client frame rate
```

Seed all random behavior that affects authoritative state.

Cosmetic-only variation does not need full determinism.

---

# 56. Numerical precision

Because the long-term world may span enormous distances while supporting tiny bullet holes, one coordinate representation may not provide sufficient precision everywhere.

The engine should eventually use a hierarchical coordinate system.

Example:

```text
world / astronomical coordinate
region coordinate
local high-precision coordinate
```

Physics happens primarily in local coordinate frames.

This prevents extremely large absolute coordinates from destroying small-scale floating-point precision.

---

# 57. Future relativity

General relativity is outside v1.

The architecture should avoid assuming:

```text
velocity has no meaningful upper bound
time is globally identical forever
Euclidean global coordinates are sufficient at all scales
```

Future high-speed/astronomical simulation may introduce:

```text
speed-of-light limits
relativistic momentum
relativistic energy
reference frames
time dilation
curved spacetime approximations
```

These should be layered onto the generalized physics architecture rather than implemented as weapon-specific behavior.

---

# 58. Persistence and generated destruction

For a procedural region:

```text
BaseRegion = Generate(seed, coordinates, generatorVersion)
```

Persistent state may store:

```text
destruction operations
created objects
removed objects
transformed objects
player constructions
material changes
```

When loading:

```text
generate base
↓
load compacted persistent state
↓
apply recent modifications
↓
region becomes current
```

---

# 59. Persistence compaction

If a wall receives one million bullet holes, runtime should not necessarily retain one million independent operations forever.

The system may periodically bake them into a compact representation.

Before:

```text
base wall
+ 1,000,000 subtract operations
```

After:

```text
compacted wall state
+ 12 recent operations
```

The visible/collision result remains equivalent.

Compaction must preserve persistent behavior.

---

# 60. Infinite lifetime principle

Simulation cost should depend primarily on:

```text
what is currently relevant
```

not:

```text
how old the server is
```

A world running for an extremely long time should not become progressively slower merely because more history exists.

Old state should become:

```text
compacted
spatially indexed
sleeping
serialized
```

until needed.

---

# 61. Failure/degradation hierarchy

When hardware cannot perform every desired operation immediately, degrade in approximately this order:

1. cosmetic debris count
2. particles/effects detail
3. render mesh resolution
4. distant destruction surface resolution
5. distant physics update rate
6. structural-analysis refinement
7. faraway procedural generation priority
8. faraway active simulation

Do **not** silently sacrifice:

```text
authoritative object identity
critical collision correctness
local player input
nearby gameplay state
persistence correctness
```

merely to preserve cosmetic fidelity.

---

# 62. Required v1 implementation boundary

v1 does NOT need:

- real finite-element structural analysis
- relativity
- organ simulation
- city-scale real-time fracture
- Earth-sized active simulation
- arbitrary molecular physics
- perfect fracture mechanics

v1 SHOULD establish the architecture that allows those capabilities to replace approximations later.

Minimum v1:

```text
Blender destructible metadata
universal material reference
metric units
mathematical subtraction primitives
local geometry update
local collision update
client prediction
server authority
network destruction events
cosmetic debris
connected-region separation
basic support connectivity
promotion into moving physical object
persistence-ready IDs/state
frame-budgeted processing
```

---

# 63. Core v1 behavioral examples

## Revolver into wall

```text
projectile collision
↓
calculate physical interaction
↓
small subtraction volume
↓
client predicts immediately
↓
small hole appears
↓
collision hole exists
↓
server confirms
↓
other clients reproduce event
```

---

## Minigun

Hundreds of bullets:

```text
hundreds of small local operations
```

NOT:

```text
delete giant Blender triangles
```

The wall can gradually become riddled with actual holes.

---

## Rocket

```text
high-energy event
↓
larger affected volume
↓
material removed
↓
nearby structural connections may disappear
↓
unsupported region detaches
```

---

## Falling floor

```text
support destroyed
↓
floor structural island disconnected
↓
floor becomes moving physical object
↓
table loses static support
↓
table falls
↓
crate follows through normal contacts
```

---

## Cut-out wall chunk

```text
destruction operations surround region
↓
region becomes disconnected
↓
connected-component analysis finds island
↓
island inherits material
↓
volume determines mass
↓
island becomes physical object
```

---

## Massive spherical attack

```text
subtractSphere(center, radius=5000m)
```

Spatial system determines affected chunks.

Interior chunks may become simply absent.

Boundary chunks generate detailed cut surfaces.

The network does not transmit millions of destroyed triangles.

---

# 64. Core architectural invariant

This should be treated as one of the most important rules of this specification:

> **Store and communicate the simplest underlying cause that can deterministically reproduce the required state, instead of storing or communicating every derived detail.**

Examples:

```text
sphere + radius
```

instead of 50,000 vertices.

```text
world seed
```

instead of every untouched skyscraper.

```text
material + volume
```

instead of manually configured fragment mass.

```text
mass + velocity + collision
```

instead of manually configured "impact damage."

---

# 65. Another architectural invariant

> **Triangles are representations of surfaces, not units of physical matter.**

Changing triangle density must not fundamentally change how durable an object is.

A wall imported using:

```text
2 huge triangles
```

and the same wall imported using:

```text
20,000 tiny triangles
```

should behave physically equivalently if their:

```text
shape
material
density
thickness
```

are equivalent.

This prevents asset topology from accidentally becoming gameplay physics.

---

# 66. Another architectural invariant

> **Static world and moving physical objects are states of generalized physical matter, not permanently unrelated categories.**

A static wall section may become a moving object.

A moving object may come to rest and sleep.

A generated object may become persistent.

An animated object may lose its controller and become ordinary physics.

The systems should interoperate.

---

# 67. Another architectural invariant

> **Visual complexity must be separable from simulation complexity.**

Two clients may render the same spherical crater with radically different triangle counts.

They must still agree about:

```text
where the surface is
whether a bullet passes through it
whether a player fits through it
what matter remains
```

within configured simulation tolerances.

---

# 68. Acceptance criteria for initial implementation

The initial system is considered architecturally successful when all of these are possible without weapon-specific wall code:

### A
A revolver creates a small actual hole in a destructible wall.

### B
The wall can consist of only a few source triangles without the entire triangle disappearing.

### C
The local firing client sees the predicted hole immediately.

### D
The server validates and reproduces the authoritative destruction for peers.

### E
A projectile can travel through a sufficiently deep completed hole.

### F
Repeated bullets can gradually remove more material.

### G
A completely detached wall region becomes a moving physical object.

### H
Its mass derives from its remaining volume and material density.

### I
Destroying support beneath an object causes that object to wake/fall.

### J
A cosmetic debris setting can change from zero pieces to many pieces without affecting authoritative gameplay.

### K
The same physical collision pipeline can accept a bullet or ordinary rigid object as the impact source.

### L
An untouched giant wall does not require extremely high mesh density merely to support tiny future holes.

---

# 69. Long-term success condition

The mature system should make interactions such as this possible without a bespoke script:

```text
player A hits player B
↓
B gains extreme velocity
↓
B strikes tower
↓
tower material receives physical impact
↓
penetration/fracture occurs
↓
B loses momentum while traveling through structure
↓
structural regions lose support
↓
sections collapse
↓
falling sections strike neighboring structures
↓
secondary destruction occurs
↓
nearby clients simulate gameplay-critical state immediately
↓
faraway detail resolves according to budgets
↓
world modifications persist
```

The engine should treat this as an unusual magnitude of normal physics rather than a special cinematic event.

---

# 70. Design philosophy

The ultimate objective is not to individually program every possible event.

The objective is to define a sufficiently small, sufficiently general set of physical rules that enormous numbers of different events emerge naturally from combinations of:

```text
matter
geometry
mass
energy
movement
collision
support
time
```

The system should become more accurate primarily by improving these generalized foundations rather than accumulating special cases.

**Unknown behavior should become known by refining the underlying model, not by hardcoding every observed outcome.**