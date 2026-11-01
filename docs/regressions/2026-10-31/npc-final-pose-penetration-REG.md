# NPC final animated pose entered static geometry

Time created: 2026-10-31T11:50:24Z
Time last updated: 2026-10-31T18:06:36Z

Status: IMPLEMENTED IN SOURCE; SELFTEST PASS; RUNTIME VISUAL PENDING

Related specification:
`docs/specs/npc-behavior/npc-behavior.md`

Related architecture:
`docs/architecture/collision/collision.md`,
`docs/architecture/player-npc-systems/npc-movement.md`

Related changelog:
`docs/changelog/2026-10-31/20261031_140500-collision-stepup-and-scenario-evidence.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-31T11:50:24Z` (human report)

NPCs and large monsters can be seen with limbs or weapons inside blocks even
when the actor root is outside. The player specifically reports the Chainsaw
Boss, whose legs enter world geometry. The boss is much taller than a player and
does not behave as if its actual body shape were colliding with the world.

### Expected Behavior

A living NPC or monster must not visibly occupy static world geometry. Its final
animated body and held weapon are the collision shapes. Collision may move the
root until the final pose is clear, but must not rewrite animation transforms.
If the pose cannot be cleared, the NPC restores its last clear root pose.

### Actual Behavior

Movement collision runs before the final AimBody/arm-ragdoll pose. The final
animated limbs are not re-tested against the world. Recovery checks only the
NPC root point. A limb can therefore be inside a block while the root is clear,
and the NPC remains there.

### Why This Is Bad

Tall monsters look like they are clipping into the map, can become stuck, and
make collision behavior depend on which part of the body happens to be checked.
A single actor-pair capsule is not a valid substitute because it does not match
a tall, wide, thin, or multi-limb avatar's actual world silhouette.

### Specification

`docs/specs/npc-behavior/npc-behavior.md` now defines final-pose world
containment: final body/weapon collision meshes, root-only correction, 1-2 cm
standoff, last-clear restore, and bounded safe-point/respawn fallback.

`docs/architecture/collision/collision.md` separates actor-vs-world body
triangles from the actor-pair capsule approximation.

### Wrong Code

Not yet confirmed. The current order is:

- `src/npc/npc.cpp:2553` runs `physicsMainUpdate()` for the NPC;
- `src/npc/npc.cpp:2570` runs `updateNpcAim()` afterward;
- `src/ragdoll/ragdoll-mode.cpp:1472` writes final aim/ragdoll limb transforms;
- `src/network/server-npcs.cpp:1886` checks whether only the NPC root point is
  inside solid geometry.

That order permits final limb overlap after the movement collision pass.

### Confirmed Cause

Not confirmed. The order and root-only recovery are strong candidates and match
the human report, but a final-pose trace is still required.

### Evidence

- `logs/2026-10-31/20261031_143828/events.jsonl` contains 29
  `npc.capsule.recovered` events, including downward corrections of about 10 m
  (for example from z `2343.73` to z `2333.11`).
- The supplied `scenario-20261031_143812-collisss.jsonl` contains
  `client_collision` but has no footer and no complete server acceptance.
- No final-pose NPC collision event exists yet.

### Next Step

Add bounded final-pose penetration events including NPC id, part, triangle id,
normal, depth, root before/after, and reset reason. Reproduce on a
dev-loop-owned server before changing the solver.

## Resolution — 2026-10-31T18:06:36Z

Implemented `containActorFinalPose()` in
`src/physics/movement/actor-final-pose.cpp`, called from `npc.cpp` after
`updateNpcAim()`. It tests final body and held-weapon meshes, translates only the
root, records the last clear pose, and falls back to a bounded safe standing
point. Logs: `npc.pose.penetration`, `npc.pose.contained`, `npc.pose.last_clear`,
and `npc.pose.reset`.

Deterministic checks pass:
- final pose is contained
- root correction clears the wall
- final residual penetration is below standoff
- clear pose is accepted and remembered

Runtime evidence on build 4003: four NPCs were spawned and simulated with no
`npc.pose.penetration` or `npc.pose.reset` events. Human visual acceptance and
the no-valid-pose server respawn hook remain open.
