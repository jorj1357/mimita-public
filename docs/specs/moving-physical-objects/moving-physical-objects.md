9 7 2026 1706 jorj todo - explain moving phsical objects, connects to  the implementation goal of like

ragdoll works like retrograd, shootuing guns is  phsical aim mode the gun shotos where it is aimed, realistic ish  world and harsh realsiitc etc, realsitic blood etc whatever

but this is suposed to explain like the ideal behabior with constraints like 

has to work server sided nad replicate ur position naturally, thru the same logical whatever  functions so we can send even less data than we currently do, becoming more efficient , less input for more output 

has to work with C:\mimita-priv-v8\docs\specs\visuals\blood-bulletholes-viscera.md this, blood bullet holes etc, should just prob be applied as pngs direct onto the surface of whatever  its affecting? so its low render cost and stuff 

has to be pick up able, and interact with the world similar wirth collisions, like a player would almost. so , walk up to a phsics object a crate, push it with ur shotgun = the crate moves automatically bc its a  phsical object. also abolutely implement a  gravity gun!!!!! that would be awesome!!!! and a portal gun omg omg omg 


todo 9 8 2026 1039 est jorj - do we need map editor fo rthis?  

# Physical Moving Objects

**Created:** 2026-09-07T17:06:00-04:00  
**Last updated:** 2026-09-09T10:01:00-04:00

## 1. Purpose

MiMITA should treat the simulated world as a unified physical system rather than a collection of unrelated feature-specific systems.

The long-term goal is:

> Everything visible or physically meaningful in the world should be representable as an entity composed from the same small set of generalized physical primitives.

Crates, players, ragdolls, weapons, projectiles, grenades, doors, debris, moving platforms, vehicles, body parts, world chunks, gravity-gun targets, portal-crossing objects, and future unknown objects should all use the same fundamental physical rules wherever possible.

The engine should prefer:

```text
general physical primitive
        ↓
general interaction
        ↓
emergent behavior
```

instead of:

```text
crate-specific behavior
ragdoll-specific behavior
weapon-specific movement behavior
grenade-specific attachment behavior
gravity-gun-specific physics behavior
```

The objective is **less specialized input producing more emergent output**.

---

# 2. Related systems

This specification must integrate with, rather than duplicate:

```text
docs/architecture/ecs-entity-etc/ecs.md
docs/specs/visuals/blood-bulletholes-viscera.md
docs/specs/networking/networking.md
docs/specs/weapons/weapons.md
docs/specs/weapons/melee-weapons.md
docs/specs/destruction/destructible-world.md
```

Exact paths may evolve, but ownership boundaries should remain clear.

The physical-object system defines generalized physics behavior.

Other specifications define how particular gameplay systems configure or consume those primitives.

---

# 3. Core rule

## 3.1 Everything is an entity

Every persistent world thing should have a stable entity identifier.

Conceptually:

```text
EntityId
```

identifies the thing.

Capabilities come from components.

Example:

```text
Entity 100
├── Transform
├── PhysicalBody
├── CollisionShape
├── Material
├── Renderable
├── Destructible
└── NetworkReplication
```

A different entity may contain:

```text
Entity 200
├── Transform
├── PhysicalBody
├── CollisionShape
├── Player
├── Health
├── NetworkReplication
└── ConstraintCollection
```

The physics layer should not need to know that one is a crate and the other is a player unless a component supplies behavior relevant to physics.

---

# 4. No feature-specific physics

Avoid logic such as:

```text
if objectIsCrate:
    pushCrate()

if objectIsRagdoll:
    moveRagdoll()

if targetIsStickyGrenadeTarget:
    attachStickyGrenade()
```

Prefer:

```text
collision()
applyImpulse()
applyForce()
createConstraint()
destroyConstraint()
transformThroughBoundary()
removeMaterial()
```

Feature behavior should emerge from combinations of these primitives.

Examples:

```text
walking into crate
=
collision + momentum transfer
```

```text
shotgun hitting crate
=
projectile/body collision + impulse transfer
```

```text
ragdoll arm attached to torso
=
constraint
```

```text
hand grabbing wall
=
constraint
```

```text
sticky grenade attached to player
=
constraint
```

```text
gravity gun holding crate
=
constraint with configurable target, spring, damping and force
```

```text
door hinge
=
angular constraint
```

---

# 5. Continuous space

