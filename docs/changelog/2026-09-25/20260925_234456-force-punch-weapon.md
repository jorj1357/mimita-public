# Force Punch one-tick sphere weapon

## Request

Add a new weapon that creates a sphere at the actor's right hand for exactly
one client/server simulation tick. A target at the sphere center receives
maximum force, damage, and knockback; edge contacts receive less.

## Specification review

- Existing owner reused: the QuickHit physical-contact path already owns short
  hand-attached contact weapons, server attack activation, player collision,
  authoritative damage, and knockback.
- New weapon identity: `force_punch`, slot 13, JSON behavior type `quickhit`.
  No new weapon framework or network transport was added.
- Active duration: `activeHitboxTicks=1`.
- Shape: `hitboxSphere=1`, centered at the hand position derived from the
  right-arm attachment and `handForwardOffset`.
- Force equation for the sphere:
  `centerForce = clamp(1 - centerDistance / (sphereRadius + bodyRadius), 0, 1)`;
  movement force is also normalized by `maxForceSpeed`, and the larger value
  is used.
- Damage equation: `minDamage + (maxDamage - minDamage) * force^exponent`.
- Knockback uses the same force value and its own configurable exponent.
- The server repeats the calculation and owns the real player health and
  knockback result.

## Changes

- Added the `force_punch` weapon definition to `config/weapons.json`.
- Added its built-in fallback definition in `src/combat/weapon-data.cpp`.
- Extended QuickHit shape calculation to support a one-tick hand sphere.
- Extended server physical-contact calculation to use center overlap force for
  both damage and knockback.
- Reused the existing local and remote weapon-shape debug rendering.

## Evidence

- `force_punch` JSON validation passed.
- `git diff --check` passed.
- `python build_agent.py` with the local compiler and GLFW paths: SUCCESS;
  `mimita.exe` linked successfully.
- `mimita.exe --snapshot-chunk-selftest`: PASS.
- A live two-client hit and visual acceptance were not performed in this
  session.
