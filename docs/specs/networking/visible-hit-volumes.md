// 2026-11-27
/* purpose
* define one actor hit volume built from the visible fixed-tick pose
* make "what you see is what you hit" true for every damage source
* fold hitscan, projectile, melee, splash, and claim validation into one geometry owner
* this file DOES NOT define weapon damage numbers or gore rules
* this file DOES NOT replace the contact/damage authority in contact-authority.md
*/

# Visible hit volumes

**Status:** Desired behavior contract.

## The rule

> The visible limb is the hit limb.

There is no separate invisible damage box. Every damage source traces the same
`ActorHitVolume` that the renderer poses at the same tick.

## One owner

`ActorHitVolume` is the single geometry owner for actor damage and actor
collision queries. It is built from the fixed-tick pose, not from the render
frame and not from a hand-authored capsule.

```text
ActorHitVolume
  actor id
  tick
  parts: [ { name, bodyPart, transform, localMin, localMax } ]
  capsule fallback: { a, b, radius }  // only when parts are absent
  revision / pose hash
```

It is cached per actor per tick. Building it twice for one tick is a defect.

## One collision function

All consumers use the same primitive:

```text
traceRay(hitVolume, origin, direction, length, filter) -> hit
overlapAabb(hitVolume, aabb) -> hits
sweepSphere(hitVolume, from, to, radius) -> hits
```

Rules:

- Hitscan, projectile, melee, splash, deathball, and claim validation call this
  surface.
- No consumer re-implements part iteration or point-in-box math.
- No consumer adds a second capsule-only path for actors that have parts.
- The fallback capsule is used only when the actor has no parts, and the event
  says so.
- Per-query allocation is forbidden in the fixed-tick path.

## Server authority

The server owns the authoritative hit result for damage.

Client flow:

```text
client predicts a hit and sends a claim:
  target, tick, hit point, claimed part, weapon, pellet count
```

Server flow:

1. Build the target's `ActorHitVolume` at the claimed tick.
2. Trace the same ray or swept shape against it.
3. Accept the claimed part only if the trace returns that part.
4. If the trace returns no hit, reject with `hit.claim.rejected` and a reason.
5. If the trace returns a different part, use the server part and record the
   disagreement.
6. Apply damage through the one damage sink.

The server never applies damage from a claim it did not verify. The client never
writes health.

## Part multipliers

Per-part multipliers are configuration, not code:

```json
{
  "head": 2.0,
  "torso": 1.0,
  "leftArm": 0.75,
  "rightArm": 0.75,
  "leftLeg": 0.75,
  "rightLeg": 0.75
}
```

Rules:

- The table is hot-reloadable.
- A missing part falls back to `torso`.
- The multiplier applies once per damage event, not once per pellet unless the
  weapon config says otherwise.
- The event records the part, multiplier, and final damage.

## Crouch and scale

- The hit volume uses the same crouch height scale and socket lift as the
  visible pose.
- The capsule fallback uses the same crouched height and grounded bottom.
- A hit volume MUST NOT lag the visible pose by a render frame.
- A remote actor uses the interpolated pose at the target tick, not the latest
  render pose.

## Events

```text
hit.volume.built       actor, tick, part count, pose hash
hit.traced             source, actor, part, point, distance
hit.claim.accepted     shooter, target, part, tick
hit.claim.rejected     shooter, target, reason
hit.part.disagreement  claimed part, server part, point
hit.fallback.capsule   actor, reason
```

These events make visible = hit auditable from a run journal.

## Performance

- Build once per actor per tick; share the immutable volume.
- Broadphase first, narrowphase second.
- No `std::vector` growth per query; use a fixed-capacity part buffer.
- The per-tick cost is recorded as `contact.hitvolume_ms`.
- A frame or tick that exceeds its contact budget emits a named event.

## Migration

1. Keep the current per-part boxes and capsule fallback as the source.
2. Extract `ActorHitVolume` from `server-contact.cpp` and the client tracer.
3. Point hitscan, projectile, melee, and splash at it.
4. Turn on server claim verification for players, then NPCs.
5. Remove the remaining actor capsule-only damage paths.
6. Delete duplicated part-iteration code.

Each step preserves current behavior until the new path is proven.

## Relationship to nodes and connections

In the graph model:

```text
actor --owns--> hit-volume --contains--> part
weapon --targets--> actor
contact --hits--> part --damages--> actor
claim --asserts--> part --validated-by--> server
```

The hit volume is a node, not a hidden copy of the actor. It is a per-tick
projection of the actor's visible pose.

## Acceptance

- A shot at a visible arm resolves to the arm and uses the arm multiplier.
- A shot that misses every visible part is rejected, even if it crosses the old
  capsule.
- The server rejects a forged part claim with `hit.claim.rejected`.
- Crouched actors have a crouched hit volume and grounded capsule fallback.
- Hitscan, projectile, melee, and splash all call the same trace/overlap
  surface.
- No per-query allocation appears in the tick profile.
- The journal shows `hit.volume.built` and `hit.traced` for a proven shot.
