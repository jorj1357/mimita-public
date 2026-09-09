9 8 2026 1010 est jorj - todo explain ragdoll like retrograd, relates to countnee strike and  source mode style things 

a/d = grab with left, right arm

grab means , its a single point in the world that , a invisible capsule surrounds ur  limbs and that is the movement/world collision hitbox, not the damage hitbox, damage hitbox needs to actual intersect with ur body

but there is a single point on that capasule that will hit the world, and u grab with A/D, and now ur other bod parts in ragdoll m ode are phsics objects, todo relate it to  C:\mimita-priv-v8\docs\specs\moving-physical-objects\moving-physical-objects.md 

left/right click = extend ur arm
with a  factor of speed , like, how fast u can do it how fast ur arms go 

ragdoll body mass editable as well , in ajson 

# MiMITA Ragdoll Mode v1 Specification

**Created:** 2026-09-08 10:10 EST  
**Expanded:** 2026-09-09  
**Status:** Behavioral specification / implementation target

Related:

- `C:\mimita-priv-v8\docs\specs\moving-physical-objects\moving-physical-objects.md`
- `C:\mimita-priv-v8\config\ragdoll.json`
- `C:\mimita-priv-v8\config\aimbody.json`

---

# 1. Goal

Ragdoll mode turns the player from a normal movement-controlled character into a **player-controlled articulated physics object**.

The player does not move by normal WASD locomotion while ragdolled.

Instead, motion emerges from:

- gravity,
- momentum,
- body mass,
- body collisions,
- head movement,
- arm extension,
- grabbing,
- pushing against surfaces,
- interactions with moving physical objects,
- interactions with other players,
- constraints between body parts.

The system should intentionally allow fun, unusual, and sometimes physically exaggerated behavior as an emergent result of generalized physics rules.

Ragdoll should not be implemented as a large collection of special-case animations or movement states.

It should reuse the generalized physical-object system wherever possible.

---

# 2. Relationship to moving physical objects

Ragdoll physics should depend heavily on:

`docs/specs/moving-physical-objects/moving-physical-objects.md`

A ragdolled player is conceptually a collection of moving physical objects connected together.

The generalized physical-object system should provide the underlying behavior for:

- mass,
- velocity,
- momentum,
- gravity,
- collision,
- forces,
- impulses,
- constraints,
- moving-object interaction,
- attachment to other objects,
- network authority,
- interpolation,
- collision resolution.

Ragdoll should extend those primitives rather than implement alternate physics behavior.

Example:

```text
physical object system
        ↓
mass / forces / collision / constraints
        ↓
ragdoll body-part entities
        ↓
player-controlled arm and head targets
```

Changes to physical-object behavior should therefore naturally affect ragdolls unless intentionally overridden in `ragdoll.json`.

---

# 3. Entering and exiting ragdoll

## 3.1 Toggle

Default input:

```text
G = toggle ragdoll
```

Press once:

```text
normal movement
→ ragdoll mode
```

Press again:

```text
ragdoll mode
→ normal movement
```

---

# 4. Player authority

`plrOrigin` remains the authoritative player root.

It represents the authoritative root position/state associated with the player.

The body exists relative to and reacts around this authoritative root.

Conceptually:

```text
plrOrigin
    ↓
torso
    ↓
head / arms / legs
```

The torso attempts to remain physically associated with `plrOrigin`.

Other player systems that need a canonical player reference should continue to use `plrOrigin` rather than selecting an arbitrary limb.

Examples include:

- player ownership,
- player identity,
- respawn,
- networking authority,
- replay identity,
- spectating,
- game-mode logic,
- player-position queries that require a canonical root.

The visible/physical body is allowed to move, rotate, stretch, sway, and collide around that authoritative structure.

---

# 5. v1 body structure

v1 intentionally uses a simple Minecraft-style body.

Body parts:

```text
head
torso
left arm
right arm
left leg
right leg
```

No elbow, knee, forearm, upper-arm, shin, hand, or foot segmentation is required in v1.

