# Grounding follows the limbs; walls no longer play the land sound

Date: 2026-09-23
Status: hot-code built; deterministic tests pass; human acceptance pending

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Reported symptoms

- The "higher grounded" state still won: the body rested above the floor.
- Running into a wall (not landable) played the landing / ground-smash sound.
- Wall contact did not feel like it reset abilities the way afad20a did.

## Cause

1. `settleToGround` and the grounded `feetZ` scan still used the root **capsule**.
   The capsule is longer than the visible legs, so it reached the floor first and
   held the body above the limb-touching height.
2. The land/ground-smash sound was gated on incoming speed across **all** world
   contacts, so a fast wall hit triggered it even though `grounded` was false.
3. afad20a reset abilities on **any** contact (ground, wall, ceiling, slope, limb,
   weapon) because every contact carries `resetsAbilities = true`; grounded was
   only `normal.z > 0.80` AND the contact point near the feet. The hot path
   already resets on any contact; the wall sound bug made walls feel like ground.

## Fix (hot code only)

`src/hot-reload/packages/collision/collision-package-solver.cpp`:

- `settleToGround`: now uses the lowest **body** collider (head/torso/arms/legs),
  excluding the capsule and weapon; the capsule is used only when no body
  collider exists. The body therefore rests where the limbs touch.
- `resolveOnce` `feetZ`: prefers body colliders (legs) and computes the bottom of
  oriented capsules from their lowest endpoint, so grounding follows the limbs
  rather than the capsule helper.

`src/hot-reload/modules/movement-system.cpp`:

- Landing/ground-smash sound now requires `q.grounded`, so a wall hit does not
  play it.

## World-touch semantics (afad20a parity)

- Any contact on the actor resets abilities. The hot path sets
  `st.collided = worldContact || bodyContact || contactCount > 0`, and the reset
  runs on `st.collided || st.grounded`. Touching a wall therefore restores dash /
  down-dash / freeze / jump, which is what lets you chain dashes along a wall.
- Grounded is still only a walkable normal near the feet; a wall is not ground.
- Collision skin: the hot solver uses `kSkin = 0.001` and
  `kContactTolerance = 0.02`; afad20a used `COLLISION_SKIN = 0.02` and a body skin
  of 0.04. Tune `kContactTolerance` in `collision-package-solver.cpp` (hot) if
  contact feels too early/late.

## Evidence

Build evidence:

- Hot DLL `DLL build success` (82 sources). No cold rebuild for this change.

Test evidence:

- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

Human acceptance:

- Pending. Standing should rest at the limb height; running into a wall must not
  play the land sound; wall contact must reset abilities.

## Limits

- If a low-hanging arm/weapon is the lowest body collider, the actor rests on it
  (limb authority, afad20a). Exclude a specific part in the hot solver if that is
  unwanted.
- The limb fix from the earlier session still requires the EXE built after it
  (`mimita-20260922T223346.exe` or newer); the changes in this session are hot.
