2026 10 08 1600 jorj todo explain how  actors vs actor collisions work 

2026 10 08 1624 jorjt odo  make  a section:
npc "hybrid" mode means 

C:\mimita-v9\config\aimbody.json

see how here its hybrid mode for players just do that body animation mode for npcs too 

# MiMITA Universal Actor-vs-Actor Collision Specification v1

**Date:** 2026-10-08  
**Status:** Draft v1 / authoritative target behavior pending implementation and runtime proof  
**Scope:** Player, NPC, monster, ragdoll, vehicle, projectile, held-object, and actor-support collision behavior  
**Simulation rate:** Fixed 60 Hz authoritative simulation  
**Primary goal:** Replace fragmented actor collision behavior with one generalized, predictable, configurable, full-body triangle-based collision contract.

## Normative: NPC AimBody hybrid mode and shared triangle actors

“NPC hybrid mode” means the exact existing player AimBody hybrid behavior:
the actor keeps a physical body simulation while springs pull its six body
parts toward the current procedural animation pose. NPCs do not receive a
separate body-animation implementation. They use the same hot-reloadable
`config/aimbody.json` values, six-limb ordering, right-arm pointing motor, and
weapon attachment rules as players.

NPCs have no literal camera, keyboard, or RMB state. Their equivalent aim
input is the current weapon target direction when a target exists, otherwise
the current weapon/facing direction. The authoritative transform order is:

```text
NPC animation/procedural pose
→ NPC weapon/target aim
→ AimBody hybrid physical solve
→ final body-part transforms
→ triangle collision meshes
→ rendering and replication
```

NPCs are server-owned. The server generates their hybrid pose and authoritative
collision state. Player clients may predict and submit their own hybrid pose
claims, but the server validates those claims. Ragdolls remain physics-owned;
they are not converted to AimBody hybrid mode and contribute their current
physical limb triangles to the shared actor solver.

Players, NPCs, and ragdolls use one fixed-60-Hz actor collision pipeline. Full
triangle detection aggregates contacts into a stable body-level manifold; a
triangle pair does not receive an independent impulse. `config/aimbody.json`
owns body-animation hybrid tuning. Existing collision configuration owns
global collision response and solver limits. Actor presets own per-actor mass,
friction, restitution, and collision-participation overrides.

The required collision path is:

```text
actor pair
→ swept triangle candidates
→ triangle contacts
→ merged contact manifold
→ bounded depenetration
→ mass-based response
→ support selection
→ final actor state
```

The server must not claim authoritative triangle collision until it can
construct or validate the same body triangles represented by the replicated
pose. Every migration fallback must be explicit in the canonical structured
event stream; no actor may silently fall back to a legacy collision path.

---

# 1. End goal

MiMITA actors should physically exist in the world.

They should not behave as:

```text
root points
capsules with cosmetic bodies
nonphysical visual meshes
```

when interacting with other actors.

The target system is:

```text
animated body triangles
+
held-object triangles
+
shared triangle collision
+
mass/momentum response
+
support/carry behavior
+
network prediction
+
server authority
```

Every physical actor should use the same conceptual collision system.

---

# 2. Fundamental physical model

An actor consists of physical geometry.

That geometry may include:

```text
head
torso
left arm
right arm
left leg
right leg
weapon
tool
item
armor
future arbitrary body parts
```

If those shapes visibly occupy space, they should eventually participate in collision.

Example:

```text
player holding revolver
        ↓
revolver physically extends forward
        ↓
revolver touches another actor
        ↓
collision exists
```

The weapon should not visually pass through the other actor simply because only the actor root has collision.

---

# 3. Universal collision principle

The same collision contract should apply to:

```text
human player
NPC
monster
boss
ragdoll
downed player
flying actor
teleporting actor
vehicle
projectile
held object
```

with configuration determining behavior differences.

The major exception is:

```text
ghost / spectator
```

which may be non-solid relative to normal actors.

---

# 4. Current implementation problem

Current source evidence indicates multiple collision ownership paths exist.

Existing behavior includes:

- local actor-vs-world triangle collision;
- legacy NPC collision;
- root capsule actor-vs-actor collision;
- non-authoritative/server no-op cases;
- physical entity support logic not universally applied to actors.

The target is to remove this conceptual fragmentation.

The final architecture should answer:

```text
What shape represents an actor?
Who detects actor-vs-actor collision?
Who resolves it?
Who owns mass?
Who owns support?
Who owns momentum transfer?
Who predicts it?
Who authoritatively confirms it?
```

with one coherent answer.

---

# 5. Collision shape

The desired actor collision representation is:

```text
FULL ANIMATED BODY TRIANGLES
```

not:

```text
single root capsule
```

for actor-vs-actor physical collision.

---

# 6. Full-body collision

Each animated body part contributes physical triangles.

Examples:

```text
head triangles
torso triangles
arm triangles
leg triangles
weapon/tool triangles
```

All may participate in contact detection.

---

# 7. Animated geometry

Collision geometry should follow actual animated body transforms.

Conceptually:

```text
bind mesh triangles
        ↓
skeleton/body transform
        ↓
world-space actor triangles
        ↓
collision queries
```

If the arm visibly moves:

```text
collision arm moves
```

If the weapon visibly swings:

```text
weapon collision moves
```

---

# 8. Visible geometry should match physical geometry

The broad target is:

```text
what I see
≈
what physically exists
```

This applies to actor-vs-actor collision.

Avoid:

```text
visual arm here
physical capsule somewhere else
```

whenever practical.

---

# 9. Triangle ownership

The actor collision system should consume body geometry through a generalized actor-shape provider.

Conceptually:

```text
Actor
    ↓
ActorCollisionShapeProvider
    ↓
world-space triangles
```

Exact class/function names should follow existing repo architecture.

---

# 10. No player/NPC split

Players and NPCs MUST use the same actor collision solver.

Do NOT maintain:

```text
PlayerActorCollision()
NpcActorCollision()
```

with different physical behavior.

Both should produce:

```text
ActorCollisionInput
```

and receive:

```text
ActorCollisionResult
```

through the same owner.

---

# 11. Collision response feel

Two actors touching should generally feel:

```text
solid
slightly bouncy
pushable
smooth
```

not:

```text
sticky
snaggy
immovable
explosive
```

---

# 12. Equal head-on collision

If two equal actors move directly toward each other at equal speed:

```text
Actor A → ← Actor B
```

then, with bounce enabled:

```text
contact
→ both transfer momentum
→ both rebound slightly
→ if movement input continues:
   they approach again
→ collide again
→ rebound again
```

This repeated bounce is acceptable and intentional.

---

# 13. Bounce configurability

Bounce must be configurable.

Example:

```json
{
  "restitution": 0.15
}
```

Possible values:

```text
0.0 = no bounce
small value = soft body rebound
1.0 = highly elastic
```

Default should be relatively small.

---

# 14. No-bounce mode

If restitution is:

```text
0
```

then equal opposing movement should not bounce.

Instead:

```text
momentum cancels/redistributes
+
actors slide/push
```

according to mass and friction.

---

# 15. Momentum transfer

Actor collision should transfer momentum.

If:

```text
Actor A moving fast
Actor B stationary
```

and A hits B:

```text
A loses some velocity
B gains some velocity
```

subject to:

- mass;
- restitution;
- friction;
- response clamps;
- mode config.

---

# 16. Pushability

Equal actors should be able to physically push each other.

A stationary actor must not become an infinite-mass wall unless configured that way.

Example:

```text
A walks into B
B stationary
```

Expected:

```text
A rebounds slightly
+
B moves slightly in impact direction
```

---

# 17. Mass

Every actor should have effective collision mass.

Initial source of mass:

```text
actor preset
```

Example:

```json
{
  "collisionMass": 100
}
```

---

# 18. Body-size contribution

Long-term mass should be derivable from:

```text
body volume
×
density
```

Conceptually:

```text
mass = Σ(part volume × density)
```

V1 does not require physically exact volume integration.

---

# 19. Density

Future actor definitions may expose:

```text
density
```

allowing larger actors to derive mass naturally.

For v1:

```text
explicit collisionMass
```

is sufficient.

---

# 20. Juggernaut example

Example:

```text
Fighter:
mass = 100

Juggernaut:
mass = 1000
```

If Fighter hits Juggernaut:

```text
fighter loses substantial momentum
juggernaut moves very little
```

---

# 21. Swarm mass

If many actors push one actor:

```text
30 zombies → player
```

their combined momentum should matter.

The target should not require explicitly summing group mass as a special rule.

It should emerge from repeated/parallel physical contacts.

---

# 22. Energy propagation through crowds

If:

```text
A pushes B
B pushes C
C pushes D
```

momentum propagates through the chain.

But response should decay naturally due to:

- mass;
- restitution < 1;
- friction;
- solver damping;
- clamps.

By later bodies in the chain, motion may be minimal.

---

# 23. No perpetual energy creation

Actor contacts must not generate net energy over repeated collisions.

A chain such as:

```text
A ↔ B ↔ C
```

must not gradually accelerate because of numerical error.

The solver should enforce bounded energy behavior.

---

# 24. Friction

Actor contact should support configurable friction.

Friction affects tangential motion along contact surfaces.

---

# 25. Shoulder sliding

If two actors brush shoulder-to-shoulder while moving:

```text
A ↑
B ↑
```

they should:

```text
smoothly slide
```

rather than snag.

---

# 26. No limb snagging

Full triangles introduce a major risk:

```text
arm catches arm
weapon catches shoulder
leg catches leg
```

causing actors to stick unnaturally.

The solver should prioritize smooth sliding for ordinary moving contacts.

---

# 27. Contact manifold simplification

Even though collision detection uses full triangles, response may aggregate nearby contacts into a stable manifold.

Example:

```text
12 tiny triangle contacts
```

should not necessarily produce:

```text
12 independent impulses
```

that explode the actors apart.

Instead, response may derive:

```text
representative normal
penetration depth
contact region
```

for stable resolution.

---

# 28. Rotation

Physical actor body rotation may respond slightly to impact.

Example:

```text
shoulder collision
→ torso/body rotates somewhat
```

This should not automatically rotate the player camera.

---

# 29. Camera independence

Player camera orientation remains controlled by view input.

Physical collision may produce:

```text
small camera flinch
```

if configured.

But collision must not:

```text
violently rotate camera with body
```

unless explicitly desired.

---

# 30. Camera flinch

Optional collision presentation:

```text
impact magnitude
→ small temporary camera response
```

This is cosmetic.

The camera effect must not alter authoritative physical aim unless the mode explicitly wants that.

---

# 31. Actor collision config

A generalized config should exist.

Potential location:

```text
config/actor-collision.json
```

or existing equivalent architecture.

Exact path should follow repo ownership.

---

# 32. Example global collision config

Conceptually:

```json
{
  "enabled": true,
  "triangleActorCollision": true,
  "defaultMass": 100.0,
  "defaultRestitution": 0.1,
  "defaultFriction": 0.4,
  "pushScale": 1.0,
  "impactDamageEnabled": false,
  "impactDamageScale": 0.02,
  "maxCollisionImpulse": 500.0,
  "maxSeparationSpeed": 50.0,
  "supportEnabled": true,
  "supportVelocityInheritance": true
}
```

Exact schema should be derived from implementation needs.

---

# 33. Preset overrides

Actor presets may override collision values.

Example:

```json
{
  "collision": {
    "mass": 1000,
    "restitution": 0.05,
    "friction": 0.8
  }
}
```

---

# 34. Mode overrides

Gamemodes may apply collision policy overrides.

Examples:

```text
Juggernaut:
heavy mass differences

Bomb Tag:
more bounce

Zombie Tower:
crowd stability emphasis

Competitive Duels:
lower push force
```

Avoid duplicating the complete physics configuration inside every mode.

Use overrides/reference presets.

---

# 35. Support / standing on actors

Actors may stand on other actors.

Examples:

```text
player standing on player head
zombie standing on zombie shoulder
player standing on ragdoll torso
```

This is legal.

---

# 36. Support identity

When an actor is grounded on another actor:

```text
supportActorId
supportBodyPart
contact point
support velocity
```

should be known to the solver.

---

# 37. Moving support

If lower actor moves:

```text
support velocity
```

is inherited by supported actor.

Example:

```text
support actor = +20 m/s X
supported actor local movement = 0
```

Expected world velocity:

```text
+20 m/s X
```

---

# 38. Additive support velocity

If supported actor also moves:

```text
support = +20 m/s X
local player movement = +20 m/s X
```

then target world movement is approximately:

```text
+40 m/s X
```

subject to normal movement physics.

---

# 39. Support jump

If the bottom actor jumps:

```text
support gains upward velocity
```

Actors stably supported on it inherit relevant support movement.

This can create:

```text
stack jumps
```

naturally.

---

# 40. Actor staircases

It should be possible for actors to stack.

Example:

```text
30 zombies
```

may physically form an approximate staircase/pile if the geometry permits it.

This is acceptable and desirable.

---

# 41. Support death

If support actor dies:

```text
alive actor
→ ragdoll
```

The supported actor is no longer supported by a stable active body unless a ragdoll body part remains physically underneath.

Then gravity acts naturally.

---

# 42. Standing on ragdolls

Ragdolls remain physically collidable.

A player may stand on:

```text
ragdoll torso
ragdoll head
ragdoll limbs
```

and should be slightly elevated accordingly.

---

# 43. Ragdoll pile

