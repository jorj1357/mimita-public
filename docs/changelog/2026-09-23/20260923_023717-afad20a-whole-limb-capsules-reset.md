# afad20a limbs — whole-limb capsules + permissive touched-world reset

Date: 2026-09-23
Status: hot + cold built; deterministic tests pass; requires the new EXE; human
acceptance pending

Related gold: `docs/gold/2026-09-23-afad20a-limb-collision-method.md`
Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Reported symptoms

- Limbs still enter walls a little; the user wants zero limb penetration.
- Down-dash / dash reset felt like it happened only once while standing still,
  unless moving/WASD.

## Root cause of the remaining limb penetration

The `body.parts` capability returned only the part world **position and
rotation**, but `Player::physicalBody.parts[].worldTransform` carries the model
**scale** (the GLB node local transforms include scale; `player-loader.cpp`
`nodeTransform` applies `glm::scale`). The hot builder computed the collider
centre as `origin + rot * localCenter`, which drops the scale, so every limb
collider was misplaced. The live journal confirmed the symptom:
`limbSrc=body.parts limbCols=6 limbHits=0`.

## Fix

- `src/hot-reload/game-api.h` `GameBodyPartV1`: now carries the full
  `worldMatrix[16]` and `previousWorldMatrix[16]` (column-major) plus the
  part-local collider AABB and a `space` flag.
- `src/live-code/live-behavior.cpp` `capBodyParts`: returns the raw
  `worldTransform` / `previousWorldTransform` matrices, so the collider centre is
  transformed by the full matrix (including scale).
- `src/hot-reload/modules/movement-system.cpp` `buildPlayerCollision`: each limb
  is now an **oriented capsule spanning the whole collider AABB** along its
  longest local axis (radius = the larger of the two cross-section half-extents,
  scaled by the matrix), instead of a single small sphere. This covers the whole
  arm/leg/torso/head so geometry cannot slip inside. The previous-tick matrix
  gives the sweep.
- The `space != 1` legacy path falls back to the socket path, so an older EXE
  hot-loading the new DLL does not misplace limbs.

## Touched-world reset

The afad20a rule is: touching anything restores the ability, and the restored
availability **persists until consumed**, so you only need to have touched the
world at some point before using it. The journal shows this already works: every
`q_down` in the latest session logged `down_dash_available=1`, and the reset
record shows `touch=1 grounded=1 collided=1 ... restored=1`.

To make it maximally permissive, `st->collided` now also counts any returned
contact this tick (`q.contactCount > 0`) in addition to the sticky
`worldContact`/`bodyContact` flags and `grounded`.

## Evidence

Build evidence:

- Hot DLL `DLL build success`; cold build `Status: SUCCESS`
  (`mimita-20260922T223346.exe`). The capability is EXE code, so the limb fix
  only takes effect after restarting with this EXE.

Test evidence:

- `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--collision-selftest`:
  PASS.

Journal evidence:

- Down-dash availability is restored while grounded: `movement.contact_ability`
  `touch=1 grounded=1 collided=1 ... restored=1`, and every `q_down` logs
  `down_dash_available=1`.
- The remaining limb failure before this fix: `limbSrc=body.parts limbCols=6
  limbHits=0`.

Human acceptance:

- Pending. Run `mimita-20260922T223346.exe`, push arms/legs into walls, and check
  `movement.collision ... limbHits>0 limb0=(...)`; limbs should stop at geometry.

## Limits

- The whole-limb capsule is an AABB-derived approximation (one capsule per part
  along the dominant axis), not the exact mesh. Very concave limbs could still
  clip a corner before the capsule contacts.
- `limb0` in the throttled log names the first limb capsule origin and radius so
  a remaining misplacement is still visible.
- The `space != 1` path is the socket fallback for older EXEs; it keeps the old
  (approximate) placement.