Future versions may expand the body hierarchy.

Conceptually:

```text
             HEAD

LEFT ARM — TORSO — RIGHT ARM
             |
       LEFT LEG RIGHT LEG
```

The torso is the central body part.

Head, arms, and legs are attached relative to the torso/player root.

---

# 6. Physics-driven body

While ragdolled, all body parts are physics-driven.

They respond to:

- gravity,
- linear velocity,
- angular velocity,
- collisions,
- forces,
- impulses,
- body constraints,
- grabs,
- movement of connected objects,
- arm extension forces,
- head rotation forces,
- external physical objects,
- other players.

No limb should merely teleport to its target transform.

Desired movement should be represented through forces, constraints, target velocities, target orientations, or equivalent generalized physics behavior.

---

# 7. Body mass

Every body part has independently configurable mass.

At minimum:

```text
head mass
torso mass
left arm mass
right arm mass
left leg mass
right leg mass
```

A global body-mass multiplier may also exist.

Configuration belongs in:

`C:\mimita-priv-v8\config\ragdoll.json`

Example structure:

```json
{
  "mass": {
    "globalMultiplier": 1.0,
    "headKg": 1.0,
    "torsoKg": 1.0,
    "leftArmKg": 1.0,
    "rightArmKg": 1.0,
    "leftLegKg": 1.0,
    "rightLegKg": 1.0
  }
}
```

Values above are illustrative only.

The actual defaults should be tuned through playtesting.

---

# 8. Collision representation

Each body part has at least two conceptually separate collision representations.

## 8.1 Physical/world collider

Used for:

- touching the world,
- preventing penetration,
- physical interactions,
- pushing,
- body-on-body collision,
- grabbing proximity,
- self-collision.

Current desired shape:

```text
capsule
```

There should be a capsule around each:

```text
head
torso
left arm
right arm
left leg
right leg
```

These can be somewhat larger/more forgiving than the visible body.

---

# 9. Damage hurtboxes

Damage detection must remain separate from movement/world collision.

A bullet passing through only the outer movement capsule should not automatically count as a hit.

Damage should require intersection with the actual damage volume associated with that visible body part.

Therefore:

```text
PHYSICS CAPSULE
= movement/world interaction

DAMAGE HURTBOX
= actual combat hit detection
```

The damage hurtbox should align much more closely with the actual visible body.

This separation applies to:

- bullets,
- melee,
- other direct-hit damage systems.

Explosion logic may continue to follow its own generalized explosion specification.

---

# 10. Self-collision

All body parts collide with all other body parts.

Example:

```text
left arm ↔ torso
left arm ↔ right arm
left arm ↔ head
left arm ↔ legs
right leg ↔ left leg
head ↔ torso
etc.
```

The intended behavior is that the ragdoll body cannot simply fold through itself.

The implementation must avoid unstable solver behavior while maintaining this rule.

If stability requires additional generalized solver handling, that should be solved in the physical-object/constraint system rather than silently disabling self-collision.

---

# 11. World collision

No ragdoll body part may freely tunnel through solid world geometry.

Continuous collision detection, sweeps, substeps, or another appropriate generalized collision method should be used where needed.

Fast-moving body parts must not simply skip through walls or floors.

This should follow the same generalized anti-tunneling rules used by moving physical objects.

---

# 12. Normal movement disabled

While ragdolled:

```text
WASD locomotion is not the normal character controller.
```

In particular:

```text
A = left-arm grab control
D = right-arm grab control
```

Therefore `A` and `D` no longer strafe the player.

The body moves only through physical interactions.

Legs do not directly provide player locomotion in v1.

---

# 13. Passive body behavior

The following body parts are primarily passive in v1:

```text
torso
left leg
right leg
```

They:

- fall under gravity,
- preserve momentum,
- collide,
- swing,
- rotate,
- react to connected-body forces,
- react to grabs,
- react to arm pushes,
- react to head movement,
- react to external forces.

Legs simply dangle and participate physically.

