# Zombie Tower first playable contract

- Status: `SPEC_UPDATED_NEEDS_DECISIONS`.
- Timestamp: 2026-10-19T22:23:31Z (2026-10-19 18:23:31 EDT, America/New_York).
- Request: Incorporate the user's answers into a clear first-playable Zombie Tower implementation contract.
- Skill: `docs/skills/spec-behavior-review-v1.md`.
- Router note: `docs/doc-review-09-03-2026.md`, listed by `docs/ROUTER.md`, is absent in this checkout.
- Pre-existing work: The user's raw Zombie Tower note had already been added at the beginning of the spec and appeared as an uncommitted change before this task. It was preserved verbatim below the new contract under a historical-notes heading. Other unrelated worktree changes were not modified.

## Change

Added `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1-61`, a first-playable implementation contract covering the four phases; 30-second in-map lobby/intermission and load wait; 3/2/1/GO countdown; four-actor default human team and NPC fill/replacement; starting weapons/ammo; finite spawn-volume encounters; three attempts, down/revive, checkpoints; final-boss target; result metrics; and required mode music, fog, rain, torch lighting, forced first person, and an effects toggle. The new section labels unresolved behavior as decisions needed before implementation and marks the original informal text as historical context without deleting or rewriting it.

## Exact before and after

- Before at the top of the file: `2026 10 19 1752 jorj  im gona do juts like a little`
- After: no prior text for the new section; it begins with the exact addition `## First playable version — implementation contract` and is followed by the 59-line contract at `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1-59`.

## Review and evidence

- Read `docs/ROUTER.md`, the Zombie Tower spec, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/npc-behavior/npc-behavior.md`, and `docs/skills/spec-behavior-review-v1.md`.
- The spec's exact lobby-start rule, inclusive human-team actor count, NPC fill/replacement, loadout, encounter activation, reactivation policy, revive/wipe state, five-second results, statistics, and visual/audio requirements now reflect the user's answers.
- Open decisions remain for player-controlled monster role/admission and late joining; what happens after a wipe when attempts remain (the five-second Results answer conflicts with checkpoint retries and three shared attempts); final-boss preset/behavior; exact music catalog entries and effects-toggle command/scope; and the full tower's concrete required/optional encounter manifest.
- `git diff --check -- docs/specs/gamemodes/zombie-tower/zombie-tower.md` reported trailing whitespace in the preserved raw user notes, which are uncommitted additions from before this task. No lines in that historical note were edited to avoid altering the user's work.
- No tests, build, or runtime validation were run; this was a specification-only change. The mode's runtime behavior remains unverified.
