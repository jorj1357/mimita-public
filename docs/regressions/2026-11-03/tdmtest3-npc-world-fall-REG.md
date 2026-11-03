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