There is no active leg-control mechanic in v1.

---

# 14. Head control

The head is actively influenced by player aim.

Mouse/camera look establishes the desired head orientation.

The head should not simply teleport to that orientation.

Instead, the head physically attempts to rotate toward the desired orientation.

That motion should propagate through the body.

Example:

```text
left hand attached to wall
player looks hard to the right

→ head rotates right
→ neck/body attachment transmits force
→ torso rotates
→ attached limbs react
→ hanging arm reacts
→ entire player's momentum can change
```

Everything should influence everything else through the generalized physics system.

---

# 15. Camera

While ragdolled, the camera follows the physical head.

The camera therefore naturally experiences:

- swinging,
- body rotation,
- impacts,
- falling,
- grabbing,
- pulling,
- collisions,
- physical head motion.

However, raw physics jitter should not directly create an unreadable camera.

The existing smooth body/aim behavior defined around:

`C:\mimita-priv-v8\config\aimbody.json`

should be reused where appropriate.

Conceptually:

```text
player mouse input
    ↓
desired head orientation
    ↓
physical head movement
    ↓
smoothed visual camera transform
```

The body is physical.

The camera presentation may be smoothed.

---

# 16. Arm extension controls

Default controls while ragdolled:

```text
LMB held = actively extend left arm
RMB held = actively extend right arm
```

Releasing the button removes that active extension target.

Then the arm becomes passive again.

Example:

```text
LMB pressed
→ left arm actively reaches

LMB held
→ left arm continuously tries to reach

LMB released
→ left arm goes limp
→ gravity/momentum control it
```

The arm does not remain artificially held out after input is released unless something else physically constrains it.

---

# 17. Extension target

Arm extension is based on camera-forward direction.

It is not fundamentally a crosshair-hit-position system.

Conceptually:

```text
cameraPosition + cameraForward * infinity
```

defines a direction/line in which the arm wants to extend.

The arm then tries to extend along that direction as far as its physical constraints permit.

Therefore:

```text
cameraForward
      ↓
desired arm direction
      ↓
physical force/target
      ↓
arm moves
```

The arm does not teleport directly to the first object under the crosshair.

---

# 18. Arm extension strength

Arm extension strength is physical and hot-reloadable.

Configuration belongs in:

`C:\mimita-priv-v8\config\ragdoll.json`

The configuration should preferably use physically interpretable units wherever practical.

Examples:

```text
Newtons
Newtons per kilogram
meters/second target speed
Newton-meters for torque
```

Exact implementation depends on the generalized physical-object solver.

Conceptually:

```json
{
  "arms": {
    "extensionForceNewtons": 0.0,
    "extensionSpeedMetersPerSecond": 0.0,
    "maxExtensionMeters": 0.0
  }
}
```

Values are placeholders.

---

# 19. Arm extension must interact physically with the world

Extending an arm does not ignore obstacles.

Example:

```text
player near wall
→ extend arm toward wall
→ hand/arm hits wall
→ extension force continues attempting to move forward
→ wall resists
→ equal/opposite reaction affects player body
→ player can push themselves away from wall
```

This is intentional.

Arm extension can therefore become a locomotion mechanism without implementing a special "wall push" action.

The result emerges from:

```text
arm force
+
collision
+
body mass
+
reaction force
```

---

# 20. Arm force influences the entire body

Arm extension should not be a visual-only motor.

If the player applies substantial force through the arm, the rest of the body should react.

For example:

```text
light extension force
→ arm mostly moves

large extension force
→ shoulder/torso/body reacts strongly
```

The exact result should emerge from:

- body-part masses,
- constraint stiffness,
- extension force,
- current velocity,
- external contacts.

---

# 21. Grabbing controls

While ragdolled:

```text
A held = left arm attempts to grab / maintains left-arm grab
D held = right arm attempts to grab / maintains right-arm grab
```

Grab state is independent from active extension state.

Example:

```text
hold LMB
→ extend left arm toward wall

hold A
→ grab nearby wall

release LMB
→ arm no longer actively extends

continue holding A
→ grab remains active
→ player hangs from wall
```

---

# 22. Grab acquisition

A grab occurs at a specific physical point.

The player does not merely become logically marked as "attached to wall."

Instead:

```text
hand/body-part point
↔
target object point
```

becomes constrained.

Preferred acquisition behavior:

1. check the corresponding arm/hand's current collision/contact,
2. identify the first appropriate nearby world/object point,
3. allow a configurable grace radius,
4. create a physical grab constraint at that point.

Initial grace distance:

```text
0.5 meters
```

This must be hot-reloadable.

Example:

```json
{
  "grab": {
    "graceDistanceMeters": 0.5
  }
}
```

---

# 23. Grab grace

The player does not need frame-perfect geometric contact.

If the appropriate point is sufficiently close to the hand/arm endpoint, grabbing may succeed.

Default intended distance:

```text
0.5 m
```

The search may use an appropriate generalized technique such as:

- sphere sweep,
- capsule sweep,
- local proximity query,
- raycast plus tolerance.

The final technique should produce predictable physical behavior rather than forcing a specific implementation prematurely.

---

# 24. Grab persistence

Once acquired, a grab persists as long as the corresponding grab input remains held.

For example:

```text
A held
= left-hand grab remains active

A released
= left-hand grab constraint removed
```

No stamina system exists in v1.

No timer exists.

No grip fatigue exists.

---

# 25. Grab strength

v1 grabs are intentionally arcade-like and effectively unbreakable.

They should not fail because:

- the player is heavy,
- another player pulls them,
- gravity is strong,
- the connected object accelerates,
- the player has been holding too long.

Gameplay interpretation:

```text
grab break force = infinite
```

or whatever solver-safe equivalent produces the same behavioral result.

Later versions may add:

- break force,
- stamina,
- joint damage,
- dismemberment,
- hand failure.

Not v1.

---

# 26. Grab constraint compliance

Although gameplay considers the grip unbreakable, the constraint may have a small amount of solver compliance/stretch to maintain stability.

Therefore:

```text
unbreakable
≠
mathematically zero movement under every numerical condition
```

A tiny configurable stretch/compliance is allowed.

The purpose is numerical stability and good-feeling physics, not simulated grip weakness.

---

# 27. Grabbing moving objects

Players can grab moving physical objects.

This is a major requirement.

A grab should attach to the target object's local physical point, not just the original world-space coordinate.

Example:

```text
player grabs point P on crate
crate moves
→ point P moves with crate
→ player's hand remains attached to same part of crate
```

Conceptually:

```text
grab target =
{
    entityId,
    localPoint
}
```

rather than merely:

```text
worldPosition
```

when the target is a movable entity.

---

# 28. Grabbing the static world

For static geometry:

```text
grab target =
world-space point
```

Example:

- wall,
- floor,
- ceiling,
- static map geometry.

The grab remains anchored there.

---

# 29. Grabbing other players

Ragdolls may grab other players.

This should use the same generalized entity attachment system as grabbing any other moving physical object.

No separate special-case "grab player" implementation should be required beyond permissions/game rules.

---

# 30. Grabbing a normally moving player

If Player A is ragdolled and grabs Player B while B is in normal movement mode:

```text
Player A becomes physically attached to Player B.
```

Player B must now physically account for Player A's:

- mass,
- momentum,
- constraints,
- other grabs.

Example:

```text
A grabs B
A's other hand grabs wall

→ B attempts to move
→ B is indirectly connected to wall through A
→ B may be unable to move or may be heavily constrained
```

The physical chain should matter.

---

# 31. Player-to-player constraint chains

Constraints should compose.

Example:

```text
wall
 ↑
right hand of Player A
 ↑
Player A
 ↑
left hand of Player A
 ↑
Player B
```

The physical solver should treat the entire connected structure as interacting physics entities.

This enables emergent behavior without bespoke code for each configuration.

---

# 32. Ragdoll players grabbing each other

