# NPC root could remain fully inside a closed block

Time created: 2026-11-01T21:40:00Z
Time last updated: 2026-11-07T16:04:00Z

Status: DOWNWARD-PUSHOUT GUARD RUNTIME-PROVEN; INITIAL CAPSULE COLLISION MISS AND FINAL FALL-SAMPLE RUNTIME CHECK OPEN

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

---

## Regression Occurrence 3 — nearest-face recovery exited through the floor (2026-11-07)

### Observed

Live Fun Counter Strike run on `dust2cyberiav4`, build
`2026-11-07T15:40:16.360512Z:032addb5b338935afeda24da592c38af785dc5e33316a71888a53fb24b1adb77`,
journal `logs/2026-11-07/20261107_154026_01/server-01/events.jsonl`:

- NPC collision world had 5,463 source and NPC triangles with aligned solid IDs.
- At tick 4,296 actor 100004 was inside solid 37 at
  `(-867.2983, 150.5471, 2343.2297)`; `npc.root.pushout` accepted a downward
  correction `(0, 0, -1.9380)` to z `2341.2917`.
- At tick 4,442 the same actor was at z `2133.2598`, below the map bound
  `2295.3118`, and `npc.below-map.respawned` moved it to spawn.

### Confirmed recovery cause

`pushActorRootOutOfSolid` selects the nearest enclosing face. The server treated
any correction that cleared the solid test as safe, including a downward exit
through the bottom of a floor slab. That left the NPC below the walkable floor.

### Correction and current limits

`src/network/server-npcs.cpp` rejects downward nearest-face corrections, restores
the pre-correction position, and attempts the existing validated standing-point
recovery. If no safe point exists it now respawns immediately. A confirmed
below-map position also respawns on the following fixed tick. These are safety
recoveries; they do not prove that the capsule solver no longer misses an initial
world contact.

### Runtime evidence

On build
`2026-11-07T15:59:33.671985Z:fbb6c9494de679e6ec71dd25f8c8563006da3d9b1ee77c32e16a43183bc0b085`,
mode 47 produced journal `logs/2026-11-07/20261107_155949/server-01/events.jsonl`.
It recorded two `npc.root.pushout.rejected` events, eight immediate
`npc.root.respawned` events with `reason=no_validated_safe_point`, 67 `npc.trace`
samples, zero `npc.below-map.respawned`, and zero trace samples below the map
bound. This proves the recovery path handled the observed embedded actors in
that run; it does not prove human-visible movement or that all floor geometry
is safe.

The later build
`2026-11-07T16:03:14.815682Z:b336717d0c367a74498a75fa817565143e5b244691ec21bf12a8e942ee111469`
adds `npc.collision.fall-sample` with capsule sweep inputs and outcome. That
event still needs a live mode-47 run. The mode selection changed during this
session, so the active mode-51 run was left untouched.
