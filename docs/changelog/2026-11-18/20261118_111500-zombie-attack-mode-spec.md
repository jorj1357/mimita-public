# Zombie Attack mode specification

- Added a standalone `zombie_attack` specification for endless cooperative
  monster waves, starting with five monsters, progressive count/speed/damage
  escalation, and staged random roster introductions.
- Linked the specification from the gamemode index. Existing Zombie Tower
  contracts remain separate.
- Evidence: inspected `docs/specs/gamemodes/zombie-tower/zombie-tower.md`,
  including its first-playable fixed tower run and future Infinite Zombie
  Tower compatibility section; neither specifies this separate wave loop.
- Validation: documentation links and changed-file structure reviewed. No
  implementation, build, runtime, or human gameplay acceptance was performed.
- Repository-wide documentation TODO scan found a related unresolved note at
  `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5861`: `2026 10 06
  1603 jorj todo - explain zombie tower`. Suggested expansion: summarize the
  co-op survival premise, floor progression, and how that mode differs from
  the new endless wave mode. It was left unchanged because it is outside this
  spec addition.
- Review instructions used: `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/documentation-checker-v1.md`.