## 5.1 World coordinates are continuous

MiMITA physics should not fundamentally operate on a fixed voxel grid.

Positions may be continuous floating-point or future deterministic equivalents:

```text
x = 12.438291 m
y = 3.000417 m
z = -8.721933 m
```

Objects do not need to occupy multiples of:

```text
0.001 m
0.01 m
1 cm
```

unless a particular optimization layer chooses to quantize something internally.

---

# 6. Minimum meaningful feature size

A minimum destructible feature size may exist **without making the world a grid**.

For example:

```text
minimumPhysicalFeatureSize = 0.001 m
```

would mean:

> The engine does not promise to preserve physically meaningful destruction details smaller than approximately 1 mm.

It does **not** mean that every point must lie on a 1 mm lattice.

Example:

Valid continuous positions:

```text
0.314159 m
0.314734 m
0.315991 m
```

even if the minimum persistent destructive feature is:

```text
0.001 m
```

This distinction should remain explicit.

### Continuous coordinate system

```text
any representable position
```

### Minimum meaningful physical detail

```text
smallest feature the engine promises to represent distinctly
```

These are separate concepts.

The minimum feature size should be hot reloadable and may vary by:

```text
distance
importance
material
platform
performance budget
server configuration
```

---

# 7. Physical body

A generalized physical body should support properties including, but not limited to:

```text
entityId

position
orientation

linearVelocity
angularVelocity

mass
inverseMass

centerOfMass

gravityScale

linearDrag
angularDrag

friction
restitution

sleepState

collisionLayers

materialId

destructibility

importance

simulationRate
```

Additional properties may be introduced as generalized needs emerge.

---

# 8. Multiple bodies and shapes per entity

A single entity may contain one or more physical bodies or collision shapes.

Example human:

```text
Player Entity 500

torso
head
leftUpperArm
leftLowerArm
rightUpperArm
rightLowerArm
leftUpperLeg
leftLowerLeg
rightUpperLeg
rightLowerLeg
```

Each can have:

```text
shape
mass
material
velocity
constraints
```

while still belonging to the same logical player entity.

Similarly:

```text
vehicle
├── chassis
├── wheel
├── wheel
├── wheel
└── wheel
```

or:

```text
machine
├── rotating arm
├── base
└── movable platform
```

The ECS should allow both:

```text
one entity → several physics components/bodies
```

and:

```text
parent logical entity → multiple child physical entities
```

depending on what produces cleaner ownership and replication.

---

# 9. Collision shapes

Collision should use a small collection of generalized primitives.

Initial examples:

```text
sphere
capsule
box
plane
triangle
convex shape
triangle mesh
```

Complex geometry should ultimately reduce to generalized collision operations between supported geometric primitives.

Do not create:

```text
crateCollision()
playerCollision()
grenadeCollision()
```

when equivalent generalized shape collision can solve the problem.

---

# 10. Triangles and curved objects

Rendered meshes may be composed from triangles.

That does not mean all mathematical physics objects must be triangle meshes.

A sphere may exist analytically as:

```text
center
radius
```

A capsule may exist as:

```text
segment
radius
```

A plane may exist as:

```text
normal
distance
```

These shapes may be infinitely smooth mathematically even when their rendered representation uses polygons.

Therefore MiMITA may combine:

```text
analytic collision geometry
+
polygonal rendering geometry
```

without conflict.

A visually smooth sphere does not need millions of physical triangles.

---

# 11. Materials

Physical materials should be data-driven.

Example properties may include:

```text
density

friction
staticFriction
dynamicFriction

restitution

compressionStrength
tensileStrength
shearStrength

penetrationResistance

fractureEnergy

hardness

thermalConductivity
heatCapacity
meltingPoint

soundProfile

surfaceVisualProfile
```

Examples:

```text
wood
steel
concrete
glass
rubber
flesh
bone
water
soil
```

should primarily be configurations of the same underlying material system.

---

# 12. Mass should emerge from physical properties where possible

Instead of manually assigning arbitrary object mass forever, the preferred long-term relationship is:

```text
mass = density × volume
```

with configurable overrides available when gameplay requires them.

Example:

```text
steel object
volume = 0.01 m³
density = 7850 kg/m³

mass ≈ 78.5 kg
```

Gameplay tuning may alter effective values, but the underlying model should remain understandable.

---

# 13. Forces, momentum and impulses

