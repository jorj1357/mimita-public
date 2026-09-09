9 7 2026 1658 est jorj - this should be the big implementation for the alreadt existing but  bad performance   of blood spray when shooting otherrs, bullet holes on the world, cracks on the world, bombs exploding leave a big mark onteh ground. and relates to like camera sway and stuff for realism, like bobm exploding near u in realistic mode = u can 

# Generalized Effects, Blood, Gore, and Persistent Visual Consequences

Created: 2026-09-07T16:58:00-04:00  
Last updated: 2026-09-09  
Status: design / implementation specification

## 1. Purpose

This document defines the generalized effects system used for blood, gore, bullet impacts, world marks, debris, smoke, sparks, cracks, explosion visuals, camera reactions, audio reactions, and other visual or physical consequences.

The primary immediate goal is to preserve or improve the existing visual quality of blood spray, blood impacts, bullet holes, and related effects while radically reducing their frame-time cost.

The current problem is not that the desired visuals are wrong.

The current problem is:

> the desired output is known, but the current representation and implementation are too expensive.

Optimization may change representation, batching, scheduling, simulation frequency, LOD, storage, networking, and rendering implementation.

Optimization must not unnecessarily change the intended observable result.

---

# 2. Core Principle

The effects system must be generalized.

Do not create independent one-off systems such as:

```text
BloodEffectSystem
BulletHoleSystem
ExplosionScorchSystem
DashEffectSystem
GoreSystem
DebrisEffectSystem
```

when the same generalized effect machinery can represent them.

Prefer:

```text
Effect(...)
```

with configurable data describing:

```text
source
target
event type
position
direction
velocity
force
material
geometry
lifetime
persistence
render mode
simulation mode
LOD rules
network rules
collision behavior
spawn behavior
```

The generalized effects system should continuously expand so that new effect types are usually new data/configuration or new generalized capabilities rather than entirely separate systems.

---

# 3. Relationship to Other Systems

This system interacts heavily with:

```text
docs/specs/destructible-world/destructible-world.md
```

and the existing collision, physics, networking, materials, camera, sound, ECS, and world-object systems.

The effects system does not replace authoritative physical destruction.

For example:

- a visual blood stain may be handled entirely by the effect system,
- a physical bullet hole that can be walked through belongs to destructive world state,
- debris may begin as destruction-generated physical objects and use the effects system for presentation,
- smoke may be cosmetic or gameplay-relevant depending on configuration.

The systems must compose rather than duplicate each other's responsibilities.

---

# 4. Fundamental Separation

The engine must separate:

```text
authoritative world truth
simulation state
visual representation
rendering
```

These are not the same thing.

Example:

A crate was shot five times.

The authoritative world may contain five persistent changes.

That does not imply:

```text
5 expensive entities
× 60 simulation ticks/s
× 60 render updates/s
× 12 hours
```

The engine may represent those changes however is cheapest while preserving the same observable state.

Example:

```text
authoritative truth
    ↓
near player
    detailed physical geometry + detailed visual effects
    ↓
far player
    simplified representation
    ↓
offscreen
    no rendering
    ↓
irrelevant and stationary
    no active simulation
```

When the player returns, the appropriate representation is reconstructed.

---

# 5. Effect Invocation

Effects should normally emerge from existing gameplay events.

Example collision flow:

```text
projectile collides with entity
        ↓
generic collision system resolves collision
        ↓
collision generates authoritative event/context
        ↓
effect system receives event
        ↓
effect system evaluates:
    source type
    target type
    material
    impact position
    impact direction
    relative velocity
    mass
    force
    hit region
    configuration
        ↓
appropriate effects are generated
```

Example:

```text
bullet
+
player head
+
impact velocity
+
impact direction
```

may naturally generate:

```text
blood spray
blood surface stain
impact particles
audio
camera effects if local player
physical damage
future gore/destruction behavior
```

There should not need to be arbitrary code such as:

```text
if weapon == revolver:
    spawnSpecificRevolverBlood()
```

unless genuinely unique behavior cannot be expressed through generalized inputs.

---

# 6. Blood V1

V1 blood behavior is intentionally simpler than the eventual simulation.

V1 includes:

