# Deathball world pass-through suppressed actor damage

Time created: 2026-11-01T01:54:30Z
Time last updated: 2026-11-01T01:54:30Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/weapons/deathball.md`,
`docs/specs/gamemodes/funcounterstrike.md`,
`docs/specs/networking/contact-authority.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_015253-fun-counterstrike-hud-deathball-damage.md`

---

## Regression Occurrence 1

### Observed

Time: human report 2026-10-31; runtime evidence
`2026-11-01T01:09:47Z` (journal `20261101_010944`)

Deathball projectiles appeared and passed through the world but applied no
damage for player -> NPC, NPC -> player, NPC -> NPC, or player -> player.

### Expected Behavior

On every tick that the ball intersects a player or NPC, it applies the
configured center-to-edge damage, outward knockback, and shared hit feedback,
while still decaying faster for that tick when it intersects world geometry.

### Actual Behavior

The server recorded `deathball.spawn` and repeated `deathball.wall-pass` but zero
`deathball.contact`. No damage, hit marker, or damage number was produced.

### Why This Is Bad

The weapon's entire damage contract was inert. The player could see the ball
pass through an actor without any authoritative health change or feedback.

### Specification

`docs/specs/weapons/deathball.md` requires per-tick actor damage (1 at the edge,
10 at the center, interpolated by contact distance), outward knockback at 0.1 per
damage point, repeated damage on later ticks, wall pass-through with faster
lifetime decay, and the shared hit-marker/damage-number/health path.

`docs/specs/networking/contact-authority.md` requires Deathball to use the
shared contact and damage sink.

### Wrong Code

File:

`src/combat/projectile-simulation.cpp`

The candidate ordering gave world geometry priority over actor capsules at equal
time, and the world pass-through branch consumed the step without reporting the
actor:

```cpp
if (c < best) best = c;
...
if (config.passThroughWorld)
{
    state.position = proposedPos;
    result.type = ProjectileCollisionType::WorldPassThrough;
    ...
    remainingFraction = 0.0f;
}
```

File:

`src/network/server-projectiles.cpp`

```cpp
if (step.type == ProjectileCollisionType::WorldPassThrough) {
    // wall decay only
} else if (step.type == ProjectileCollisionType::PlayerPassThrough &&
           stepDef && stepDef->id == "deathball") {
    // damage branch
}
```

Knockback was also always upward:

```cpp
const glm::vec3 targetCenter = victimPos + glm::vec3(0.0f, 0.0f, 0.9f);
glm::vec3 push = targetCenter - victimPos; // always up
```

### Confirmed Cause

The configured Deathball radius is 5 m, so its sweep continuously overlaps the
floor. Every tick the world candidate won at `t=0`, the world pass-through branch
ran, and the actor capsule candidate was never processed. Evidence: journal
`20261101_010944` contains `deathball.spawn` and `deathball.wall-pass`, and zero
`deathball.contact`.

### Attempted Fix 1

Time: `2026-11-01T01:50:57Z`

Change:

- `ProjectileStepResult` now carries `passedThroughWorld` independently of the
  result type.
- When both pass-through flags are set and the world candidate would hide an
  actor, the actor candidate becomes the primary result while
  `passedThroughWorld` remains true for lifetime decay.
- Server and client wall decay use `step.passedThroughWorld`.
- Server knockback uses `victimPos - center`.
- Server contact point uses `nearestServerActorBodyPoint`.
- Deathball damage source is `PhysicalContact`, not `RocketExplosion`.

### Corrected Code

File:

`src/combat/projectile-simulation.cpp`

```cpp
if (worldContactThisStep)
    result.passedThroughWorld = true;
const bool preferPassThroughActor =
    config.passThroughWorld && config.passThroughActors &&
    best.type == 0 && bestCapsule.t <= 1.0f;
if (preferPassThroughActor)
{
    best = bestCapsule;
}
```

File:

`src/network/server-projectiles.cpp`

```cpp
if (step.passedThroughWorld) {
    // apply wall lifetime decay
}
if (step.type == ProjectileCollisionType::PlayerPassThrough &&
    stepDef && stepDef->id == "deathball") {
    glm::vec3 push = victimPos - center;
    ...
    glm::vec3 contactPoint = victimPos;
    nearestServerActorBodyPoint(players, npcs, victimRef.id, victimRef.kind,
                                center, stepTick, contactPoint);
    contact.point = contactPoint;
    contact.source = ServerDamageSource::PhysicalContact;
}
```

### Fix

A world overlap no longer consumes the actor contact. The same tick can apply
wall decay and actor damage. Damage uses the victim body contact point for
feedback and pushes outward.

### Proof

Runtime journal `20261101_014026`, build 8, sandbox mode, player -> NPC:

- `deathball.spawn` for projectile 13;
- three `deathball.contact` events, each `passed_through_world:true`;
- example contact: damage 8, `distance_fraction:0.175`, `distance_m:0.876`,
  health 76 -> 68, outward knockback `[0.7665, 0.2004, -0.1108]`,
  `contact_point` on the NPC;
- `server.damage.applied` recorded per-tick damage
  `2,3,5,6,7,8,9,8,7,5,3,2,1`, dropping the NPC from 99 to 33 health.

Build 15 compiled and passed `--canonical-contact-selftest`,
`--objective-selftest`, and `--map-config-selftest`.

### Remaining Coverage

This session proved player -> NPC. NPC -> player, NPC -> NPC, and
player -> player are not yet runtime-proven. The same server branch resolves an
NPC owner when `projectile.ownerNpcId != 0`, but the required actor/weapon matrix
still needs a multi-actor run.

### Human Review Still Required

- Confirm hit markers and damage numbers in normal gameplay.
- Confirm outward knockback feel and the expected 10 m visual diameter.
- Complete the actor/weapon matrix and any bad-connection profile runs.