Movement should emerge from physical quantities.

Important concepts include:

```text
mass
velocity
acceleration
force
impulse
momentum
torque
angular momentum
```

Objects should react according to these quantities.

Example:

A player walking into a light crate should move it substantially.

A player walking into a heavy steel block should barely move it.

This should not require:

```text
if crate:
    move amount X
```

It should follow from:

```text
mass
relative velocity
contact
friction
impulse
```

---

# 14. Momentum transfer

Physical interactions should transfer momentum naturally.

Examples:

- landing hard on a crate can move or break it;
- a player can stand on another player;
- a fast player colliding with another player can shove them;
- a projectile can transfer momentum;
- an explosion can accelerate objects;
- a powerful weapon can produce recoil;
- an extremely powerful weapon may physically move the shooter;
- firing while ragdolled may move the weapon, arms, torso, head and therefore camera naturally.

Gameplay coefficients may be tuned for fun.

The mechanism should remain generalized.

---

# 15. Damage should derive from physical interaction

Traditional weapon-specific integer damage should not be the lowest-level model.

The desired direction is:

```text
physical interaction
↓
energy / momentum / penetration / material response
↓
physical damage
↓
gameplay consequences
```

For kinetic energy:

```text
E = 0.5 × m × v²
```

This does not mean every gameplay outcome must perfectly reproduce Earth physics.

It means weapon behavior should ideally emerge from physically interpretable variables.

Examples:

```text
projectile mass
projectile velocity
impact angle
target density
target hardness
target thickness
target structural strength
```

then determine:

```text
penetration
deformation
fracture
momentum transfer
damage
```

Fun can be introduced by tuning these inputs and coefficients rather than adding unrelated special cases.

---

# 16. Universal interaction event

The engine should converge toward a generalized representation of physical interaction.

Conceptually:

```text
InteractionEvent
{
    eventId
    tick

    sourceEntity
    targetEntity

    contactPoint
    contactNormal

    sourceVelocity
    targetVelocity
    relativeVelocity

    force
    impulse
    energy

    materialA
    materialB
}
```

Not all fields must be transmitted.

Many should be derived locally when possible.

The goal is for:

```text
player vs crate
crate vs crate
grenade vs grenade
bullet vs player
weapon model vs wall
ragdoll vs floor
debris vs player
```

to travel through the same general physical interaction pipeline.

---

# 17. Networking authority

## V1 rule

The server is final authority.

Clients may predict.

Clients may report:

```text
input
interaction claims
hit claims
expected target
expected contact information
```

but the server determines authoritative world state.

Entity IDs must identify physical entities consistently across server and clients.

---

# 18. Replicate causes before consequences

Avoid sending every object's complete transform every network update when the same result can be reconstructed from a smaller causal event.

Instead of repeatedly sending:

```text
Entity 100:
position = ...

Entity 100:
position = ...

Entity 100:
position = ...
```

prefer events such as:

```text
tick 50291

entity 100 received impulse:
(12.4, 2.1, -5.8)

at local point:
(0.4, 0.1, -0.3)
```

Every simulation then executes the same underlying physics.

This should substantially reduce required network traffic when large numbers of entities exist.

---

# 19. Authoritative checkpoints

Pure deterministic agreement is not required in V1.

Clients and server should attempt to simulate equivalent results from equivalent events.

However, floating-point differences, execution order, collision solver differences and other numerical issues may cause divergence.

Therefore the server may periodically send authoritative checkpoints containing state such as:

```text
entityId

tick

position
orientation

linearVelocity
angularVelocity
```

These checkpoints correct accumulated differences.

---

# 20. Correction behavior

Client correction should prioritize visual continuity.

Define configurable thresholds.

### Negligible error

Ignore it.

```text
error < tinyThreshold
→ no correction
```

### Small error

Smoothly converge.

```text
tinyThreshold < error < largeThreshold
→ interpolation / error decay
```

### Large error

Correct aggressively.

Possible methods:

```text
rewind + resimulation
hard correction
rapid convergence
```

The exact algorithm may evolve.

The behavioral objective is:

> Players should not see objects constantly snapping because the server and client disagree by tiny amounts.

---

# 21. Long-term networking goal

The desired end state is approximately:

```text
Client A
Client B
Client C
Client D
Server
```

receive equivalent causative events and execute equivalent simulation logic.

Therefore:

```text
what A sees
≈ what B sees
≈ what C sees
≈ what server sees
```

with increasingly rare corrections over time.

Perfect mathematical identity across all hardware is not required initially.

The system should be designed so determinism can improve incrementally.

---

# 22. Variable simulation frequency

Not every entity needs full simulation frequency.

Important local gameplay may run at:

```text
1.0× simulation frequency
```

Less important things may run at:

```text
0.5×
0.25×
0.125×
0.1×
```

or other configurable rates.

Example:

```text
nearby rocket
→ full frequency

crate being grabbed
→ full frequency

nearby ragdoll
→ full frequency

distant moving box
→ lower frequency

extremely distant decorative debris
→ extremely low frequency
```

Interpolation and extrapolation should visually smooth reduced-frequency simulation.

---

# 23. Importance-based scheduling

Simulation frequency should be selected from importance rather than merely object type.

Inputs can include:

```text
distance to players

visible/not visible

currently interacting

velocity

potential gameplay impact

recent collision

constraint participation

network relevance

destruction relevance

player ownership

camera relevance
```

An important faraway projectile may deserve more processing than an unimportant nearby blood droplet.

---

# 24. Sleeping physical objects

Objects should be capable of existing without being actively simulated.

Sleeping objects may preserve:

```text
entity identity
transform
material
damage
constraints
surface modifications
persistent state
```

while consuming almost no recurring physics CPU.

---

# 25. Wake conditions

Sleeping objects should wake naturally when physical relevance returns.

Examples:

```text
collision
nearby explosion
force application
impulse application
constraint creation
supporting geometry moves
another object falls onto it
player pushes it
projectile hits it
destructible support disappears
environmental force affects it
```

Prefer natural interaction-based wake events instead of specialized wake rules.

---

# 26. Scale target

Do not design around an arbitrary permanent maximum number of existing objects.

The long-term objective is:

> An extremely large number of entities should be able to exist because inactive entities cost almost nothing.

Distinguish carefully:

```text
existing entities
```

from:

```text
actively solved physical bodies
```

Millions or more persistent entities may eventually be viable if only a small active set requires processing.

The system should optimize active work rather than impose unnecessary limits on existence.

---

# 27. Central work queue

Physics should integrate into a generalized deferred-work scheduling system.

The long-term repository-wide direction is:

> Expensive work enters common scheduling infrastructure instead of every system inventing its own independent queue.

Potential users include:

```text
physics
destruction
effects
networking
map editing
world generation
audio processing
image editing
replay rendering
movie rendering
asset conversion
background persistence
LOD processing
```

---

# 28. Frame-time budget

Core gameplay has highest priority.

The scheduler should work toward configurable budgets such as:

```text
target simulation/frame budget ≤ 1 ms
```

when possible.

This should be treated as a target and optimization direction, not a claim that every future workload can already fit inside 1 ms.

Tasks outside the current budget should be:

```text
deferred
reduced in frequency
simplified
batched
distributed across frames
put to sleep
processed asynchronously where safe
```

instead of causing uncontrolled frame spikes.

---

# 29. Priority

Example conceptual priority order:

```text
P0
player input
player movement
critical collision
combat correctness

P1
nearby projectiles
active constraints
objects affecting players
important physics

P2
nearby destruction
important debris
important environmental motion

P3
ordinary visible world simulation

P4
distant objects

P5
cosmetic debris
blood movement
small visual effects
editor/background processing
```

The exact categories should be configurable.

The principle is:

> Preserve fun and gameplay correctness first.

---

# 30. Hot reload

Physics behavior should be hot reloadable during runtime wherever technically safe.

This includes:

```text
gravity
mass
density
material properties
friction
restitution
drag
constraint parameters
body-part properties
weapon properties
projectile properties
destruction properties
sleep thresholds
LOD thresholds
simulation frequency
scheduler budgets
```

The game should not require full restart cycles merely to experiment with physics behavior.

---

# 31. Runtime configuration changes

A configuration change should enter the generalized work queue.

Example:

```text
config edited
↓
change detected
↓
validated
↓
queued
↓
applied at safe simulation boundary
↓
dependent systems updated
```

The engine should avoid unpredictable half-applied states.

Changes affecting large numbers of entities may be distributed over multiple frames.

---

# 32. Versioned runtime changes

For deterministic debugging and networking, runtime changes should be identifiable.