Two ragdolled players may grab each other.

This is intentionally allowed even when it creates unrealistic or exploitable-looking movement.

Example:

```text
Player A grabs Player B
Player B grabs Player A
```

This may create:

- unusual oscillations,
- boosts,
- rotations,
- upward movement,
- chaotic momentum transfer,
- "jank."

Some of this behavior is intentionally desirable.

The goal is not to immediately eliminate every non-realistic interaction.

Instead, preserve stable but fun emergent physical exploits when they arise from the generalized system.

---

# 33. Intentional fun-jank principle

Ragdoll physics should be physically generalized but not obsessively sanitized.

If a consistent interaction emerges from the common rules and is:

- learnable,
- reproducible,
- fun,
- not catastrophic to networking/performance,

it may intentionally remain.

This especially includes unusual multiplayer physics interactions.

The system should avoid feature-specific hacks whose only purpose is to forbid players from discovering unexpected movement techniques.

---

# 34. Dual-hand grabbing

Both hands may maintain independent grabs simultaneously.

Example:

```text
left hand → wall A
right hand → wall B
```

The body exists between those two constraints.

---

# 35. Overextension between two grabs

If both hands are attached to points farther apart than the normal body's total reach, the grabs do not break.

Instead, the entire body stretches.

The intended visual/physical concept is similar to uniform spatial expansion.

The required separation should be distributed across the body rather than concentrated exclusively into a single arm.

Conceptually:

```text
grab point A                         grab point B
     ●-----------------------------------●

normal body cannot span distance

→ total required stretch distributed
  across player's articulated body
```

Head, torso, arms, legs, and their relative offsets may all participate in the stretched configuration.

The body should not simply:

- release one grab,
- teleport,
- select one limb to stretch infinitely.

---

# 36. Arcade body strength

In v1, body constraints are effectively impossibly strong.

Stretching does not cause:

- damage,
- broken limbs,
- dismemberment,
- blood,
- death.

Future versions may connect extreme strain into:

- body-part damage,
- tearing,
- blood effects,
- dismemberment,
- weakened constraints.

That is explicitly outside v1.

---

# 37. Gravity

Every ragdoll body part is gravity affected.

This includes:

- torso,
- head,
- arms,
- legs.

Holding an arm extended does not disable gravity.

Holding a grab does not disable gravity.

Example:

```text
left hand attached to wall
→ hand constraint holds
→ rest of body continues falling
→ body hangs/swings from attached hand
```

---

# 38. Falling speed and extreme velocity

Ragdolls should not accumulate numerically destructive downward speed forever.

This should preferably be solved in the generalized physical-object system.

Possible generalized controls include:

```text
terminal velocity
linear damping
angular damping
solver-safe maximum velocity
```

These must not merely hide underlying tunneling or collision bugs.

The generalized system should remain stable under large physical speeds.

Exact values must be hot-reloadable.

---

# 39. Momentum preservation

Ragdoll mode preserves actual player momentum.

Entering ragdoll should not arbitrarily zero velocity.

Exiting ragdoll should also preserve velocity.

Example:

```text
player moving at 20 m/s
→ press G
→ ragdoll continues at approximately 20 m/s

ragdoll flying at 15 m/s
→ press G
→ normal player returns while retaining that momentum
```

Any safety corrections should alter velocity only as much as necessary.

---

# 40. Exiting ragdoll

When leaving ragdoll:

1. release ragdoll-only grabs,
2. restore normal player movement state,
3. return player orientation upright,
4. preserve current velocity,
5. perform a small upward recovery hop,
6. ensure the normal collision body does not spawn buried in the floor.

The transition should appear immediate rather than requiring a long stand-up animation.

---

# 41. Exit recovery hop

The recovery hop exists primarily to solve bad physical placement.

Example:

```text
ragdoll torso partly pressed into floor
→ G
→ normal player controller activates
→ small upward displacement/impulse provides clearance
```

Its strength must be hot-reloadable.

Example:

```json
{
  "exit": {
    "hopVelocityMetersPerSecond": 0.0,
    "preserveVelocity": true
  }
}
```

Value remains to be tuned.

A generalized safe-capsule placement test may supplement the hop.

---

# 42. Weapons while ragdolled

Players retain normal weapon access while ragdolled.

Example:

```text
1 = equip revolver
```

Weapons should continue using their normal generalized weapon/networking systems.

Ragdoll is not a separate weapon implementation.

---

# 43. Overlapping weapon and arm inputs

LMB has two possible meanings while ragdolled:

```text
left-arm extension
weapon primary fire
```

These are intentionally allowed to occur simultaneously.

Example:

```text
revolver equipped
LMB pressed

→ revolver fires
AND
→ left arm attempts to extend
```

This must be configurable.

Initial setting:

```json
{
  "arms": {
    "extendWhileWeaponEquipped": true
  }
}
```

Default:

```text
true
```

---

# 44. Weapon recoil must affect ragdoll physics

Weapon impulses should naturally affect the ragdoll.

For example:

```text
revolver fires
→ weapon recoil impulse
→ attached arm reacts
→ torso reacts
→ hanging/swinging body reacts
```

The result should emerge through the same mass/constraint system.

Future powerful weapons could therefore become intentional ragdoll propulsion tools without needing dedicated movement code.

---

# 45. Head aiming while armed

Weapon aiming should remain associated with the current player aim/camera-forward direction.

Because the camera is physically associated with the head, ragdoll orientation can influence actual shooting behavior.

The system should avoid silently pretending that the player's body is upright when it is not.

---

# 46. Hot-reload requirements

Ragdoll tuning belongs primarily in:

`C:\mimita-priv-v8\config\ragdoll.json`

The configuration must be hot-reloadable according to MiMITA's standard configuration behavior.

Values that should eventually be configurable include at least:

```text
grab grace distance
grab compliance
grab strength/break behavior
arm extension force
arm extension speed
arm maximum reach
arm behavior while weapons equipped
head target strength
head rotational speed
body-part masses
global mass multiplier
joint/constraint strength
body stretch behavior
gravity scale
linear damping
angular damping
maximum fall velocity
exit hop velocity
camera/head smoothing parameters where ragdoll-specific
```

Do not hardcode gameplay-tuning constants when they can reasonably exist in configuration.

---

# 47. Example configuration structure

Illustrative only:

```json
{
  "enabled": true,

  "mass": {
    "globalMultiplier": 1.0,
    "headKg": 5.0,
    "torsoKg": 40.0,
    "leftArmKg": 4.0,
    "rightArmKg": 4.0,
    "leftLegKg": 10.0,
    "rightLegKg": 10.0
  },

  "arms": {
    "extensionForceNewtons": 1000.0,
    "extensionSpeedMetersPerSecond": 10.0,
    "maxExtensionMeters": 1.5,
    "extendWhileWeaponEquipped": true
  },

  "grab": {
    "graceDistanceMeters": 0.5,
    "unbreakable": true,
    "compliance": 0.01
  },

  "head": {
    "rotationStrength": 1.0,
    "rotationSpeed": 1.0
  },

  "physics": {
    "gravityScale": 1.0,
    "maxFallSpeedMetersPerSecond": 50.0,
    "linearDamping": 0.0,
    "angularDamping": 0.0
  },

  "exit": {
    "preserveVelocity": true,
    "hopVelocityMetersPerSecond": 3.0
  }
}
```

These numeric values are examples only and are not authoritative defaults.

---

# 48. Example: hanging from one arm

Initial state:

```text
player enters ragdoll
```

Player looks at wall.

Player holds:

```text
LMB
```

Left arm extends camera-forward.

The arm physically reaches toward the wall.

It collides with the wall.

Player holds:

```text
A
```

The left arm finds a valid grab point within the configured grace distance.

A constraint is created.

Player releases:

```text
LMB
```

The active extension stops.

Player continues holding:

```text
A
```

The left hand remains attached.