- initial blood spray,
- collision of blood with nearby surfaces,
- blood stain/decal generation,
- directional spray based on impact,
- client-side presentation,
- configurable particle count,
- configurable size,
- configurable velocity,
- configurable spread,
- configurable texture,
- configurable lifetime where applicable,
- persistent resulting stains where configured.

V1 does not require:

- fluid simulation,
- dripping,
- flowing blood,
- spreading pools,
- transfer between surfaces,
- footprint tracking,
- physically simulated liquid volume,
- realistic organ behavior.

Those can be V2+.

---

# 7. Blood V2+

Future versions may expand the same generalized system to support:

- dripping,
- pooling,
- flowing,
- smearing,
- blood transfer,
- blood on moving objects,
- blood interacting with explosions,
- blood responding to wind or forces,
- body parts,
- organs,
- viscera,
- tissue destruction,
- detached geometry,
- fluid-like material behavior.

This should still emerge from generalized materials, collision, destruction, physics, and effects systems.

Avoid creating a special isolated gore engine if the shared systems can represent the same behavior.

---

# 8. Effect Representation Modes

An effect may have different representations.

Examples include:

```text
camera-facing PNG/billboard
surface decal
3D textured plane
mesh
particle
physical collider
physical entity
procedural geometry
volume
surface-state modification
shader effect
audio effect
camera effect
```

The same logical effect may use different representations depending on:

```text
distance
screen-space size
visibility
importance
hardware
current frame budget
age
motion
gameplay relevance
```

Example blood:

Near:

```text
3D particles
+
surface collision
+
detailed stain
```

Far:

```text
reduced particle count
+
simplified stain
```

Very far:

```text
resulting stain only
```

Offscreen:

```text
no rendering
```

---

# 9. Persistent Effects

Effects may be persistent.

Example requirement:

> Shoot a crate five times, continue playing on the same world/server for 12 hours, return to the crate, and the consequences should still exist.

Persistence does not mean the effect must remain actively simulated.

Persistent state should become increasingly cheap after interaction ends.

Examples:

```text
active flying debris
    ↓
settled debris
    ↓
sleeping physical object
```

or:

```text
individual blood particles
    ↓
surface impacts
    ↓
merged blood surface state
```

or:

```text
many overlapping bullet modifications
    ↓
merged current geometry
```

The server should store the resulting world state, not wastefully replay every historical operation every frame.

---

# 10. Persistent World Storage

Long-term persistent server storage is a future architecture requirement.

Potential systems may include:

```text
base world
+
deterministic seed
+
mutation/event stream
+
periodic compressed snapshots
+
hash verification
=
current world state
```

This spec does not mandate one exact persistence architecture yet.

A random seed alone cannot generally encode arbitrary future player decisions.

The system may eventually use:

- deterministic event streams,
- snapshots,
- compression,
- content-addressed storage,
- hashes,
- distributed storage,
- client-assisted storage,
- server persistence.

These are future implementation concerns.

The behavioral requirement is persistence.

---

# 11. Determinism

Effects should be deterministic wherever practical.

Given identical authoritative inputs:

```text
event ID
seed
position
direction
velocity
material
target state
configuration
```

clients should reproduce the same result.

Example:

```text
eventId = 839152
seed = 59102951
type = projectileImpact
target = entity 918
position = ...
velocity = ...
material = flesh
```

The client may derive:

```text
particle directions
particle sizes
blood textures
debris rotations
particle timing
minor visual variation
```

from the deterministic seed.

This reduces network traffic.

The server should not need to individually transmit every cosmetic particle.

---

# 12. Replay Determinism

Long term, deterministic simulation should make replay data extremely compact.

The conceptual goal is:

```text
initial world state
+
seed(s)
+
authoritative player inputs/events
=
reproducible game/session
```

A single seed alone cannot describe millions of arbitrary player decisions.

However, a small deterministic initial state plus compressed authoritative input/event history may reproduce enormous amounts of derived simulation.

The engine should avoid transmitting or recording data that can be deterministically reconstructed.

---

# 13. Client and Server Ownership

## Server owns

The server owns authoritative gameplay truth.

Examples:

```text
damage
player state
destructible geometry
actual physical holes
gameplay-relevant debris
gameplay-relevant smoke
persistent world mutation
authoritative collision events
physical impulses
```

## Client owns

Clients own cosmetic presentation derived from authoritative events.

Examples:

```text
blood particles
minor debris
sparks
cosmetic dust
camera shake
camera sway
ear ringing presentation
muffled audio presentation
surface decals where non-authoritative
cosmetic particle animation
```

The server does not render cosmetic effects.

The server sends sufficient authoritative event information for clients to reproduce them.

---

# 14. Visibility Rule

The strongest default optimization rule is:

> If work cannot affect anything observable, do not perform the work.

Examples:

If an effect is behind the camera:

```text
do not render it
```

If completely occluded:

```text
do not render it
```

If its visual detail cannot be perceived at the current distance:

```text
do not calculate that detail
```

If a persistent stain has not changed:

```text
do not continuously update it
```

If an object is stationary and asleep:

```text
do not spend full-rate simulation work on it
```

---

# 15. Offscreen Physical Simulation

Rendering and physical continuity are separate.

Example:

```text
explosion happens
↓
debris flies through air
↓
player turns away
↓
debris is not rendered
↓
player turns back
↓
debris is where physics says it should now be
```

Turning away must not freeze physical truth.

However, the engine does not necessarily need to perform every intermediate full-detail step.

For deterministic systems it may:

```text
store state + time
↓
skip expensive intermediate presentation
↓
analytically or deterministically advance state
↓
reconstruct current state when relevant again
```

The result should appear as though the world continued existing.

---

# 16. Level of Detail

LOD should preferably be continuous rather than only fixed levels.

An object/effect gets an importance score.

Conceptually:

```text
importance =
    visibility
    × screenSpaceSize
    × proximity
    × gameplayRelevance
    × motionImportance
    × recency
    × interactionPotential
```

The exact formula is implementation-defined and configurable.

Higher importance receives more:

```text
simulation frequency
render detail
network frequency
particle count
collision precision
animation precision
memory priority
```

Lower importance receives less.

---

# 17. Distance Scaling

Distance should affect at least:

```text
rendering detail
particle count
simulation rate
collision detail where safe
network replication frequency
animation frequency
audio complexity
shadow complexity
effect complexity
```

Example conceptually:

```text
very close:
    full-rate / full-detail

medium:
    reduced detail

far:
    reduced update rate

extremely far:
    extremely low rate such as 1 Hz or 0.1 Hz where safe

invisible:
    no rendering
```

There should not be an assumption that every entity needs 60 Hz full-detail updates at every distance.

---

# 18. Screen-Space Importance

Distance alone is insufficient.

A huge explosion 500 meters away may matter more visually than a tiny blood droplet 5 meters away.

LOD should consider screen-space contribution.

Example:

```text
apparent pixel area
```

may be more important than raw distance.

If an effect is smaller than a perceptible threshold, detailed rendering should stop.

---

# 19. Unified Performance Budget

The effects system has a universal performance budget.

The goal is to approach:

```text
0 ms additional frame time
```

as closely as physically possible while preserving intended output.

The engine should track actual:

```text
CPU frame time
GPU frame time
memory consumption
network cost
effect queue latency
```

Effects must never be allowed to significantly degrade core gameplay responsiveness.

---

# 20. Gameplay Priority

Performance priority should broadly follow:

```text
1. player input
2. movement
3. gameplay-critical physics
4. networking
5. collision
6. damage/game rules
7. authoritative world state
8. important visible effects
9. distant effects
10. invisible cosmetic work
```

Lower-priority work degrades before higher-priority work.

Cosmetics should never cause movement or networking to become unstable merely because many effects were spawned.

---

# 21. Adaptive Frame-Time Target

The engine should support a user-selected desired frame-rate/frame-time target.

Examples:

```text
60 FPS   = 16.67 ms
120 FPS  = 8.33 ms
144 FPS  = 6.94 ms
240 FPS  = 4.17 ms
360 FPS  = 2.78 ms
```

The engine measures actual runtime frame time and adapts effect workload dynamically.

The system should not rely exclusively on presets such as:

```text
Low
Medium
High
Ultra
```

Instead:

```text
target performance
+
available measured resources
+
user visual preferences
=
dynamic quality
```

---

# 22. User Settings

Users should still be able to explicitly control individual effect categories.

Examples:

```text
blood = on/off
gore = on/off
bullet holes = on/off
debris = on/off
smoke = on/off
camera shake = on/off
ear ringing = on/off
shadows = on/off
```

Where possible, settings UI may estimate expected frame-time impact.

