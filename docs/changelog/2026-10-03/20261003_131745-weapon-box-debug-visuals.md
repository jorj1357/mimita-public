# Weapon box debug visuals

Date: 2026-10-03 13:17:45 EDT  
Branch: `afad20a-rebuild`  
Result: `PASS_WITH_HUMAN_REVIEW`

## Request and finding

The requested `visible` flag in `config/weaponcollisions.json` did not show
box-based weapon colliders. The loader built box triangles, but the renderer
only handled capsules and required `hasWeaponCollisionCapsule`, which is false
for `source: "boxes"`. Spy Knife also had a separate attack OBB debug renderer
controlled by `config/weapons.json` `custom_params.hitboxVisible`, currently 1,
so its visible box was not the `weaponcollisions.json` collider.

## Changes

- `src/entities/player.h`: added runtime debug records for configured boxes.
- `src/combat/weapon-collision-config.cpp`: copies enabled box geometry into
  runtime debug state alongside the existing collision triangles.
- `src/debug/debug-visuals.h` and `src/debug/debug-visuals-lines.cpp`: added an
  ungated oriented box wireframe renderer using the weapon transform, center,
  half-size, scale, and Euler rotation.
- `src/combat/weapon-system.cpp`: `weaponcollisions.json` `visible` now draws
  every enabled configured box for all box-mode weapons; capsule rendering is
  retained for capsule-mode weapons.
- `config/weapons.json`: changed Spy Knife `custom_params.hitboxVisible` from
  `1` to `0`, hiding its separate attack-OBB debug drawing.

## Specification and skill review

Reviewed `docs/specs/20261001-weapon-collision-handoff.md`,
`docs/architecture/collision/collision.md`,
`docs/architecture/json-configuration/json-configuration.md`, and
`docs/skills/spec-behavior-review-v1.md`.

Finding: `weaponcollisions.json` documents `visible` as showing the configured
collider, but the prior implementation only showed capsules. This was a
spec-code disagreement; the implementation now covers box geometry without
changing collision ownership. The Spy Knife attack OBB remains a separate
gameplay/debug shape owned by `config/weapons.json`.

## Validation

- `git diff --check`: passed for the changed files; pre-existing whitespace
  warnings remain in unrelated files.
- `python build_agent.py`: `BUILD SUCCESS`, return code 0, 100 compiled and
  417 skipped.
- `mimita.exe --actor-collision-mesh-selftest`: passed.
- `mimita.exe --actor-triangle-solve-selftest`: passed, including weapon mesh
  contact and no-launch assertions.

## Human review still required

Launch the newly built executable, equip rocket launcher, grenade launcher,
Spy Knife, and another box-mode weapon, and confirm `visible: true` draws the
cyan wireframe at the actual collider while `visible: false` hides it. Confirm
Spy Knife no longer shows the red attack OBB, and separately verify its attack
behavior still functions. Build and self-tests do not prove visual acceptance.

## Pre-existing work preserved

The worktree already contained unrelated changes in maps, gamemode/network
files, `build_agent.py`, configuration files, and an existing changelog; those
were not modified or claimed as part of this change.