Gravity acts on all body parts.

The player's body swings below the attachment.

---

# 49. Example: climbing hand-over-hand

Player hangs from left hand.

They look toward another surface.

Hold:

```text
RMB
```

Right arm physically extends in camera-forward direction.

Right arm approaches target.

Hold:

```text
D
```

Right hand grabs target.

Both hands are now attached.

Release:

```text
A
```

Left hand detaches.

Gravity and momentum cause the body to swing from the right hand.

No dedicated climbing state exists.

Climbing emerges from:

```text
reach
+
grab
+
release
+
gravity
+
momentum
```

---

# 50. Example: pushing off wall

Player is near a wall.

They aim toward it.

Hold:

```text
LMB
```

Left arm extends.

Arm reaches wall before reaching desired extension.

Arm continues producing extension force.

Wall prevents further penetration.

Reaction force travels through the player body.

Player is accelerated away from wall.

Increasing:

```text
extensionForceNewtons
```

should increase the resulting push, subject to the rest of the physical system.

---

# 51. Example: carrying another player

Player A enters ragdoll.

Player A grabs Player B.

If B walks:

```text
B's movement
→ moves B's body/root
→ Player A's grab follows B
→ Player A is dragged/carried
```

Player A's mass must influence the physical interaction.

---

# 52. Example: pinning a moving player

Player A:

```text
left hand grabs Player B
right hand grabs wall
```

This creates:

```text
wall
↔
Player A
↔
Player B
```

Player B cannot simply ignore this physical structure.

Their attempt to move must interact with:

- Player A's mass,
- Player A's body,
- Player A's wall attachment,
- constraint strength.

---

# 53. Example: multiplayer jank

Player A grabs Player B.

Player B grabs Player A.

Both are ragdolled.

They manipulate arms/head/body and create unusual constraint forces.

Possible result:

```text
rotation
oscillation
boosting
launching
upward movement
unexpected momentum transfer
```

If behavior remains stable enough to play with, it is not automatically considered a bug.

Emergent movement techniques are a desired property of this system.

---

# 54. Networking principle

The server remains authoritative over meaningful physical state.

Ragdoll networking should follow the networking and physical-object architecture rather than trusting arbitrary client transforms.

The client should be able to immediately predict locally important actions such as:

```text
ragdoll toggle
arm extension input
grab request
grab release
head input
```

while the authoritative simulation resolves:

```text
physical positions
velocities
constraints
grabs
collisions
inter-player interactions
```

Exact packet design belongs in the networking specification.

---

# 55. Inputs should be networked as intent where possible

Prefer sending compact player intent rather than constantly trusting complete client-produced ragdoll transforms.

Conceptually:

```text
ragdollEnabled = true
leftArmExtend = true
rightArmExtend = false
leftGrab = true
rightGrab = false
aimDirection = ...
```

The authoritative simulation can then determine physical results.

This preserves:

- server authority,
- deterministic-ish interaction,
- anti-cheat boundaries,
- multiplayer consistency.

---

# 56. Replay behavior

Replay capture must preserve enough data to accurately reproduce ragdoll interaction.

That includes, as required by the replay architecture:

- ragdoll state changes,
- body transforms/state,
- grabs,
- grab targets,
- physical-object relationships,
- weapon usage,
- player aim,
- resulting physics.

A replay should not merely show the player's root moving while body physics are missing.

---

# 57. No special-case locomotion

Do not implement separate commands like:

```text
wallJumpWhileRagdolled()
climbLedge()
pullPlayer()
swing()
pushOffWall()
carryObject()
```

when these behaviors can emerge from the common physical rules.

Preferred:

```text
forces
+
contacts
+
constraints
+
mass
+
player input
=
behavior
```

The goal is to create primitives capable of producing many interactions.

---

# 58. v1 non-goals

The following are not required for v1:

