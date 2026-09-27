# Ground settling uses the lowest body collider (limb authority)

Date: 2026-09-23
Status: hot-code built; deterministic tests pass; human acceptance pending

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## Reported symptom

Two resting heights: standing stable put the actor slightly ABOVE the ground,
while a ground down-dash dipped slightly BELOW and sometimes bounced. The user
wants the resting height to be where the limbs touch the ground, not the higher
capsule height.

## Cause

`settleToGround` in `collision-package-solver.cpp` only considered the root
capsule. It pulled the actor down until the CAPSULE bottom touched the floor, so
the visible body (whose feet/legs sit at a different height than the capsule
bottom) rested slightly above the surface. The down-dash briefly moved the body
low enough for the limbs to touch, which is the intended lower height.

## Fix (hot code only)

`src/hot-reload/packages/collision/collision-package-solver.cpp`
`settleToGround` now finds the **lowest surface among all colliders** (capsule,
head, torso, arms, legs, weapon) and settles the actor so that lowest collider
touches a walkable surface. Grounding is therefore governed by the whole body,
so the resting height is the limb-touching height. Only snaps down, only within
`kGroundSettleDistance`.

## Hot-reload boundary (the cold build question)

The cold rebuild was needed only because the `body.parts` capability payload
(`GameBodyPartV1` in `game-api.h` / `capBodyParts` in `live-behavior.cpp`) is EXE
code and its struct changed. Going forward:

- The limb collider builder (`movement-system.cpp`, the region around lines
  494-585) is hot: it lives in `src/hot-reload/modules/*.cpp`.
- The collision solver and this ground-settle change are hot:
  `src/hot-reload/packages/collision/*.cpp`.
- Only a change to `GameBodyPartV1` / `capBodyParts` (the kernel capability)
  needs a cold rebuild.

So limb shape, radius, sampling, and ground-settle tuning are now hot-editable.

## Evidence

Build evidence:

- Hot DLL `DLL build success` (82 sources). No cold rebuild for this change.

Test evidence:

- `--movement-selftest`, `--movement-parity-selftest`, `--live-code-selftest`,
  `--collision-selftest`, `--afad20a-parity-selftest`: PASS.

Human acceptance:

- Pending. Standing should now rest at the limb-touching height; the ground
  down-dash bounce should remain.

## Limits

- If a low-hanging arm/weapon is the lowest collider, the actor will rest on it
  (limb authority, matching afad20a). If that is undesired for a specific part,
  exclude that part from the settle scan in the hot solver.
- The limb fix from the previous session still requires the new EXE
  (`mimita-20260922T223346.exe`) because the capability is EXE code; this
  ground-settle change is hot.
