# NPC root could remain fully inside a closed block

Time created: 2026-11-01T21:40:00Z
Time last updated: 2026-11-01T21:40:00Z

Status: IMPLEMENTED IN SOURCE; RUNTIME RECOVERY VALIDATION PENDING

Related specification:
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/architecture/collision/collision.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_194906-actor-support-ai-match-journal-rotation.md`

---

## Regression Occurrence 1

### Observed

Live Dust2 game: NPCs visibly inside a block near the CT-spawn side. Four or
more actors appeared stuck while the journal still showed them moving.

### Expected Behavior

An NPC must not remain fully inside a closed static block. Recovery may move
the root to the last clear pose, a bounded safe point, or respawn it.

### Actual Behavior

`containActorFinalPose` only tested triangle intersections. An actor fully
inside a large closed block can have no animated surface crossing a block face,
so containment accepted the pose as clear even though the root was inside the
solid. A ray parity test alone is not enough because the map's enclosing room
shell is also "inside" the world mesh.

### Correction

Added `actorRootInsideClosedSolid`: it finds the nearest downward supporting
triangle, reads that triangle's collision solid id, and tests parity only
against that closed solid's triangles. Final-pose acceptance, last-clear
validation, bounded safe points, spawn-pose checks, and server inside-solid
recovery now use this test instead of global room parity.

`collision_probe` now reports `inside_closed_solid`, the ground triangle index,
and its solid id.

### Evidence

Live journal `logs/2026-11-01/20261101_204505/events.jsonl` (rotated into
`events-0001..0003` plus active `events.jsonl`) recorded NPC positions inside
the block, for example actor 200005 at `(-588.7, 89.1, 2361.1)` with
`inside_solid=true`.

Build 157 probe at that point reports `inside_closed_solid=true`,
`ground_solid_id=329`. A clean long runtime recovery run is still pending
because the dev-loop build pipeline was rebuilding other source changes.

---

## Regression Occurrence 2 — repeatable AI-playthrough check (2026-11-02)

### Observed

Human AI-playthrough report on 2026-11-01: an NPC was visibly inside a wall
during a live AI match. The previous occurrence's reported block point was
`(-588.7, 89.1, 2361.1)`.

### Expected Behavior

Every NPC remains outside closed static geometry in normal AI play. The first
failure must be visible in the canonical journal as `npc.pose.reset`,
`npc.pose.penetration`, or an `inside_closed_solid=true` containment sample.

### Actual / Current Evidence

Added `devscripts/scenarios/npc_wall_containment_v1.json`:

- probes the reported block with `collision_probe` and requires
  `inside_closed_solid=true` (proves the probe identifies the block);
- spawns four NPCs beside the block at `(-580,89,2361)`, `(-575,89,2361)`,
  `(-580,95,2361)`, and `(-575,95,2361)`;
- soaks for 30 seconds;
- fails if any `npc.pose.reset` or `npc.pose.penetration` appears.

Current runtime result on build `209`:

- scenario `npc_wall_containment_v1`: PASS, 3 assertions;
- journal `logs/2026-11-02/20261102_002034/events.jsonl`;
- no `npc.pose.reset` and no `npc.pose.penetration` in the scenario window.

### Remaining

The live human AI-playthrough replay that first showed the NPC inside the wall
was not captured. Run the same map/mode/route with the new containment sample
and attach the first `npc.pose.*` or `inside_closed_solid` event. If it
reproduces, record the exact actor, position, root correction, and solid id here
and fix the responsible final-pose/closed-solid owner.