Multiple ragdolls may physically stack.

This may create:

```text
piles
ramps
temporary obstacles
```

This is acceptable.

---

# 44. Support sideways impulse

If a support actor is knocked sideways:

```text
supported actors inherit support motion
```

while maintaining their own relative velocity.

They should not remain magically floating at the old world position.

---

# 45. High-speed impact

Actor collision must support extreme MiMITA speeds.

Examples:

```text
dash
rocket jump
ragdoll launch
fall
vehicle impact
100+ m/s movement
```

---

# 46. High-speed collision response

High-speed collision may produce:

```text
large knockback
```

proportional to relative impact velocity and mass.

---

# 47. Relative velocity

Collision response should consider:

```text
vRelative = vA_contact - vB_contact
```

not merely root translational velocity.

This matters because an arm/weapon may be moving quickly relative to the actor root.

---

# 48. Limb velocity

For animated/rotating body parts:

```text
contact-point velocity
```

should eventually include:

```text
root linear velocity
+
angular/body transform motion
+
animation-driven motion
```

where practical.

---

# 49. Impact damage

Impact damage should be optional.

Default initial test:

```text
very small damage
```

such as:

```text
1–2 damage
```

for meaningful high-speed collision.

Exact formula remains tunable.

---

# 50. Impact damage formula

Conceptual:

```text
impactDamage =
max(0, relativeNormalSpeed - threshold)
× damageScale
```

then:

```text
clamp
```

to avoid ridiculous damage.

---

# 51. Impact damage disabled mode

Many modes may use:

```text
impactDamageEnabled = false
```

while still retaining knockback.

---

# 52. Anti-orbit / anti-explosion limits

The solver MUST prevent numerical response from launching actors into absurd speeds unintentionally.

Useful clamps:

```text
max normal impulse
max separation speed
max restitution response
max inherited support correction
```

These should be configurable.

---

# 53. Clamp philosophy

Clamp only pathological numerical responses.

Do NOT artificially cap legitimate movement globally.

MiMITA may intentionally permit:

```text
100 m/s
500 m/s
```

movement.

Collision clamps should limit solver-generated explosion, not player-generated velocity.

---

# 54. High-speed sweep

At high velocity, discrete triangle checks may tunnel.

Actor-vs-actor collision should eventually support:

```text
continuous/swept collision
```

or substep equivalent for fast contacts.

---

# 55. No tunneling through actors

A high-speed dash must not simply pass through an actor because the bodies never overlapped at one discrete tick.

V1 may use:

- swept bounds;
- conservative advancement;
- substeps;

depending on existing collision architecture.

---

# 56. Crowd behavior goal

Crowds should remain physical without becoming immovable solid walls.

Example:

```text
64 fighters
→ doorway
```

should remain chaotic but traversable.

---

# 57. Crowd compression

Small temporary penetration/compression is acceptable under extreme density.

This is preferable to:

```text
explosive jitter
```

or:

```text
permanent deadlock
```

---

# 58. Compression limit

Penetration must remain bounded.

The solver should not permit actors to completely merge into one body.

---

# 59. Crowd priority

In dense situations prioritize approximately:

```text
1. solver stability
2. bounded penetration
3. momentum preservation
4. sliding
5. bounce
6. visual exactness of tiny contacts
```

This is a stability preference, not a separate crowd physics mode.

---

# 60. Doorway behavior

Twenty actors moving through a narrow doorway should:

- collide;
- bounce slightly;
- push;
- slide;
- compress slightly if necessary;
- eventually flow through.

They should not form:

```text
perfect rigid wall
```

unless the geometry genuinely makes passage impossible.

---

# 61. Swarm pushing

If many actors attack one target:

```text
swarm pressure
```

should naturally emerge.

The target may get physically pushed.

Actors behind the front line may push the front actors.

---

# 62. Rag-doll-through-crowd

Ragdoll movement can allow unusual traversal through crowded groups.

The system should not special-case ragdoll to be ghost-like.

A ragdolled actor remains physical.

---

# 63. Contact ordering

Collision resolution must use deterministic/stable ordering where possible.

Do not let:

```text
unordered container iteration
```

determine wildly different physical results every run.

---

# 64. Pair ordering

For each tick, actor pair processing should use a stable ordering based on something such as:

```text
actor lifecycle ID
pair key
```

Exact implementation is up to the solver.

---

# 65. Multiple-contact iterations

Crowd stability may require iterative solving.

Conceptually:

```text
detect contacts
→ solve pair contacts
→ repeat several bounded iterations
```

rather than solving each contact exactly once.

---

# 66. Anti-bounce crowd damping

In extreme multi-contact situations, restitution may be reduced.

Example:

```text
single impact:
normal restitution

10 simultaneous compression contacts:
effective restitution reduced
```

This prevents swarm explosions.

---

# 67. This must be physical, not a special swarm script

Do NOT implement:

```text
if actorCount > 20:
    disable physics
```

The system should remain one solver with stability rules.

---

# 68. Lifecycle states

Collision behavior must define each actor lifecycle state.

---

# 69. Alive actor

Default:

```text
solid
full triangle collision
pushable
support-capable
```

---

# 70. Downed actor

Default Zombie Tower-style downed actor:

```text
solid
physical
possibly ragdoll-driven
```

Still collides with normal actors.

---

# 71. Dead actor

Dead actor transitions into physical ragdoll.

It remains collidable.

---

# 72. Ragdoll

Ragdoll:

```text
collides with world
collides with alive actors
collides with other ragdolls
can provide temporary support
```

---

# 73. Spectator / ghost

Spectator is non-solid relative to physical actors.

Target:

```text
ghost ↔ alive = no collision
ghost ↔ NPC = no collision
ghost ↔ monster = no collision
ghost ↔ projectile = no collision
```

---

# 74. Ghost-vs-ghost

Ghosts may collide with other ghosts if desired.

Initial target from current specification:

```text
ghosts may collide with ghosts
```

This should be configurable.

---

# 75. Freshly spawned actor

Freshly spawned actor becomes physical immediately.

There is no automatic long invulnerability/noncollision window unless a mode explicitly configures one.

---

# 76. Teleporting actor

After teleport completes:

```text
actor immediately participates in collision
```

---

# 77. Flying actor

Flying actors are fully physical and collide with actors.

---

# 78. Boss actor

Bosses use the same collision contract.

Their different physical feel comes from:

- mass;
- geometry;
- friction;
- restitution;
- movement.

Not a special boss collision subsystem.

---

# 79. Vehicle

Vehicles should eventually participate through the same generalized physical contact architecture.

Their collision geometry and mass may be dramatically different.

---

# 80. Projectiles

Projectiles physically collide with actors according to their own projectile/collision policy.

Projectile collision is not necessarily solved identically to actor body collision response, but it shares body geometry queries.

