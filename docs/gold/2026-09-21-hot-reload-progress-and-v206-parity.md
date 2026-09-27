# 2026-09-21 — Hot reload progress and v2.0.6 parity

Status: `GOLD / PROGRESS RECORD` — honest checkpoint for the hot C++/JSON
migration.

## What we expected

The original expectation was that restoring movement, collisions, animations,
weapons, tools, and ragdoll would require many cold executable rebuilds.

The work so far has instead stayed on the hot side. The user's current estimate
is that this has taken less than 100 real-world hours since starting the move to
hot-reloadable C++ behavior, with no cold executable rebuild needed so far.

That is possible because the work has been kept behind the existing generic hot
package boundary: hot systems, fixed POD recipes, JSON polling, and generation
swaps. The running EXE has not been replaced by this work.

## What is implemented so far

### Movement source selection

`config/movement.json` selects `cpp` or `json`. Both sources feed the same hot
movement tuning envelope and movement functions. JSON changes are read at the
tuning boundary; C++ changes arrive through a new hot DLL generation.

### Collision direction

The root capsule is marked as a helper. Hot body-part colliders are marked as
authoritative when present. The collision solver now allows body contacts to
handle wall and ceiling response instead of automatically letting the capsule
become an invisible wall.

This is only the first body-authority slice. It does not yet prove the desired
full feet/torso/arms/weapon root correction or the final v2.0.6 feel.

### Animation ownership

`pose-generation.cpp` remains the phase selector and
`tool-visuals.cpp` remains the hot tool animation owner. The existing C++ tool
phases were migrated into reusable JSON phase sets in `config/animations.json`.
Tools reference named phase sets, so animation data does not need to be copied
into a giant `weapons.json`.

The default remains `behaviorSource: cpp`, preserving the existing C++ phase
behavior. Switching to `json` activates the migrated phase sets. Missing or
invalid JSON phases fall back to the C++ phase data.

### Build boundary

The hot DLL has built successfully as:

`build/hotreload/mimita-live-g20260921.dll`

The live build path does not write `mimita.exe`. This checkout does not contain
`mimita.exe`, so live visual activation and human feel acceptance remain
unverified.

## What is still visibly wrong

The current result is not yet the old v2.0.6 behavior.

- Player locomotion and air movement have not completed deterministic v2.0.6
  parity verification.
- Player body animation and aim-body behavior are still only partially migrated.
- Weapon hitboxes can still disagree with the visible weapon/body pose. The
  visual transform and a collision capsule share a path in the cold viewmodel
  code, but the complete hot body/tool collision authority is not finished.
- Full physical arm, hand, weapon, torso, and feet contacts are not yet driving
  root correction in the requested way.
- Client-side ragdoll restoration has not yet been completed.
- No human playtest has confirmed wall bounce, floor contact, fast rotation
  deflection, or v2.0.6 movement feel.

## Why the hot path is useful now

The hot boundary is already carrying the parts that need frequent tuning:
movement behavior, collision policy, body pose, tool phase animation, and JSON
selection. A normal tuning change should therefore be a hot DLL generation or
a JSON edit, not an executable rebuild.

A cold build is still required only when the generic boundary itself changes:
the EXE ABI, component storage layout, scheduler phases, dynamic-loader
primitive, renderer/physics kernel, or another OS-level primitive. The current
work has avoided those boundaries.

## Next proof, in order

1. Trace the weapon hitbox from tool pose to collision query and fix the first
   owner where it diverges from the visible pose.
2. Reconstruct the exact v2.0.6 movement constants and formulas from Git and
   compare deterministic ticks.
3. Finish body-part sweeps and root correction, including feet, torso, arms,
   head, and weapon contacts.
4. Restore the old client-side ragdoll path and then make its policy hot.
5. Run live C++ → JSON → C++ switching and human acceptance without restarting
   the world or executable.
