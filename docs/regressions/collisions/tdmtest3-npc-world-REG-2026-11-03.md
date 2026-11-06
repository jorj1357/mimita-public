# tdmtest3 headless NPC world had no floor

Time created: 2026-11-03T17:44:09Z
Time last updated: 2026-11-03T17:44:09Z

Status: ATTEMPTED FIX (1) — solid-id fallback in source; runtime review pending

Related specification: `docs/specs/npc-behavior/npc-behavior.md`
Related architecture: `docs/architecture/collision/collision.md`
Related changelog: pending for this session.

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T17:44:09Z`

Human recording:
`devscripts/scenarios/user/scenario-20261103_174409-tdmtest3npc.jsonl`

### Expected Behavior

NPCs spawned on tdmtest3 stand on the map floor, report ground contact, and
walk or slide like the Nov 2 builds.

### Actual Behavior

- Player position is near `z=2328`.
- NPC positions are near `z=50–288`.
- Every NPC sampled has `v.z=-175`, `ground=false`, `world_contact=false`.
- The recording stopped with `stop_reason:"capture_failed"` after 22.6 s.

### Confirmed Cause

The headless server NPC collision world was empty on tdmtest3 after the map
loader marked every triangle with the removed sentinel. NPC physics had no
support surface, so NPCs fell indefinitely.

### Corrected Code

`src/network/server-npcs.cpp` `buildNpcWorldCollision()` now falls back to the
complete headless triangle set when filtering solid ids leaves the NPC world
empty. This is uncommitted source at the time of this record and is retained by
the current fix.

### Proof

Source change only at this time. Runtime proof must show the
`npc.collision-world.solid-id-fallback` event with a valid triangle count and
NPCs grounded on tdmtest3 for at least 60 seconds.

---

## Regression Occurrence 2 — union output dropped from NPC world (2026-11-04)

Time: 2026-11-04T01:00:00Z

### Observed

Human recording `devscripts/scenarios/user/scenario-20261103_214709-atdmfall.jsonl`:
8 of 9 NPCs reached `z ≈ -502`; four ended the recording still falling with
`v.z ≈ -175`. The server log showed 51,879 headless triangles but only
45,227 NPC-world triangles.

### Confirmed Cause

`src/map/map-static-union.cpp` marked all 6,654 union-output triangles with
`UINT32_MAX`, while `buildNpcWorldCollision()` treated `UINT32_MAX` as a removed
solid and filtered the union geometry out. The NPC world also did not copy
`solidIds`, so `actorRootInsideClosedSolid()` returned false for every NPC.

### Corrected Code

Distinct removed/union solid sentinels; NPC world copies triangles and solid
IDs; union triangles are collidable and raycastable; collision world bounds are
rebuilt after chunk construction. `npc.collision-world.summary` now reports
alignment and union counts.

### Proof

30 s `atdm` run: `headless_triangles=51879`, `npc_triangles=51879`,
`union_triangles=6652`, `solid_ids_aligned=true`; all final NPC traces were
above the map with `inside_solid=false`. Runtime proof on `tdmtest3` itself for
60 seconds remains pending.