---

# 81. Held items

Held physical items may collide with actors.

Example:

```text
revolver
rocket launcher
sword
tool
```

Their triangles follow attachment/bone transforms.

---

# 82. Held-object response

Contact with held objects may have one of several configured behaviors:

```text
physical blocker
damage source
cosmetic-only collision
```

Weapon config determines semantics.

---

# 83. Spawn overlap

Actors MUST NOT remain trapped inside one another after spawning.

---

# 84. Spawn penetration resolution

If a new actor overlaps another actor:

```text
detect penetration
→ choose shortest safe push-out
→ resolve smoothly
```

---

# 85. Push-out priority

Prefer:

```text
minimum translation
```

to a non-overlapping configuration.

---

# 86. Spawn fallback

If normal push-out cannot find a valid result:

```text
fallback to last valid position
```

or:

```text
valid spawn point
```

depending on lifecycle context.

---

# 87. No infinite overlap loop

Spawn overlap resolution must have:

```text
bounded attempts
```

and explicit failure handling.

Do not iterate forever.

---

# 88. World collision interaction

Actor-vs-actor resolution must also respect world collision.

Example:

```text
Actor A pushes Actor B toward wall
```

B must not be resolved through the wall.

---

# 89. Constraint ordering

A conceptual solver order:

```text
integrate desired movement
        ↓
world collision
        ↓
actor collision candidates
        ↓
resolve actor contacts
        ↓
recheck world constraints
        ↓
support resolution
        ↓
final state
```

Exact solver order needs implementation experimentation.

---

# 90. One solver owner

There should be one conceptual collision response owner.

Avoid:

```text
world solver corrects actor
then NPC solver corrects actor differently
then player collision corrects again
then networking corrects again
```

without clear ownership.

---

# 91. Collision contract

Conceptually:

```text
ActorCollisionInput
{
    actorId
    lifecycleId
    shape
    position
    orientation
    velocity
    mass
    friction
    restitution
    movementIntent
    supportState
}
```

Output:

```text
ActorCollisionResult
{
    correctedPosition
    correctedVelocity
    contacts
    supportActor
    appliedImpulses
    penetrationResolved
}
```

Exact structs should follow existing architecture.

---

# 92. Broad phase

Full triangle-vs-triangle cannot naively compare every triangle against every other actor triangle.

Use broad-phase rejection first.

Potential:

```text
actor bounds
AABB/BVH
body-part bounds
```

then narrow-phase triangle collision.

---

# 93. Body-part broad phase

A useful hierarchy:

```text
actor bounds
    ↓
body-part bounds
    ↓
triangle candidate pairs
```

This dramatically reduces pair checks.

---

# 94. Triangle acceleration structure

Body triangles may need:

```text
BVH
AABB tree
spatial hierarchy
```

for scalable narrow-phase.

Reuse existing triangle structures if possible.

---

# 95. Body animation update cost

Do not rebuild expensive structures from scratch for every triangle every tick if transforms can update cheaply.

Potentially:

```text
bind-space BVH
+
transformed bounds
```

or other optimized representation.

Implementation should be profiled.

---

# 96. Actor broad phase

Do not test every actor against every other actor at large scale.

Use:

```text
spatial grid
BVH
broadphase hash
```

or existing physics broad phase.

---

# 97. Candidate pair

Only nearby actors produce candidate pairs.

Example:

```text
64 actors
```

does not necessarily imply:

```text
2016 full triangle pair tests
```

every tick.

---

# 98. Server authority

The server is authoritative for actor-vs-actor collision outcome.

It determines final:

```text
position
velocity
support
collision response
```

---

# 99. Client prediction

The local client should predict actor collision immediately.

Target:

```text
local movement input
+
predicted remote actor state
+
same collision solver
        ↓
predicted collision response
```

---

# 100. Prediction goal

The ideal is:

```text
client prediction
≈
server result
```

such that reconciliation is minimal.

---

# 101. Remote actor prediction

Remote actors contribute predicted collision immediately.

Do not wait for:

```text
later authoritative packet
```

before physically acknowledging an actor visibly touching the player.

---

# 102. Remote state

Prediction may use:

```text
latest confirmed remote state
+
interpolation/extrapolation
```

to estimate contact state.

---

# 103. Server confirmation

Server independently simulates collision using authoritative actor state.

It returns confirmed position/velocity.

---

# 104. Reconciliation

If:

```text
predicted state ≠ server state
```

client smoothly reconciles.

Small error:

```text
smooth correction
```

Large impossible error:

```text
stronger correction / snap if necessary
```

---

# 105. Collision correction telemetry

Networking should expose:

```text
actor collision prediction error
```

separately from general movement correction where possible.

---

# 106. Prediction mismatch means bug signal

Frequent large collision reconciliation is not merely cosmetic.

It is evidence that:

```text
prediction model
≠
server model
```

and should be investigated.

---

# 107. Shared solver code

Client and server should use the same collision solver logic where architecture permits.

Avoid duplicated formulas.

---

# 108. Host remains a client

Host player must use the same prediction/reconciliation architecture as any other player.

Do not special-case local host physics into a different path.

---

# 109. NPC server simulation

NPCs are server-authoritative actors.

They use the same collision solver as human actors.

---

# 110. Non-host fairness

100 ms latency must be explicitly tested.

Example:

```text
Client A: 100 ms
Client B: server/host
```

They collide.

Expected:

- local collision feels immediate;
- server confirms;
- corrections remain bounded;
- neither actor appears to pass fully through the other.

---

# 111. Historical collision question

For high latency, actor-vs-actor contact may need historical state considerations.

This is especially relevant if:

```text
predicted player collided with where remote actor appeared locally
```

but server remote position differs.

Exact historical actor-collision policy remains an implementation/design subproblem.

---

# 112. Default principle for latency

Prefer:

```text
what I see has immediate physical consequence locally
```

while server remains final authority.

This should align with MiMITA's broader:

```text
what I see = what happens
```

goal.

---

# 113. Logging purpose

Logging does NOT define collision behavior.

The specification defines behavior.

Logging proves:

```text
candidate existed
contact existed
response selected
impulse applied
support selected
state changed
server/client disagreed or agreed
```

---

# 114. Canonical logging

Use:

```text
StructuredLogger
```

and:

```text
logs/<date>/<run>/events.jsonl
```

No separate collision text log.

---

# 115. Bounded pair logging

Do not log every actor pair every tick in a 100-actor swarm by default.

Support:

```text
actor ID filters
pair filters
sample intervals
state-change logging
scenario logging
```

---

# 116. Collision candidate event

Possible event:

```text
actor-collision.candidate
```

Fields:

```text
tick
actorA
actorB
lifecycleA
lifecycleB
shapeTypeA
shapeTypeB
positionA
positionB
velocityA
velocityB
```

