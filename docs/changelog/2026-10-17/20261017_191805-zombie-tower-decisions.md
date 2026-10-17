# Zombie Tower design decisions added

- Timestamp: 2026-10-17 15:18:05 EDT (2026-10-17T19:18:05Z)
- Branch: `20261011zt`
- HEAD before this documentation change: `bc4f69817358bcfcc2d4fafabafe7e410402e0a0`
- Result: PASS_WITH_HUMAN_REVIEW

Updated `docs/specs/gamemodes/zombie-tower.md` with the user's 2026-10-17
answers. The spec now states a four-player starting target without a fixed
party cap, a 50-floor minimum, automatic 30-second lobby intermission, a 75%
floor-clear gate and the requested Arena Closer behavior, three shared team
attempts, checkpoint-only wipe recovery, zombie return for dead players, the
tentative loadout, and persistence from the first shipped version. The current
JSON NPC cap of one remains a test setting and was not changed.

## Exact spec edits

- `docs/specs/gamemodes/zombie-tower.md:522` — changed the implicit starting
  party expectation to four players while clarifying that it is not a cap.
- `:555` — changed the tower target from `20+ floors` to `at least 50 floors`.
- `:763-790` and `:809` — specified the themed playable lobby and automatic
  start after 30 seconds.
- `:844-848` and `:867-872` — made checkpoint recovery whole-party and
  checkpoint-only; clarified three attempts are shared by the team; recorded
  the return-as-zombie role detail as `NEEDS_SPEC_DECISION`.
- `:999-1007` — added the tentative melee weapon and plentiful finite ammo;
  left exact ammo and shop/loadout interaction open.
- `:1352-1355` — specified unique recognizable models, sounds, attacks, and
  animations per v1 monster while reusing shared code.
- `:2883-2886` — specified return to lobby with a run summary after attempt
  exhaustion and retention of earned gold/XP.
- `:3670-3680` — required gold, XP, unlocks, and completion rewards from the
  first shipped version; recorded currency and shop decisions as open.
- `:4136-4152` — specified that kills, higher checkpoints, and completing the
  tower award gold/XP that persist after a wipe.
- `:697-721` — replaced the informal Arena Closer note with a 75% floor
  clear rule, light/torch navigation guidance, and the 10 m radius, 100,000 HP,
  20 m/s, wall-passing, audible, 66-damage-per-tick target behavior.
- `:4895-4898` and `:5004-5014` — updated the v1 feature target and acceptance
  criteria to include the long run, lobby, floor gate, and shared attempts.

Representative before/after text:

```text
Before: 20+ floors
After: at least 50 floors

Before: The intermission may occur in a generalized shared lobby.
After: The run starts automatically when the 30-second intermission ends.

Before: 3 attempts per run
After: These are three attempts for the team as a whole, not three attempts per player.

Before: monsters had an animation set and sound set as configurable data
After: Each v1 monster should have its own recognizable model, sound set, attack, and animation set.

Before: (informal Arena Closer notes described a 20 m radius)
After: a black sphere with a face, 10 m radius, and 100,000 health
```

The shop currency and exact purchase rules, exact starting ammo, and how a dead
player returns as a zombie remain explicit design decisions rather than
invented requirements. The existing 45–60 minute target is retained but marked
for review against a minimum 50-floor tower.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/gamemodes/zombie-tower.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- The router-referenced `docs/doc-review-09-03-2026.md` is absent in this
  checkout; the route table and focused skills were followed directly.
- JSON/config, code, build, runtime, and multiplayer behavior were not changed
  or validated. The user stated the NPC cap of one is only for testing; it is
  left as-is.
- `git diff --check` reports the existing trailing whitespace in the long
  historical voice-transcription line at `zombie-tower.md:5526`; that text was
  not edited.
- Human review remains needed for the listed `NEEDS_SPEC_DECISION` items and
  for confirming the 45–60 minute duration is compatible with 50+ floors.

## Workspace preservation

The working tree already contained unrelated user changes in account config,
animation, entity-editor, gamemode, weapon, and changelog files. They were
preserved and are not attributed to this documentation update.
