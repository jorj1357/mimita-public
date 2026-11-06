# NPC template body colliders caused phantom overlap and floating

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-03T19:09:00Z

Status: ATTEMPTED FIX (1) — source changed; runtime review pending

Related specification: `docs/specs/npc-behavior/npc-behavior.md`
Related architecture: `docs/architecture/collision/collision.md`
Related changelog: pending for this session.

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T19:09:55Z`

Build `487ecbeb9ba80e2e9808cd9498a0d2f7145950022822d154e61633a8998c0df7`
included uncommitted worktree code that added
`ensureNpcTemplateCollisionBody()` to headless NPCs.

Journal: `logs/2026-11-03/20261103_191001/events.jsonl`

### Expected Behavior

Headless NPCs use the shared capsule movement path and a bounded ground clamp.
NPC collision must not be several hundred milliseconds per tick, and static
world contact must not lift NPCs into the air or reset them repeatedly.

### Actual Behavior

- 707 `performance.server-tick-stall` events.
- NPC stage 112–200 ms average, up to 320 ms; total tick up to 587 ms.
- One 67-NPC window recorded `npc_avg_ms=1344.9`.
- 3961 `npc.pose.reset`, 17106 `npc.pose.contained`, 289
  `npc.pose.penetration`.
- 1330 `collision.last_valid.rollback`; one body contact reported
  `penetration=107.4 m` and `part_velocity≈(8082,-13221,-0.76)`.
- Safe-point searches tried 125 candidates each, averaging 17.7 ms and reaching
  55.3 ms.

### Confirmed Cause

The uncommitted template body created six box colliders per headless NPC and
ran final-pose containment up to three times per NPC per tick (`post_animation`,
`post_ground_clamp`, `post_actor_pair`). The collider sweep evidence is not
consistent with a body aligned to the actor root, so containment chased a
phantom overlap. `findBoundedSafePoint()` then searched 125 poses, each doing a
same-solid triangle scan through `actorRootInsideClosedSolid()`.

### Corrected Code

Removed `ensureNpcTemplateCollisionBody()` and all three call sites from
`src/network/server-npcs.cpp`. The tdmtest3 solid-id fallback in
`buildNpcWorldCollision()` is retained.

### Proof

Source change only at this time. Runtime proof must show:
- no `npc.collision-body.template-fallback`;
- no `npc.pose.reset` or `npc.pose.penetration` in normal tdmtest3 NPC play;
- NPC stage and fixed-tick p99 within budget;
- NPCs grounded and walking/sliding under human review.