Example:

```text
Blood: High
Estimated GPU cost: +0.18 ms
Estimated CPU cost: +0.06 ms
```

These values should be measured dynamically where possible rather than hard-coded assumptions.

---

# 23. Dynamic Quality Allocation

Effects receive compute proportional to importance until the performance budget is exhausted.

Conceptually:

```text
sort effects by importance
↓
allocate highest quality to most important
↓
continue downward
↓
budget reached
↓
reduce / defer / skip low-value work
```

The quality selection should adapt every frame or over a smoothed time window.

Avoid abrupt visual oscillation when possible.

Use hysteresis/smoothing so effects do not constantly jump between quality levels.

---

# 24. No Hard Global Effect Count

Avoid arbitrary architecture like:

```text
maxBulletHoles = 100
maxBlood = 250
maxDebris = 500
```

as the primary solution.

The logical system should support extremely large quantities.

Performance should instead come from:

```text
batching
merging
compression
LOD
sleeping
culling
instancing
reduced simulation
deterministic reconstruction
surface baking
spatial partitioning
queueing
```

Practical safety limits may exist, but they should prevent catastrophic resource exhaustion rather than define intended game behavior.

---

# 25. Merging and Compression

Multiple effects that produce equivalent state should be merged.

Example:

Instead of:

```text
hole 1
hole 2
hole 3
...
hole 50,000
```

the system may store:

```text
current resulting geometry region
```

if that contains equivalent information.

Likewise blood may transition from:

```text
hundreds of individual decals
```

into:

```text
one baked surface representation
```

without changing the visible result.

The server should care about current truth, not preserving expensive redundant representation of historical operations.

---

# 26. Effect Batching

Effects should be batchable wherever possible.

Examples:

```text
many blood particles → one GPU batch
many decals → atlas/batched surface representation
many impact effects → instanced draw
many static marks → baked texture/mesh
many events → compact network packet
```

Batching should happen automatically based on compatible state.

---

# 27. Effect Queue

If immediate effect work would exceed the allowed budget, non-gameplay-critical work may enter a queue.

Example:

```text
huge blood event
↓
essential visible response happens immediately
↓
remaining expensive refinement is queued
↓
work is distributed across later frames
```

This prevents frame spikes.

However, effects expected to appear immediately should normally not need queueing.

If an important effect repeatedly enters the queue, the engine should log that as an optimization signal.

Example log:

```text
[Effects][Perf]
Immediate effect exceeded budget and was deferred.
EffectType=blood_spray
EventId=...
QueuedWork=...
EstimatedCostMs=...
TargetBudgetMs=...
```

The intended interpretation is:

> the effect implementation itself may need to become more efficient.

The queue is protection, not an excuse for permanently inefficient code.

---

# 28. Frame-Time Spikes

Optimization must consider frame-time distribution, not only average FPS.

Bad:

```text
average FPS = 300
but occasional effects cause 20 ms frames
```

Good:

```text
stable low frame-time variance
```

Performance instrumentation should measure:

```text
average
median
p95
p99
maximum
spike count
```

for CPU and GPU effect cost where practical.

---

# 29. Materials

Effects should use the generalized material system.

Materials may define:

```text
density
strength
hardness
fracture behavior
penetration resistance
deformation behavior
blood/tissue response
debris properties
particle type
surface decal response
sound
friction
temperature response
future chemical response
```

Example materials:

```text
flesh
bone
steel
wood
concrete
glass
dirt
rubber
water
```

Do not encode these characteristics per weapon.

---

# 30. Bullet Impacts

Projectile impacts use generalized collision/destruction rules.

Examples:

Revolver:

```text
narrow penetration
small material displacement
corresponding debris/effect
```

Shotgun:

```text
multiple individual pellet impacts
multiple smaller penetration regions
```

Rocket:

```text
large force
large deformation
debris
dust
scorching where appropriate
```

Physical holes are governed by destructive-world behavior.

This document governs presentation and generalized effects created by those interactions.

---

# 31. Blood Direction

Blood spray should respond to physical impact information.

Inputs may include:

```text
projectile direction
relative velocity
impact normal
target velocity
projectile momentum
impact energy
hit region
material properties
```

Example:

A projectile entering the front of a target should generally generate spray that follows the physically plausible resulting direction.

Avoid arbitrary fixed spray directions.

