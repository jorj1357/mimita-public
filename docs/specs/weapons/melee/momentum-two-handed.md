# Momentum-driven Two-handed Melee

**Purpose:** define reusable physical two-handed melee that blends authored
attack animation with momentum-driven weapon motion.
**Audience:** melee, physics, animation, networking, weapon, and content
contributors.
**Does not:** replace the shared contact/damage owner, require IK before the
first usable slice, or make an animation pose itself a hit.

## Behavior

- The weapon is held in both hands through the shared actor rig and weapon
  attachment path. Grip, mass, center of mass, reach, joint constraints, and
  collision shape are authored per weapon/avatar compatibility data.
- An attack clip supplies a desired pose and named windup, active, recovery,
  and return-to-idle intervals. Physical constraints and momentum may move the
  actual weapon away from that pose. Blend the animation target with the
  physical transform; do not hard-lock the blade/club to a prerecorded arc.
- The visible weapon and collision shape use the same simulated transform.
  During the active interval, fixed 60 Hz swept contacts may hit objects or
  body parts outside the nominal animation path if actual momentum carries the
  weapon there. A visual animation crossing without physical contact is not a
  hit. No damage is dealt during windup or recovery unless a later weapon
  contract explicitly defines a separate contact mode.
- The server validates contact and damage through shared melee/contact
  authority. Clients may predict pose and cosmetic response, but cannot
  self-confirm damage. Batching may reduce message overhead while preserving
  per-tick contact ordering and the authoritative active window.
- Weapon mass and swing commitment should emerge from acceleration, momentum,
  constraints, and recovery timing. Any assist/smoothing must be bounded,
  authored per weapon, and must not create a hit volume disconnected from the
  visible weapon.
- First implementation may use existing animation clips, authored keyframes,
  and shared attachment/constraint support. IK, new animation-editor workflows,
  generalized reusable actor presets, and map-editor weapon modeling are later
  capabilities and not prerequisites for the Bloodtest specification.

## Acceptance

Capture actual weapon transforms and contact ticks in a pinned multiplayer run.
Demonstrate one hit within the active window, a physically deflected swing that
can contact beyond the nominal clip path, no hit from a clip-only crossing,
correct two-hand attachment, and server-confirmed damage for players and NPCs.
Review animation blend and hit readability by a human.

## Related specifications

- [Bat](bat.md)
- [Mace](mace.md)
- [Swordsword](swordsword.md)
- [Animations](../../animations/animations.md)
- [Networking contact authority](../../networking/contact-authority.md)
