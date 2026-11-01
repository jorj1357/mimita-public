# Random-avatar hit validation, weapon final pose, and standoff stability

Time created: 2026-11-01T05:00:38Z
Time last updated: 2026-11-01T05:00:38Z

Status: PARTIAL — hitscan/rewind and generic-part code implemented; runtime hit acceptance and headless random-avatar collision bodies still need live validation.

## Goal

Fix three reported gameplay failures on `dust2cyberiav4` / TDM random avatars:

1. living humanoid and monster NPCs sinking or getting caught;
2. hitscan/shotgun direct hits sometimes doing no damage;
3. the player not being pushed by the held weapon when turning into geometry.

## Changed

### Hitscan / shotgun

- NPC hit validation now uses the NPC's replicated body-part boxes at the
  rewound tick instead of the standard humanoid template.
  - Trace targets: `buildNpcTargetBodyPartBoxes` / `fillNpcTargetBodyParts`.
  - Claim acceptance: `claimedHitInNpcBodyParts`.
- `resolveServerContact` now validates hitscan at `contactTick = rewindTick`,
  matching the pose the shooter saw.
- Client local trace now includes `physicalBody.extraParts` for remote NPCs and
  remote players, so random-avatar/monster parts are hittable.
- NPC body-part samples now include `extraParts` and canonical part mapping
  (head/leg/torso), with capacity raised from 8 to 16.
- Third-person parallax remains intentional in `docs/specs/weapons/weapons.md`:
  the shot starts at the real muzzle tip, parallel to camera forward.

### Final pose / weapon contact

- Headless actor collision construction now loads generic/extra avatar parts
  without requiring a GL context; render meshes are still skipped when
  headless.
- Final-pose containment now runs on `extraParts` for actors with no canonical
  six-part body.
- Player final pose runs again after `WeaponSystem::update` publishes the
  current weapon transform (`post_weapon`).
- Fixed standoff drift: correction moves only the penetration beyond the
  allowed standoff instead of adding the full standoff every tick.
- Safe-point recovery is limited to nearby lifts and requires a walkable
  support contact; it no longer lifts actors 8–12 m to arbitrary roofs.
- Contact diagnostics now include `part_count`, `extra_part_count`, and
  `skip_reason`, and copy the first four world contacts instead of the first
  four mixed contacts.

### Docs

- `docs/specs/movement/movement.md`: player final pose included; body/weapon
  contact may lift and support at ledges.
- `docs/architecture/collision/collision.md`: canonical and extra/generic parts
  participate; player final pose; weapon lift/support.
- `docs/specs/npc-behavior/npc-behavior.md`: player final pose and respawn hook
  status updated.
- `docs/specs/weapons/weapons.md`: third-person muzzle-origin parallax is
  intentional and must not be changed to camera-origin.

## Evidence

- Latest successful build: `86`.
- `--actor-final-pose-selftest`: PASS, including extra-only parts.
- `--actor-triangle-solve-selftest`: two pre-existing failures remain; flush
  seam checks pass.
- Live run `20261101_045902`, build 86, `npc_spawn 20`:
  - `npc.pose.contained`: 0
  - `npc.pose.penetration`: 0
  - `npc.pose.reset`: 0
  - `player.pose.contained`: 17
  - `player.pose.reset`: 0
  - NPC pose records still report `part_count=0`, `skip_reason=no_parts` for
    headless random avatars. The headless avatar collider loader was tested but
    produced invalid resets, so it is not wired into server spawn yet.

## Still open

- Headless/dedicated-server random-avatar and monster collision bodies are still
  absent; a safe headless body loader that validates body-frame alignment must
  be added before claiming NPC sink is fixed in TDM pregame.
- Hit acceptance against a moving random-avatar/monster NPC still needs a live
  revolver/shotgun test.
- Charger CT-side navigation stuck still needs a separate navigation/partial-path
  fix.
- Human visual review of NPC feet, weapon ledge support, and third-person
  parallax is required.