- elbow joints,
- knee joints,
- separate hands,
- separate feet,
- detailed human skeletal anatomy,
- stamina,
- finite grip strength,
- broken bones,
- limb damage from stretching,
- dismemberment,
- blood from joint strain,
- realistic muscle simulation,
- stand-up animation,
- IK animation system,
- perfectly realistic biomechanics.

These may be added later without changing the fundamental generalized architecture.

---

# 59. Main v1 invariants

The implementation should satisfy these behavioral invariants.

### RAG-001
Pressing `G` while normally moving enters ragdoll mode.

### RAG-002
Pressing `G` while ragdolled exits ragdoll mode.

### RAG-003
Ragdoll entry preserves current velocity.

### RAG-004
Ragdoll exit preserves current velocity.

### RAG-005
Exit attempts to place the player upright and out of the ground.

### RAG-006
Each v1 body part is physically simulated.

### RAG-007
Each body part has configurable mass.

### RAG-008
Every body part has a world/physics collider.

### RAG-009
Damage hitboxes are separate from forgiving physics capsules.

### RAG-010
Body parts collide with each other.

### RAG-011
Body parts cannot freely tunnel through solid world geometry.

### RAG-012
`A/D` no longer perform normal strafing while ragdolled.

### RAG-013
Holding LMB actively extends the left arm.

### RAG-014
Holding RMB actively extends the right arm.

### RAG-015
Releasing the corresponding mouse button removes that arm's active extension.

### RAG-016
Arm extension follows camera-forward direction.

### RAG-017
Arm extension obeys physical collisions.

### RAG-018
An arm pushing into a wall can push the player's body away.

### RAG-019
Arm extension strength is configurable.

### RAG-020
Holding `A` maintains a left-arm grab.

### RAG-021
Holding `D` maintains a right-arm grab.

### RAG-022
Releasing the corresponding grab key releases the grab.

### RAG-023
Grab acquisition has configurable positional grace.

### RAG-024
Default intended grab grace is approximately 0.5 meters.

### RAG-025
v1 grabs do not break from stamina or force.

### RAG-026
Static world points can be grabbed.

### RAG-027
Moving physical objects can be grabbed.

### RAG-028
Grabbed points on moving entities move with those entities.

### RAG-029
Other players can be grabbed.

### RAG-030
Physical chains involving multiple players and world objects affect every connected entity.

### RAG-031
Both hands can maintain separate grabs simultaneously.

### RAG-032
Grabs do not automatically release because the body cannot normally span the distance.

### RAG-033
Extreme dual-grab separation stretches the overall body instead.

### RAG-034
Body stretching does not cause damage in v1.

### RAG-035
Mouse aim physically influences the head.

### RAG-036
Head movement can physically influence torso and overall momentum.

### RAG-037
Camera follows the physical head with appropriate smoothing.

### RAG-038
Weapons remain usable while ragdolled.

### RAG-039
Weapon firing and arm extension may happen simultaneously.

### RAG-040
Weapon recoil should physically affect the ragdoll.

### RAG-041
Legs are passive physics objects in v1.

### RAG-042
Torso motion emerges from overall physics rather than direct locomotion commands.

### RAG-043
Ragdoll tuning lives in hot-reloadable configuration where practical.

### RAG-044
Ragdoll should reuse moving-physical-object primitives rather than duplicate them.

### RAG-045
Stable, reproducible, fun physics jank is allowed to become gameplay.

---

# 60. Architectural priority

Before deeply implementing ragdoll-specific behavior, verify that the generalized moving-physical-object specification clearly defines the primitives ragdoll depends on.

Especially:

```text
mass
force
impulse
linear velocity
angular velocity
gravity
terminal velocity
collision
continuous collision
self-collision
constraints
constraint compliance
entity-local attachment points
moving-object grabbing
constraint chains
server authority
client prediction
network interpolation
high-force solver stability
```

If any of these are undefined, prefer strengthening the generalized system first.

Ragdoll should then become a relatively small composition layer:

```text
physical objects
+
body hierarchy
+
player input
+
constraints
=
ragdoll
```

rather than a second physics engine hidden inside the player code.