Conceptually:

```text
PhysicsConfigRevision 872
```

A server may announce:

```text
revision 873 applies at simulation tick 500000
```

so every client changes behavior at the same logical time.

This becomes increasingly important if everything is hot reloadable.

---

# 33. Generalized constraints

Constraints should be a first-class generalized primitive.

Conceptually:

```text
Constraint
{
    constraintId

    entityA
    entityB

    anchorA
    anchorB

    linearLimits
    angularLimits

    spring
    damping

    maxForce
    maxTorque

    breakForce
    breakTorque
}
```

---

# 34. Constraints as entities/components

Constraints should themselves have stable identity where useful.

Example:

```text
Constraint 800
PlayerHand 1002
↔
WorldEntity 330
```

Creation and destruction of the constraint can replicate as generalized state.

This means the network does not need:

```text
playerStartedSpecialWallGrab()
```

It can receive:

```text
constraint 800 created
```

---

# 35. Constraint examples

The same fundamental system should support:

```text
arm attached to torso

head attached to torso

ragdoll hand grabbing wall

player grabbing player

player carrying crate

gravity gun holding object

rope

chain

hinge

door

sticky grenade

weapon attached to hand

grappling hook

suspension

moving machinery
```

These may use different parameter values but should not require completely separate attachment engines.

---

# 36. Ragdolls

Ragdolls should emerge from:

```text
multiple physical bodies
+
collision shapes
+
constraints
+
forces
```

not from a specialized animation approximation pretending to be physics.

Player view may be physically related to the head.

Therefore:

```text
head receives motion
→ camera receives motion
```

and:

```text
weapon recoil
→ hand
→ arm
→ torso
→ head
→ camera
```

may emerge physically.

Gameplay tuning can damp or modify this to remain fun.

---

# 37. Weapon recoil

Weapons should generate physical recoil based on momentum exchange where practical.

A weapon producing an extreme projectile should potentially produce extreme recoil.

Gameplay tuning may alter effective behavior.

Do not require:

```text
if weapon == SuperGun:
    pushPlayerBack(100)
```

when the same result can emerge from generalized physics values.

---

# 38. Gravity gun

A gravity gun should be implemented primarily as an application of the generalized constraint/force system.

Conceptually:

```text
player target transform
        ↕
spring/constraint
        ↕
target physical object
```

Parameters may define:

```text
holdDistance
springStrength
damping
maximumForce
maximumTorque
rotationControl
breakDistance
```

Any object compatible with the physical system should be eligible unless intentionally restricted by gameplay rules.

---

# 39. Portals

A portal should not treat objects as teleporting discontinuously if continuous traversal can be represented.

The conceptual model is:

> The portal connects two regions of space through a transformation.

A body may be partially on both sides.

---

# 40. Continuous portal crossing

When part of an object crosses a portal:

```text
crossing portion
→ represented through destination transform

remaining portion
→ remains in source space
```

The logical object remains the same entity.

Its identity does not change because it crossed the boundary.

---

# 41. Portal physics transformation

Portal traversal must transform physical quantities correctly.

At minimum:

```text
position
orientation
linearVelocity
angularVelocity
forces
constraint anchors
collision contacts
```

should transform through the portal coordinate mapping where applicable.

A fast object should preserve meaningful momentum through the portal.

---

# 42. Constraints through portals

Constraints should ideally remain valid when endpoints exist across portal boundaries.

Example:

```text
player arm here
hand through portal
hand grabbing crate there
```

should eventually be representable without introducing a specialized “portal grab” system.

This may require transform-aware constraint solving.

---

# 43. Destruction

Destruction belongs to the physical object.

A hole in a crate should not be stored merely as a world-space effect.

Example:

```text
crate receives hit
↓
local hit coordinates determined
↓
material altered
↓
crate moves
↓
damage remains attached to crate
```

Rotate the crate 90° and its physical damage rotates with it.

---

# 44. Physical destruction versus visual surface effects

Not every visible effect should require expensive geometric destruction.

Use two related representations:

## Physical modifications

Affect:

```text
volume
mass
collision
structural integrity
penetration
fracture
```

## Surface/visual modifications

Affect:

```text
blood
scorching
tiny scratches
stains
very fine cracks
surface coloration
```

These can coexist.

---

# 45. Bullet holes

A bullet hole may have:

```text
physical component
+
visual component
```

The physical component represents actual removed/deformed volume where necessary.

The visual component provides higher-frequency surface detail cheaply.

This avoids requiring every microscopic visual detail to become expensive collision geometry.

---

# 46. Blood and decals

Blood and similar surface marks should usually attach using the affected object's **local surface coordinates**.

Example:

```text
Entity 100
surface mark:
local position
local orientation
surface mapping
```

Moving the entity therefore moves the blood automatically.

Blood does not remain floating where the object used to be.

---

# 47. Destruction inheritance

When an object breaks, new physical entities should be created.

Example:

```text
Entity 100
breaks at tick 90120
```

produces:

```text
Entity 101
Entity 102
Entity 103
Entity 104
Entity 105
```

Each fragment should inherit appropriate properties from the parent.

Examples:

```text
material
density
temperature
surface modifications
velocity
angular velocity
ownership/history metadata
```

---

# 48. Conservation during fracture

When an object fractures, the resulting fragments should begin from physically coherent state.

Approximately:

```text
sum(fragment masses)
≈
remaining parent mass
```

and momentum should be distributed based on the fracture event.

Surface/material removal may reduce mass.

Explosive fracture can add energy.

---

# 49. Parentage/history

Fragments should retain traceable provenance.

Conceptually:

```text
Entity 104
parentEntity = 100
createdByEvent = 991821
createdTick = 90120
```

This helps:

```text
debugging
replay
network replication
persistent destruction
ownership
regression testing
```

---

# 50. Sound as physical consequence

Sound should increasingly derive from the same physical interaction data.

Examples:

```text
impact energy
materials involved
object mass
object size
relative velocity
contact geometry
```

can influence:

```text
volume
pitch
sample choice
resonance
duration
```

A tiny piece of metal hitting concrete should not sound identical to a 500 kg steel body hitting concrete.

---

# 51. Doppler and propagation

Moving sound sources should support physically motivated Doppler behavior.

Long-term propagation may incorporate:

```text
distance attenuation
speed of sound
occlusion
material transmission
reflection
environment
```

where performance allows.

These systems should consume generalized world and physical information rather than duplicate object classification.

---

# 52. Extreme physics

The architecture should not artificially prevent future support for extreme phenomena.

Potential future systems include:

```text
heat
thermal radiation
shock waves
pressure
fluids
aerodynamics
relativistic effects
gravitational fields
gravitational lensing
```

These are not required for initial moving-object implementation.

The important requirement is:

> Do not design V1 abstractions so narrowly that generalized physical effects later require rewriting every entity type.

---

# 53. Physical interaction pipeline

A desired generalized path is:

```text
broad-phase relevance
↓
possible interaction
↓
narrow-phase geometry test
↓
contact generated
↓
material properties read
↓
relative motion calculated
↓
forces/impulses solved
↓
constraints solved
↓
damage/destruction response
↓
effects generated
↓
sound generated
↓
network event/checkpoint generated where needed
↓
persistence updated where needed
```

Feature-specific systems should subscribe to the results rather than replacing the core pipeline.

---

# 54. Generalized collision function

The physics engine should converge toward a small number of functions capable of handling arbitrary supported pairs.

Conceptually:

```text
detectCollision(shapeA, transformA, shapeB, transformB)
```

returns generalized contact information.

Then:

```text
resolveContact(bodyA, bodyB, contact)
```

handles the physical interaction.

New gameplay objects should not require new pair-specific physics logic unless a genuinely new geometric primitive or fundamental physical behavior is being introduced.

---

# 55. ECS ownership

The ECS architecture document remains authoritative for entity/component ownership.

The moving-object system should follow it rather than creating a parallel object hierarchy.

Physics should preferably operate over component queries such as:

```text
entities with:
Transform
PhysicalBody
CollisionShape
```

rather than maintaining unrelated duplicated game-object structures.

---

# 56. Data-oriented scaling

The implementation should be compatible with processing large numbers of similar components efficiently.

Preferred directions may include:

```text
contiguous component storage
batch processing
spatial partitioning
SIMD
job batching
sleeping sets
interest management
dirty-state tracking
```

Implementation choices may evolve.

The behavioral specification should not unnecessarily mandate one exact memory layout.

---

# 57. Spatial relevance

Do not test every object against every other object.

Use spatial acceleration structures.

Potential implementations include:

```text
BVH
spatial hash
uniform grid
octree
dynamic AABB tree
hybrids
```

The exact structure is implementation-owned.

Behavioral requirement:

> Work should scale primarily with relevant nearby interactions, not total world entity count.

---

# 58. Dirty state

An unchanged sleeping entity should not repeatedly create work.

Track changes.

Examples:

```text
transformDirty
velocityDirty
constraintDirty
materialDirty
destructionDirty
networkDirty
renderDirty
```

Systems can consume only required changes.

---

# 59. Replication relevance

Not every physical event should be sent to every client.

Network interest management should consider:

```text
distance
visibility
potential future interaction
ownership
importance
spectating/replay requirements
```

The authoritative server still maintains world truth.

Replication should transmit only what each client requires to reproduce relevant state.

---

# 60. Deterministic event ordering

Every authoritative physical event should have logical ordering.

At minimum:

```text
simulation tick
event id / sequence
```

This ensures that:

```text
impulse
constraint creation
fracture
config revision
```

can be applied consistently.

---

# 61. Replays

Replay systems should record generalized causes where possible.

Instead of recording enormous transform streams for every physical object, replay may record:

```text
input
physical events
constraint events
fractures
configuration revisions
periodic checkpoints
```

then reproduce simulation.

Periodic snapshots may guarantee bounded divergence.

---

# 62. Persistence

Persistent physical state should store the minimum information necessary to reconstruct the world.

Examples:

```text
entity state
transform
sleep state
material changes
fracture history or current fragments
surface modifications
persistent constraints
```

Inactive historical computation should not need to continue while no one is observing or interacting with it.

---

# 63. Performance degradation

When resource limits are reached, fidelity should degrade gracefully.

Prefer:

```text
lower simulation frequency
sleep sooner
reduce cosmetic detail
batch updates
defer noncritical work
simplify far collision
reduce audio/effect fidelity
```

before compromising:

```text
player input
movement
combat
nearby collision
critical network correctness
```

---

# 64. No permanent arbitrary caps where avoidable

Configuration may contain safety limits.

For example:

```text
maxActiveBodies
maxFragmentsPerEvent
maxPhysicsWorkPerTick
```

but these should be treated as operational safeguards rather than foundational assumptions.

Over time, optimization should allow limits to increase.

The architectural objective remains:

> Existing object count should not inherently determine per-frame cost.

---

# 65. Configuration

General moving/destruction/physics behavior may be configured through files such as:

```text
C:\mimita-priv-v8\config\destructible-world.json
```

and other generalized physics configuration files.

Avoid duplicating the same property across many feature configs unless there is a clear ownership reason.

Potential configuration fields include:

```text
simulationHz

distanceLod[]

sleepThreshold
sleepDelay

minimumPhysicalFeatureSize

maxPhysicsBudgetMs

correctionTinyThreshold
correctionLargeThreshold

constraintDefaults

materialDefaults

destructionLod

networkCheckpointFrequency
```

---

# 66. Example emergent interaction: shotgun versus crate

No specialized shotgun-crate function should be required.

```text
trigger pulled
↓
weapon/projectile event occurs
↓
projectile collides with crate shape
↓
contact point calculated
↓
relative velocity calculated
↓
momentum/energy transfer calculated
↓
crate receives impulse
↓
off-center impulse creates rotation
↓
material system tests penetration/fracture
↓
physical volume may be removed
↓
surface effect added locally
↓
sound derives from materials + energy
↓
crate continues moving
↓
bullet hole moves with crate
```

---

# 67. Example emergent interaction: player lands on crate

```text
player falling
↓
player collision body contacts crate
↓
relative velocity calculated
↓
contact solver exchanges impulse
↓
crate moves downward
↓
player decelerates
↓
material/structure receives stress
↓
crate may survive or fracture
```

No:

```text
playerLandedOnCrate()
```

special behavior should be required at the lowest level.

---

# 68. Example emergent interaction: recoil while ragdolled

```text
weapon fires projectile
↓
momentum leaves weapon
↓
opposite impulse applied to weapon
↓
weapon-hand constraint transfers force
↓
hand-arm constraints transfer force
↓
arm-torso transfer
↓
torso/head react
↓
camera follows head
```

The final experience can be tuned using:

```text
mass
spring
damping
recoil scale
constraint stiffness
camera filtering
```

