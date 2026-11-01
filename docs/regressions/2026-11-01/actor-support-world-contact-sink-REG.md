# Actor support was merged into static-world floor contact

Time created: 2026-11-01T19:49:06Z
Time last updated: 2026-11-01T19:49:06Z

Status: IMPLEMENTED IN SOURCE; FOCUSED SELFTEST PASS; RUNTIME VISUAL PENDING

Related specification:
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/architecture/collision/collision.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_194906-actor-support-ai-match-journal-rotation.md`

---

## Regression Occurrence 1

### Observed

Human report on Dust2 TDM: NPCs and the player sink into the ground after
actor-vs-actor contact. NPCs pushed into a corner can sink or disappear into
world geometry. T-side NPCs are pushed toward spawn, and a CT-spawn block can
contain NPCs.

### Expected Behavior

Actor support keeps stacking and carry, but is never reported as static-world
contact. Any actor-pair correction, push, or support change is re-checked
against static map geometry in the same fixed tick. Losing support does not
preserve fabricated ground.

### Actual Behavior

The actor-pair solver set `onGround`/`stableOnGround` for support. The server
merge ORed that into `hasWorldContact`, and client/server paths skipped the
static-world solver whenever `supportKey != 0`. A downward push or sinking
support could therefore carry actors through the floor until a later tick.

### Cause

`src/physics/movement/actor-pair-collision.cpp` could not distinguish actor
support from world contact. `src/sim/simulate-tick.cpp` and
`src/network/server-npcs.cpp` only re-ran world collision for
`out.corrected && out.supportKey == 0`. `actor-final-pose.cpp` also recorded
actor-derived ground as its clear restorable state.

### Correction

- Added `hasWorldContact` to actor-pair inputs/outputs.
- Support/carry set grounded support only; losing support clears actor-derived
  ground.
- All player/NPC merge paths re-run the static-world solver after correction,
  push, or support, then merge `worldGrounded || actorSupported`.
- Final-pose clear ground uses real world contact only.
- Default actor-pair profile restored to 0.7 m radius and documented push
  tuning.

### Evidence

`--actor-pair-support-selftest` on build 152 passes: support is registered, its
key names the lower actor, supported ground is true, world contact stays false,
and lost support does not fabricate ground or world contact.

Runtime AI matches on build 139 and 152 recorded actor contacts, accepted NPC
shots, and damage. Human visual acceptance of no-sink and corner behavior is
still required.