---

# 117. Contact event

```text
actor-collision.contact
```

Fields:

```text
tick
actorA
actorB
bodyPartA
bodyPartB
triangleA
triangleB
contactPoint
contactNormal
penetrationDepth
relativeVelocity
```

---

# 118. Response event

```text
actor-collision.response
```

Fields:

```text
massA
massB
restitution
friction
normalImpulse
tangentImpulse
pushScale
positionBefore
velocityBefore
positionAfter
velocityAfter
```

for each actor where sampled.

---

# 119. Support event

```text
actor-collision.support-changed
```

Fields:

```text
supportedActor
previousSupport
newSupport
supportPart
supportVelocity
inheritedVelocity
```

---

# 120. Spawn push-out event

```text
actor-collision.spawn-overlap
```

Fields:

```text
actor
overlappingActor
penetration
resolutionVector
fallbackUsed
finalPosition
```

---

# 121. Reconciliation event

```text
actor-collision.reconcile
```

Fields:

```text
clientTick
serverTick
actor
predictedPosition
authoritativePosition
positionError
predictedVelocity
authoritativeVelocity
velocityError
correctionMode
```

---

# 122. Swarm summary

For large tests:

```text
actor-collision.swarm-summary
```

may record:

```text
actor count
pair candidates
active contacts
max penetration
average penetration
max impulse
average correction
solver iterations
solver time
```

---

# 123. Performance logging

Collision profiling should record:

```text
broadphase time
narrowphase time
solver time
pair count
triangle candidate count
contact count
```

for stress scenarios.

---

# 124. Runtime scenario: equal head-on

Setup:

```text
Actor A:
mass 100
velocity +10 X

Actor B:
mass 100
velocity -10 X
```

Expected:

```text
contact
small bounce
approximately symmetrical response
```

With continued input:

```text
repeat contact/rebound
```

---

# 125. Runtime scenario: bounce disabled

Same setup:

```text
restitution = 0
```

Expected:

```text
no elastic rebound
momentum cancels/slides
```

---

# 126. Runtime scenario: one stationary

Setup:

```text
A:
mass 100
velocity +20

B:
mass 100
velocity 0
```

Expected:

```text
A slows/rebounds slightly
B receives forward velocity
```

---

# 127. Runtime scenario: fighter vs Juggernaut

Setup:

```text
fighter mass 100
juggernaut mass 1000
```

Fighter charges Juggernaut.

Expected:

```text
fighter changes significantly
juggernaut moves very little
```

---

# 128. Runtime scenario: Juggernaut pushes fighter

Reverse scenario.

Expected:

```text
fighter strongly displaced
juggernaut slightly affected
```

---

# 129. Runtime scenario: support movement

Setup:

```text
Actor B stands on Actor A
A moves +20 X
B supplies no movement
```

Expected:

```text
B world velocity approximately +20 X
```

---

# 130. Runtime scenario: additive support

Same:

```text
A support velocity = +20 X
B local move velocity = +20 X
```

Expected:

```text
B world target ≈ +40 X
```

subject to movement constraints.

---

# 131. Runtime scenario: support jump

Actors stacked.

Bottom actor jumps.

Expected:

```text
upper actors inherit upward/platform movement
```

without remaining suspended.

---

# 132. Runtime scenario: support death

B stands on A.

A dies/ragdolls.

Expected:

```text
A becomes physical ragdoll
B loses active support
B falls naturally
B may land on ragdoll geometry
```

---

# 133. Runtime scenario: ragdoll pile

Spawn multiple ragdolls.

Expected:

- triangle/body collision;
- stable pile;
- actors can stand on pile;
- no explosive jitter.

---

# 134. Runtime scenario: doorway

20 equal actors attempt to move through doorway.

Expected:

- contacts;
- some bounce;
- some sliding;
- slight compression allowed;
- continued throughput;
- no permanent solver deadlock.

---

# 135. Runtime scenario: 64-actor swarm

64 fighters converge on one target.

Expected:

- target receives physical crowd pressure;
- fighters push/bounce one another;
- no huge explosive launch;
- solver remains stable;
- actors generally continue toward destination.

---

# 136. Runtime scenario: 100-actor swarm

Stress test.

Measure:

```text
simulation tick time
contact count
penetration
solver stability
```

No requirement that visual behavior be perfect initially.

---

# 137. Runtime scenario: high-speed dash

A moves:

```text
100 m/s
```

into B.

Expected:

- no tunneling;
- contact detected;
- bounded large knockback;
- optional small impact damage;
- no accidental 10,000 m/s launch.

---

# 138. Runtime scenario: 500 m/s

Stress/edge test:

```text
500 m/s actor
```

Expected:

- solver either resolves safely;
- or explicit high-speed fallback occurs.

Must not silently tunnel.

---

# 139. Runtime scenario: weapon-first contact

Actor holds revolver forward.

Revolver touches target before body.

Expected:

```text
held-object collision detected
```

according to weapon physical-collision policy.

---

# 140. Runtime scenario: arm sweep

Animated arm swings into actor.

Expected:

```text
arm triangle contact
```

and physical response according to configured behavior.

---

# 141. Runtime scenario: spawn inside actor

Spawn B overlapping A.

Expected:

```text
automatic smooth push-out
```

No permanent overlap.

---

# 142. Runtime scenario: impossible push-out

Actor spawns overlapping:

```text
another actor
+
wall
```

Expected:

```text
attempt safe resolution
→ fallback to valid position if necessary
```

No infinite loop.

---

# 143. Runtime scenario: player against wall

A pushes B into wall.

Expected:

- B cannot move through wall;
- collision response redistributes;
- A cannot tunnel B through wall.

---

# 144. Runtime scenario: ghost

Ghost walks through active actor.

Expected:

```text
no collision
```

---

# 145. Runtime scenario: ghost vs ghost

If enabled:

```text
ghosts collide
```

Otherwise:

```text
no collision
```

according to config.

---

# 146. Runtime scenario: 100 ms non-host

Remote actor collision under simulated latency.

Required evidence:

```text
local predicted contact
server authoritative contact
position error
velocity error
reconciliation amount
```

Human acceptance:

```text
collision feels immediate
no visible full-body pass-through
```

---

# 147. Runtime scenario: NPC vs player

NPC and player collide.

Expected:

```text
same contact/response owner
```

Logs must show the same solver path.

---

# 148. Runtime scenario: NPC vs NPC

Two NPCs collide.

Expected same physics.

---

# 149. Runtime scenario: monster vs Juggernaut

Small Zombie Tower monster collides with heavy Juggernaut.

Expected mass behavior emerges from configuration.

---

# 150. Runtime scenario: flying actor

Flying actor collides in air.

Expected:

- normal actor contact;
- physical knockback;
- no assumption that both actors are grounded.

---