---

# 32. Effect Interaction

Long term, effects should naturally interact.

Examples:

```text
explosion moves debris
explosion disturbs blood
blood lands on debris
debris carries blood with it
fire changes stained surfaces
water washes material
smoke moves with forces
```

These should emerge from generalized physics/material rules rather than special-case scripts where possible.

---

# 33. Explosion Response

Explosions should drive multiple systems through one generalized physical event.

Conceptually:

```text
response =
    sourceMagnitude
    × distanceFalloff
    × directionalRelationship
    × obstruction
    × materialInteraction
```

Outputs may include:

```text
player impulse
object impulse
camera displacement
camera shake
camera sway
ear ringing
temporary muffled audio
debris generation
dust
smoke
surface damage
world deformation
```

Do not implement simple binary rules such as:

```text
if within 5m:
    shake camera
```

when continuous physical relationships can produce better results.

---

# 34. Camera Effects

Camera effects may include:

```text
shake
sway
kick
displacement
impact reaction
explosion reaction
landing reaction
environmental forces
```

Intensity should generally scale continuously with the physical event.

Camera effects should remain client-side presentation.

They must not alter authoritative player position unless the actual physical event applies a gameplay impulse separately.

---

# 35. Audio Effects

The same event may also generate audio-state effects.

Examples:

```text
ear ringing
temporary muffling
low-pass filtering
volume reduction
directional audio
distance attenuation
```

These should scale with physical circumstances.

Example:

A nearby explosion should produce a stronger auditory reaction than a distant explosion.

Obstruction and material should eventually matter.

---

# 36. 3D PNG / Billboard Mode

The effect system must support camera-facing images in world space.

Examples:

```text
blood sprites
smoke
sparks
stylized particles
temporary impact flashes
```

Supported configuration should include:

```text
faceCamera
orientationMode
worldSize
screenSizeBehavior
texture
alpha
animation
lifetime
depth behavior
lighting behavior
```

This allows inexpensive presentation when actual 3D geometry would produce no meaningful visual benefit.

---

# 37. Surface Decal Mode

Effects should support applying PNG/image-based visual state directly to surfaces.

Examples:

```text
blood
scorch marks
paint
dirt
small cracks
nonphysical bullet marks
```

Large quantities must not require one independent expensive render object per decal indefinitely.

The renderer should be capable of:

```text
batching
atlas use
baking
merging
chunk-based surface textures
virtualized surface state
```

over time.

---

# 38. Physical Effect Mode

Some effects may become physical entities.

Examples:

```text
large gore chunks
body parts
debris
shell casings
broken geometry
```

Physical objects should use ECS and generalized physics rather than bespoke effect-only physics.

They should support sleeping and aggressive LOD when they stop being relevant.

---

# 39. Spatial Partitioning

Effects must be spatially indexed.

Queries should avoid scanning every effect in the world.

The engine should be able to efficiently determine:

```text
what is near the camera?
what is visible?
what is inside this world chunk?
what has changed?
what needs simulation?
what needs networking?
```

using spatial acceleration structures appropriate to the engine architecture.

---

# 40. World Chunking

Persistent effects should preferably associate with world regions/chunks.

Example:

```text
world
 ├── chunk A
 │    ├── blood surface state
 │    ├── damage state
 │    └── debris
 ├── chunk B
 └── chunk C
```

This enables:

```text
loading
unloading
compression
persistence
network interest management
LOD
```

without needing every world modification globally active.

---

# 41. Networking

The effects system should minimize bandwidth.

Prefer sending:

```text
authoritative event
+
seed
+
essential physical state
```

rather than:

```text
every particle position
every particle velocity
every animation frame
```

Clients reconstruct cosmetic details.

---

# 42. Network LOD

Replication frequency may scale with importance.

Conceptually:

```text
near / gameplay critical:
    high-frequency replication

far:
    reduced frequency

extremely far:
    potentially ~1 Hz or ~0.1 Hz where safe

irrelevant:
    no replication until needed
```

Exact numbers should not be universally hard-coded.

Interpolation, prediction, deterministic reconstruction, and event-driven updates should hide reduced frequency where possible.

---

# 43. Static Effects Should Become Nearly Free

Once an effect becomes static, its ongoing CPU cost should approach zero.

Example:

A blood stain that has existed for 30 minutes should not have meaningful per-frame CPU cost.