rather than replacing the physical chain.

---

# 69. Example emergent interaction: gravity gun

```text
player selects crate
↓
constraint created
↓
target anchor follows desired hold position
↓
spring/damping solve applies forces
↓
crate accelerates naturally
↓
crate collides with world normally
↓
mass affects responsiveness
↓
maximum force prevents impossible acceleration if configured
```

Throwing may simply destroy the hold constraint and apply an impulse.

---

# 70. Example emergent interaction: portal

```text
crate approaches portal
↓
collision boundary identifies intersecting geometry
↓
crossing region transforms into destination space
↓
same entity remains active
↓
velocity/orientation transformed
↓
remaining portion stays in original space
↓
crate continues crossing
↓
eventually entire object resides through destination
```

The crate should not become a new crate.

---

# 71. Example emergent interaction: sticky grenade

```text
grenade collides with target
↓
contact confirmed
↓
constraint created between grenade and target surface/body
↓
target moves
↓
grenade follows naturally
↓
constraint may break if break-force threshold exceeded
```

The same system works on:

```text
wall
crate
player
ragdoll
vehicle
moving platform
another grenade
```

---

# 72. Implementation philosophy

When adding a feature, ask:

> What generalized primitive is missing?

rather than:

> What special-case code makes this particular feature work?

If ragdoll grabbing cannot be expressed:

Improve constraints.

If sticky grenades cannot attach correctly:

Improve constraints/contact anchors.

If crates cannot fracture properly:

Improve generalized destruction.

If portal-held objects fail:

Improve coordinate-transform-aware constraints.

The primitive gets stronger, and every existing feature benefits.

---

# 73. Validation philosophy

A feature is not considered integrated merely because it appears to work once.

Verify its data path.

For a physical interaction, validation should eventually trace:

```text
configuration
↓
entity/component creation
↓
simulation
↓
collision
↓
force/constraint result
↓
server authority
↓
network replication
↓
client prediction/correction
↓
render state
↓
persistence/replay if applicable
```

Build success alone does not prove the physical feature works.

---

# 74. Initial implementation scope

The first implementation does not need to implement every future physical phenomenon.

V1 should focus on a generalized foundation supporting:

```text
stable EntityId-based bodies

sphere/capsule/box/basic mesh collision

mass

linear/angular velocity

force

impulse

gravity

friction

restitution

basic materials

sleep/wake

variable simulation frequency

server-authoritative simulation

client prediction

smooth correction

periodic authoritative checkpoints

general constraints

object pushing

object carrying

ragdoll integration

weapon recoil interaction

destructible-object integration

local-space surface modifications

fracture into child entities

work scheduling / prioritization
```

---

# 75. Explicit non-goal for V1

V1 does not need perfect implementations of:

```text
general relativity
gravitational lensing
fluid dynamics
thermodynamics
full material continuum mechanics
perfect deterministic physics across every CPU
billions of simultaneously active rigid bodies
perfectly continuous portal collision
```

These are future directions.

The current implementation should avoid architectural decisions that unnecessarily prevent them.

---

# 76. Success criteria

The moving-object foundation is succeeding when examples such as these require little or no feature-specific physics code:

```text
walk into crate → crate moves

jump onto crate → momentum transfers

shoot crate → crate reacts

shoot light object → strong motion

shoot heavy object → weak motion

off-center shot → rotation

powerful weapon → recoil affects shooter

ragdoll hits object → both react

player stands on player → physical contact works

grenade sticks to moving crate

gravity gun picks up arbitrary compatible object

moving object retains bullet hole

object breaks → child entities continue physically

far object reduces simulation frequency smoothly

sleeping object costs almost nothing

collision wakes sleeping object

server and clients converge on same physical state
```

---

# 77. Ultimate architectural goal

The long-term goal is an engine where increasingly complicated behaviors arise from increasingly capable simple primitives.

Conceptually:

```text
Entity
+
Geometry
+
Material
+
Mass
+
Forces
+
Collision
+
Constraints
+
Energy
+
Time
+
General scheduler
```

produces:

```text
players
ragdolls
weapons
destruction
vehicles
gravity guns
portals
machines
debris
world interaction
future systems not yet imagined
```

MiMITA should continue moving toward:

> **one physical world, one set of fundamental rules, many emergent behaviors.**

Whenever possible, improve the shared rule instead of introducing a feature-specific exception.