# 151. Runtime scenario: projectile / actor

Projectile contact should query actual body geometry.

Exact projectile response belongs to weapon/projectile spec.

---

# 152. Acceptance metrics

Useful quantitative metrics:

```text
max penetration depth
average penetration
peak solver-created velocity
energy growth
contact jitter frequency
support stability
reconciliation error
solver cost
```

---

# 153. Anti-jitter metric

Repeated contact between stationary/resting actors should eventually settle.

If two actors resting together produce:

```text
velocity sign changes every tick forever
```

at visible magnitude, solver requires damping/stabilization.

---

# 154. Intentional bounce exception

Head-on actors actively pushing each other may intentionally continue bouncing.

Do not confuse:

```text
input-driven repeated collision
```

with:

```text
rest-state solver jitter
```

---

# 155. Resting contact

If neither actor is actively driving into the other and relative velocity approaches zero:

```text
collision should settle
```

rather than micro-bounce forever.

---

# 156. Support stability

An actor standing on another actor should not:

```text
vibrate
rapidly lose/regain grounded state
```

every tick.

Support identity should use hysteresis/stability rules if necessary.

---

# 157. Contact hysteresis

Small numerical changes around contact threshold may preserve:

```text
existing support/contact
```

for a short tolerance.

This prevents flicker.

---

# 158. Full-triangle caution

Full actor triangles are the target.

However, response must be engineered for stability.

The specification does NOT mean:

```text
every triangle pair gets an independent full rigid-body impulse
```

without aggregation.

Detection can be detailed.

Response can be stabilized.

---

# 159. Triangle-triangle contract

Conceptually:

```text
triangle intersection
        ↓
contact candidate
        ↓
manifold/contact aggregation
        ↓
body-level physical response
```

This preserves visible shape fidelity without creating unstable impulse explosions.

---

# 160. Animated body ownership

Animation determines body-part transforms.

Collision consumes those transforms.

Collision may alter root/physical body response.

The system must avoid circular ownership where:

```text
animation forces body through actor
collision corrects
animation forces it back next tick
```

---

# 161. Procedural/hybrid animation interaction

Hybrid AimBody, procedural pose, and animation should produce the final collision pose through one known transform stage.

Collision must consume:

```text
final physical/render-relevant body transforms
```

not stale pre-animation transforms.

---

# 162. Ragdoll ownership

When ragdoll active:

```text
physics owns body-part transforms
```

Collision uses those physical transforms.

---

# 163. Alive actor ownership

When non-ragdolled:

```text
movement/root physics
+
animation/procedural pose
```

define actor body geometry.

---

# 164. Held-object attachment ownership

Held tools derive collision transform from:

```text
actor body attachment
+
tool local transform
```

The same transform should drive visual and collision placement.

---

# 165. Damage is separate from collision

Collision answering:

```text
these two physical shapes touched
```

does NOT automatically mean:

```text
deal combat damage
```

Combat/damage policy decides that.

---

# 166. Impact damage exception

Physics may generate:

```text
impact damage event
```

when mode/config enables it.

This event goes through the shared damage system.

---

# 167. Weapon contact damage

Weapon contact may produce damage according to weapon policy.

Again:

```text
collision detects
weapon/damage system decides damage
```

---

# 168. One contact event pipeline

Preferred conceptual architecture:

```text
Collision
    ↓
PhysicalContactEvent
    ├── physics response
    ├── support evaluation
    ├── optional impact damage
    └── weapon/contact consumers
```

Avoid each subsystem re-detecting the same geometry separately if possible.

---

# 169. Config live editing

Collision tuning should be hot reloadable where safe.

Desired tunable values:

```text
mass
restitution
friction
push scale
max impulse
max separation speed
impact damage scale
support thresholds
penetration tolerance
solver iteration count
crowd restitution damping
```

---

# 170. Live-edit goal

While testing Juggernaut:

```text
change restitution
save
→ actors immediately feel less/more bouncy
```

without rebuild if safe.

---

# 171. Invalid config

If actor collision config becomes invalid:

```text
retain last valid configuration
log error
```

Do not zero physics values accidentally.

---

# 172. Debug rendering

Optional debug visualization should support:

```text
actor triangles
body-part bounds
contact points
contact normals
support contacts
impulse arrows
broad-phase bounds
```

---

# 173. Contact labels

For selected actor pair, debug view may show:

```text
mass A/B
relative velocity
penetration
normal impulse
support state
```

---

# 174. Performance requirement

Full-body triangle collision must be scalable enough for:

```text
Juggernaut swarms
Zombie Tower swarms
large NPC battles
```

It should be profiled early.

---

# 175. Scaling target hierarchy

Test:

```text
2 actors
10 actors
20 actors
64 actors
100 actors
250 actors
```

before claiming large-scale support.

---

# 176. Physics LOD possibility

If necessary, future optimization may reduce collision complexity for distant actors.

Example:

```text
near actors:
full triangles

far actors:
simplified physical representation
```

However, this conflicts somewhat with the universal full-triangle ideal.

Any LOD must be:

- behaviorally reasonable;
- explicitly documented;
- not visible as actors suddenly becoming nonphysical.

Not required for v1.

---

# 177. Server cost priority

The authoritative server should prioritize collision correctness for:

```text
near/active interaction pairs
```

rather than spending equal work on actors far apart.

Broad-phase naturally handles this.

---

# 178. Collision pair lifecycle IDs

Actor IDs alone may be reused across respawn.

Collision logging/prediction should include:

```text
actor ID
+
lifecycle/generation ID
```

to avoid stale contact affecting respawned actor.

---

# 179. Respawn invalidation

When actor dies/respawns:

```text
old contact pairs
old support state
old predicted contact
```

must not leak into new lifecycle.

---

# 180. Network packet generation

Collision state does not necessarily require a special packet for every contact.

The authoritative consequences:

```text
position
velocity
support-relevant state where necessary
```

may already replicate through standard actor state.

Only add explicit contact networking if required.

---

# 181. Support replication

If support identity materially affects prediction:

```text
supportActorId
```

may need replication or derivation.

Investigate whether deterministic derivation is sufficient.

---

# 182. Determinism

Client/server collision must be as deterministic as practical.

Important inputs:

```text
pair ordering
solver iterations
geometry
config
tick state
```

must match.

---

# 183. Floating-point differences

Perfect cross-platform bit determinism may not be required immediately.

But results should remain close enough that reconciliation does not constantly fight physics.

---

# 184. Fixed tick

Actor collision runs in fixed simulation tick:

```text
60 Hz
```

not render frame time.

---

# 185. Render independence

A 30 FPS client and 300 FPS client should simulate equivalent collision behavior.

---

# 186. No frame-rate-dependent bounce

Restitution/impulse must not depend on how many render frames happened during contact.

---