A bullet hole in a stationary wall should not require an active update loop.

A sleeping debris chunk should not run expensive simulation.

Rendering cost should still be minimized through batching, culling, and representation changes.

---

# 44. Configurability

Behavior should be configurable through JSON where reasonable.

Examples:

```text
effects.json
blood.json
materials.json
performance.json
camera-effects.json
audio-effects.json
```

Exact file organization should follow repository architecture.

Values should be hot-reloadable where practical.

Configurable fields may include:

```text
spawn rates
particle counts
particle sizes
velocity ranges
spread
LOD curves
distance weighting
importance weighting
frame budgets
queue limits
merge thresholds
sleep thresholds
network rates
visual modes
textures
effect toggles
```

---

# 45. No Weapon-Specific Visual Rules by Default

Avoid:

```text
if revolver:
    bloodCount = 20
if shotgun:
    bloodCount = 100
```

Prefer effects derived from:

```text
impact energy
projectile count
impact area
material
velocity
mass
penetration
```

Weapons become configurations that generate physical conditions.

Effects respond to those conditions.

---

# 46. Manual Validation First

Initial validation should be manual.

The existing/previous visually good blood implementation and recorded gameplay video may serve as qualitative reference material.

The process should be:

```text
hypothesis
↓
implementation
↓
human observation
↓
identify what objectively constitutes good behavior
↓
write falsifiable regression test
↓
automate it
```

Do not begin by allowing an automated test to invent what “good blood behavior” is supposed to mean.

---

# 47. Regression Development

Once behavior has been manually established, regressions should cover cases such as:

```text
single bullet impact
continuous automatic fire
shotgun impact
multiple players bleeding
100 blood impacts
1,000 blood impacts
10,000 persistent marks
many overlapping decals
many explosions
effects behind camera
effects behind occluders
effects at extreme distance
turn away / turn back
server persistence
deterministic replay
performance overload
```

---

# 48. Visual Regression

Where practical, save:

```text
screenshots
short replay clips
event seeds
camera position
configuration
```

so the exact same effect scenario can be reconstructed.

Tests should detect accidental visual degradation after optimization.

---

# 49. Performance Regression

Effect performance tests should capture:

```text
CPU time
GPU time
frame time
p95 frame time
p99 frame time
worst frame
memory
draw calls
effect count
particle count
network bandwidth
queued work
queue latency
```

A build should eventually fail regression checks when effect performance substantially worsens.

Exact thresholds should be based on manually validated real behavior first.

---

# 50. Performance Logging

The engine should provide detailed effect performance diagnostics.

Example:

```text
effects active: 18,492
effects rendered: 411
effects simulated: 684
effects culled: 17,808
effects sleeping: 12,229

CPU effects: 0.31 ms
GPU effects: 0.72 ms
network effects: 4.8 KB/s

queued effects: 3
oldest queued work: 7.2 ms

blood decals logical: 8,221
blood decal render batches: 14
```

The difference between logical count and actual render/simulation cost should be observable.

---

# 51. Queue Diagnostics

If an effect expected to be immediate is deferred, log why.

Example:

```text
effect:
bloodImpact

reason:
CPU budget exceeded

budget:
0.40 ms

predicted additional work:
0.16 ms

decision:
render immediate essential spray
defer surface merge until later frame
```

This makes hidden performance degradation discoverable.

---

# 52. Graceful Degradation

When the effects budget is exceeded, degradation should be perceptually prioritized.

Example order:

```text
reduce imperceptible particles
reduce distant particles
reduce tiny debris
reduce simulation frequency
simplify geometry
merge decals
disable invisible work
defer background refinement
```

Do not suddenly remove the most obvious nearby effect while continuing to simulate irrelevant distant detail.

---

# 53. Essential Immediate Response

Every gameplay action should still feel responsive.

Example bullet hit:

Immediately:

```text
impact response
essential blood spray
audio
damage feedback
```

Potentially deferred:

```text
expensive decal merging
surface baking
background compression
distant secondary particles
persistence snapshot work
```

The user should never perceive the optimization queue as input latency.

---

# 54. Global Optimization Principle

Every implementation should repeatedly ask:

> Can the exact same player-visible result be represented with less computation?

Examples:

```text
100 objects → one batch
1,000 decals → one baked surface
60 updates/s → 5 updates/s + interpolation
continuous simulation → analytic reconstruction
network particle stream → deterministic seeded event
```

The objective is not fewer effects.

The objective is:

> more apparent world detail per unit of computation.

---

# 55. V1 Scope

The first implementation should focus on:

1. expanding the existing effect system rather than replacing it with isolated systems,
2. performant blood spray,
3. blood collision with surfaces,
4. blood stains/decals,
5. camera-facing PNG/3D sprite support,
6. effect batching,
7. effect visibility culling,
8. continuous importance/LOD logic,
9. performance budgets,
10. queueing expensive background effect work,
11. deterministic effect seeds,
12. server event → client cosmetic reconstruction,
13. instrumentation for effect CPU/GPU/frame-time cost,
14. integration with existing bullet/destruction events,
15. preserving existing desirable blood appearance.

---

# 56. V1 Explicit Non-Goals

V1 does not need to fully implement:

```text
fluid blood
dripping
blood transfer
realistic organ simulation
distributed persistence
permanent cross-restart worlds
complete deterministic whole-server replay compression
full gore destruction
complex chemical/material interactions
```

The architecture must avoid making these future capabilities difficult.

---

# 57. V2+ Direction

Future development may add:

```text
fluid materials
gore
organs
limb destruction
blood flow
surface transfer
fire
heat
smoke physics
advanced debris
persistent server worlds
distributed world storage
deterministic large-scale replay
effect interactions
```

using the same primitives established here.

---

# 58. Design Test

When adding a new visual consequence, ask:

```text
Can existing Effect() capabilities express this?
```

If yes:

```text
use the existing system
```

If not:

```text
add a generalized capability to Effect()
```

Only create a specialized subsystem if it represents a fundamentally different domain that cannot reasonably fit the shared architecture.

---

# 59. Required End Behavior

The finished system should support scenarios such as:

> A player fires a shotgun into another player.

The server resolves projectile collisions and damage.

The client receives authoritative collision events.

Using deterministic seeds and effect configuration, the client generates directional blood spray.

Blood collides with nearby surfaces and generates stains.

Only currently visible/relevant particles are rendered at full detail.

Offscreen cosmetic work consumes effectively no rendering cost.

Static blood becomes progressively cheaper through merging/batching.

The player can leave the area and return much later and see the resulting stains.

If the same area receives thousands of impacts, logical history may be compressed into equivalent current world/surface state rather than thousands of expensive independent objects.

Gameplay remains responsive and frame-time stable throughout.

---

# 60. North-Star Requirement

The world should feel increasingly persistent, physical, reactive, and detailed without computational cost scaling linearly with the amount of history in that world.

The ideal relationship is:

```text
world complexity ↑↑↑↑
observable richness ↑↑↑↑
ongoing compute cost ↑ only when observation actually requires it
```

The engine should continuously collapse invisible, redundant, static, distant, or reconstructible information into cheaper representations.

The world may remember enormous amounts of history.

The renderer and simulator should only pay for the tiny fraction of that history that matters right now.

---

# 61. Core Rules Summary

1. Use one generalized effects architecture.
2. Effects originate from generalized events such as collisions.
3. Server owns authoritative physical/gameplay truth.
4. Client owns cosmetic reconstruction.
5. Use deterministic seeds whenever practical.
6. Never network reconstructible cosmetic information unnecessarily.
7. Persistent does not mean permanently active.
8. Static state should approach zero CPU cost.
9. Do not render invisible information.
10. Do not simulate unnecessary detail.
11. Use continuous importance-based LOD.
12. Scale network frequency with relevance.
13. Use generalized materials.
14. Batch compatible effects.
15. Merge equivalent persistent state.
16. Queue nonessential work rather than causing frame spikes.
17. Log queueing of supposedly immediate effects as an optimization problem.
18. Respect a universal CPU/GPU/frame-time budget.
19. Dynamically adapt quality to measured machine performance.
20. Preserve the intended visible behavior while continually reducing its computational cost.
21. Validate manually before encoding automated regressions.
22. Prefer generalized capabilities over feature-specific implementations.
23. World truth, simulation, representation, and rendering are separate.
24. Physical consequences should persist even when their expensive representation does not.
25. The desired end state is effectively unlimited persistent visual/world complexity with cost proportional to what currently matters.