# 187. Shared movement integration

Movement produces desired actor motion.

Collision may modify actual physical velocity/position.

Conceptually:

```text
movement intent
→ movement acceleration
→ candidate physical state
→ collision resolution
→ final physical state
```

---

# 188. Collision is not movement policy

Actor collision must not decide:

```text
NPC should go left
```

or:

```text
player should stop pressing W
```

It resolves physical contact only.

---

# 189. NPC awareness

NPC AI may receive information that:

```text
path blocked by actors
```

from physics/navigation systems.

But collision itself does not choose tactical movement.

---

# 190. RVO/local avoidance relation

Local avoidance may reduce unnecessary actor collisions.

It must not replace physical actor collision.

Architecture:

```text
AI desired route
→ avoidance suggestion
→ movement intent
→ physical actor collision
```

---

# 191. Physical shoving remains possible

Even if RVO avoids collisions, actors may still intentionally/accidentally collide.

The physics must handle it.

---

# 192. Mode-specific examples

## Juggernaut

Collision emphasizes:

```text
mass differences
crowd pushing
fighter swarms
heavy Juggernaut stability
```

---

# 193. Zombie Tower

Collision emphasizes:

```text
large swarms
ragdoll piles
monster body pressure
brute knockback
standing on bodies
```

---

# 194. Duels

Collision emphasizes:

```text
precise physical interaction
predictable push
minimal networking mismatch
```

---

# 195. Payload

Collision emphasizes:

```text
large team crowd flow
doorway congestion
standing on moving actors/cart
```

---

# 196. Vehicles

Future:

```text
very large mass difference
high-speed contact
```

should use same generalized response principles.

---

# 197. Definition of done: component

Component-level completion requires:

- triangle actor shapes produced correctly;
- candidate pair detection;
- triangle narrow phase;
- stable manifold/contact generation;
- impulse response;
- mass differences;
- friction;
- restitution;
- support state.

But this is not full completion.

---

# 198. Definition of done: runtime

Runtime completion requires actual executable scenarios showing:

- player/player;
- player/NPC;
- NPC/NPC;
- ragdoll/alive;
- support;
- crowd;
- high-speed contact.

---

# 199. Definition of done: networking

Network completion requires:

- non-host prediction;
- server confirmation;
- bounded reconciliation;
- same solver path for host/non-host.

---

# 200. Definition of done: human acceptance

Human should be able to say:

```text
"Actors feel physical."

"I can shove someone."

"A Juggernaut feels heavy."

"People bounce a little."

"I can stand on someone's head."

"A crowd feels chaotic but not broken."

"Ragdolls pile up."

"I don't get launched randomly."

"It still feels good at 100 ms."
```

---

# 201. Implementation order

Recommended:

```text
1. Trace all existing actor/world, actor/NPC, actor/actor collision owners

2. Define final shared ActorCollisionInput / shape provider

3. Route players + NPCs through same candidate pipeline

4. Add full animated actor triangle queries

5. Implement stable contact manifold

6. Implement mass-based impulse response

7. Add restitution

8. Add friction/sliding

9. Add support actor identity

10. Add support velocity inheritance

11. Add ragdoll contact/support

12. Add spawn overlap resolution

13. Add crowd stabilization

14. Add high-speed anti-tunneling

15. Integrate server authoritative path

16. Integrate client prediction

17. Add reconciliation diagnostics

18. Run 2-actor scenarios

19. Run Juggernaut mass scenario

20. Run doorway/swarm scenarios

21. Run 100 ms network scenario

22. Remove/reduce legacy actor collision owners
```

---

# 202. Migration principle

Do NOT leave:

```text
legacy capsule actor collision
+
new triangle actor collision
```

both independently resolving production actor physics indefinitely.

During migration, both may temporarily exist for comparison.

Final ownership should be one system.

---

# 203. Fallback rule

A temporary fallback may exist during development.

If so it must be explicit in logs:

```text
collisionBackend = triangle
```

or:

```text
collisionBackend = legacy_fallback
```

Never silently switch.

---

# 204. Legacy removal gate

Legacy actor collision may be removed only after:

- player/player runtime passes;
- NPC/player passes;
- ragdoll passes;
- swarm stability passes;
- networking passes.

---

# 205. Logging gate

Do not start by adding thousands of collision logs.

First define behavior.

Then add just enough bounded logging to prove:

```text
detection
→ contact
→ response
→ support
→ networking
```

---

# 206. Permanent guiding rule

MiMITA actors should physically occupy the world they appear to occupy.

If:

```text
a hand is there
a weapon is there
a body is there
a ragdoll is there
a giant boss is there
```

then another actor should physically interact with it.

The physical response should emerge from:

```text
geometry
mass
velocity
friction
restitution
support
```

rather than hardcoded per-mode exceptions.

---

# 207. Final target model

```text
ACTOR A
    ↓
final animated/physical triangles
    ↓

                    BROAD PHASE
                         ↓
                    NARROW PHASE
                         ↓
                  CONTACT MANIFOLD
                         ↓
                SHARED ACTOR SOLVER
                  ↙      ↓       ↘
             impulse   friction  support
                  \       |       /
                   final physical state
                           ↓
                    authoritative server
                           ↓
                    replicated state
                           ↓
                  client reconciliation

ACTOR B
    ↑
final animated/physical triangles
```

Players, NPCs, monsters, bosses, and ragdolls all participate in this same physical world.

Ghosts are the explicit exception.

---

# 208. Ultimate success condition

The system is successful when something like this happens naturally:

```text
64 Fighters rush 4 Juggernauts.

Fighters collide with one another.
They shoulder past each other.
Some bounce.
Some jump over teammates.
Some ragdoll into the crowd.
Bodies pile up.
Fighters shove Juggernauts slightly.
Juggernauts shove Fighters dramatically.
A Fighter lands on another Fighter's head.
That Fighter gets knocked sideways.
The upper Fighter inherits the movement and falls.
A dead body becomes part of the pile.
Nothing tunnels.
Nothing explodes into infinity.
Nothing uses a separate NPC collision implementation.
A 100 ms client sees almost the same thing the server confirms.
```

And none of this requires:

```text
if mode == juggernaut
```

because it is simply how physical actors in MiMITA work.

### rant 2026 10 08 1613

voic to text answering 

Number one, what should actors physically feel like when they touch? It should be like. If they walk into each other at equal speed, it should be. A little bit of a bounce And like, but you can keep moving. Like if you keep walking forward, like you guys are just going to keep hitting each other and then bouncing off, then hitting each other, then bouncing off. If bounce is even enabled, if there's no bouncing, then it should just be like. I think there should be like momentum transfer. I don't really know how to do that. I do. I want it. So like if you can walk, if you run into somebody fast enough, then you can knock them like away 'cause you'll bounce off of them, but then they'll inherit your velocity too, like as a knockback thing. And that needs to be tunable and like enable or disable able. But I think it should be like you bounce. If both people are walking into each other at equal speed, then you guys are just going to bounce off each other over and over. #2 I want full animated body triangles for actor versus actor, full of it that's it's fully triangles. It's triangle versus triangle and it shouldn't differentiate between anybody else. Head, arms, legs, weapons, torso, limbs, everything. Anything. Like any tool, any item you're holding should also count as collisions. And so if you're holding a revolver and you walk into somebody, then your revolver will hit them. It's not like you walk and then your tool is like. You pass through him, you pass through them. The revolver is like an actual has like a box around it and then that's like what the collisions are doing are treating as a collision. Number three, yeah. If a small zombie runs into a juggernaut, Juggernaut should barely move. If 30 zombies push one player, Yeah. If it's like a bunch of I feel like yeah. It should be like mass. Decay like speed decay. Like if you're pushing one guy into another, then they're going to expel the energy through bouncing. But eventually you're gonna spell so much energy that you can push 1 zombie into another into another into another. By the time you get to like the 4th zombie, they're not gonna move really at all. But if they're all moving towards you. Then you're gonna have like 4 body masses to go against and so you're not really gonna be able to push Equal players should be able to shove each other. Math should come from active preset. And body size, it should eventually be from like actual physics like how much density you have. But for now just like a a config value like juggernaut should definitely be very heavy and not super easy to push. And the normal juggernaut fighters should be easy to push. #4 shoulder to shoulder, just smoothly slide, no snagging. And probably a little bit of a bounce and maybe some rotation, but like your only your body, your camera is not going to rotate. Your body will rotate and you can probably have like a little bit of a camera flinch too. If one player into another who's stationary, they should slide and move. Like if I walk in somebody who is not moving at all, they're just totally stationary. I should be able to move them. They should inherit like I should bounce off of them still, but they should inherit like the velocity that I put into them. So if I'm going like 20 meters a second into somebody with like, I don't know, like 100 density as the default, then they should go like maybe like half a meter in the direction I'm pushing them in or maybe like .1 meters. This should be. Based on like a conflict though, we might need like an actor collision, Jason. Dot JSON #5 Yes, you can stand on another player's head or shoulders. Zombies can pile up 30 actors can form a staircase. If the lower actor moves to the actor standing on top, yes, they should inherit that movement like a moving platform. If the support actor jumps, everybody else will jump. Because like as a result of them being Like inheriting your velocity and position and stuff. If the support actor dies then the next one who's alive will just fall down to the ground and then become grounded. If that support actor is a rag doll then you should be able to stand on like their torso or whatever. But. That's kind of it. And if you get knocked sideways, then if people are standing on top of you and have like a stable connection, like standing on you as a platform, then they should inherit your velocity too, so they'll jump. So if you get knocked sideways as a support, then you get. Then the people on top of you should also inherit your velocity plus whatever velocity there they have. Number six. This should create impact, Yeah, knock back based on impact velocity. Optionally deal damage, but maybe like one or two damage for now. This should absolutely depend on the mode or config and we. Yeah, it should be. To prevent ridiculous bouncing or being launched into orbit, we need to have it. We need a number one have it all super duper editable so we can tweak it live because I don't really know how this would work. #7. The solver prioritize should. It should probably like Probably #1 if 64 people are trying to go through 1 doorway, probably just prioritize. Like you can have a little bit of compression, like you can go into them a little bit if it's like a really dense area, but still absolutely bounce off each other and you can still push each other and you can ragdoll through. Multiple people who are ragdoll should be able to stack on top of each other and you should be able to collide with ragdolls in the world like their limbs. Like if you stand on a rag though, then you'll be a little bit higher up because even if they're dead because you're standing on their head or their limbs and stuff #8. These should all. Dead bodies have the same collisions, Ragged all the same collisions down zombie tower players, same collisions. Spectators are ghosts. You cannot and you can't collide with them at all. Spectators or ghosts can only collide with other ghosts, freshly spawned actors. Instantly can collide. Teleporting actors can instantly collide. Flying can collide, bosses collide, projectiles collide, vehicles all that collide. Everything but a ghost. And an actor should never be trapped inside another actor. They should have like an automatic push out. If it's really that bad, then they can just like have a teleport to like the last valid position, like maybe a spawn point or like wherever they were before that wasn't inside of that actor. But if you spawn inside of somebody then it should like automatically resolve to. Just like smoothly push out. #9. I think we should just reuse the same collisions that we have for like active versus world So I think if that's similar, it should be like predicted, It should pretty much be like that. Like the client does a lot of prediction and then the server just like says OK, that's valid or that's invalid or like that's reasonable. Like I said, reasonable that this collision would happen like that got simulated. Yeah, OK, cool. Then I'll we will allow it and replicate that. Everybody else on the server. Remote actors contributed. Yeah, they contribute predicted collision immediately. Well, yeah, they do predicted collision immediately. But the server should confirm and if the confirmed server state is different than the predicted state, then number one, our prediction is not good, not good enough, but #2 then smoothly reconcile to the. To the authoritative server position and NPCS and players use the exact same collision path number 10/2 equal walkers 2 equal actors walking head on. That should be a bounce over and over and over. 1 stationary one pushing. The pushing actor will move the stationary actor. Light actor versus heavy actor. The light actor can go super fast into the heavy actor, but the heavy actor won't move very much. We can do the juggernaut fighter versus juggernaut. Actually juggernaut. Test for that actor. Standing on a moving actor means if you're standing on the actor and they're going 20 meters a second and you're going 0 meters a second, your total velocity is 20 meters per second. But if you're standing on them and you walk forward, that means you have an additive. Velocity. So they're going to 20 meters a second, you're going 20. That means your total velocity is 40. 20 actors through a doorway. It should be a little bit of compression, but mostly like. It should be like 95% should just be the normal collisions like walking to somebody and it's a little bit of a bounce and it depends on how fast you're going or how fast the limb that even hit the other person is moving and then #10 is 64 actors swarm imagine if they're all trying to attack one person they should probably bounce off each other a little bit and push each other a little bit but generally you can just like kind of make your way to wherever you're trying to go like if somebody's in the way you can kind of just push them out of the way a little bit or like jump over them or ragdoll through them just stuff like that like it won't be like a big big deal it's not like a solid wall high speed dash impact is knocked back to the actor who gets hit and maybe a little bit of a damage after death while supporting another yeah that's good after death means you just turn into a rag doll and so the person you're supporting will just fall naturally onto wherever you are responding into occupied spawn just a nice little push out 100 millisecond non host collision tests I think we can just do that like normally through playing and events Jason L record I don't really know how we could I don't know off the top of my head umm how we could do that but I figured obviously that up to you I figure I'll leave that up to you 
