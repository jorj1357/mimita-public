# Rocket Duel loadout lock

- Timestamp: `2026-11-07T16:42:01Z` (UTC; `2026-11-07 11:42:01 EST`).
- Branch: `20261011zt`.
- Commits: none; no commit created.
- Result: `PASS_WITH_HUMAN_REVIEW`.

## Scope and pre-existing edits

Changed only the Rocket Duel mode's all-actor weapon-set policy. The worktree
already contained unrelated edits in account, analytics, audio, map, movement,
NPC difficulty, video, Blender tooling, regression/spec files, and existing
changelog/gold files; they were left untouched.

## Change

- `config/gamemodes/rocket_duel.json`, mode rule field
  `force_weapon_set_for_all_actors`:
  - Before: `"force_weapon_set_for_all_actors": false,`
  - After: `"force_weapon_set_for_all_actors": true,`
- Kept `weapon_set_id: 3`. `config/weaponsets.json` confirms set 3 is named
  `Rocket Launcher` and contains `rocket_launcher` plus `nothing` (the empty
  slot; no other weapon).
- The mode's RED/BLUE roles are Duel-specific and omit a role-level
  `weapon_set`; forcing the mode set also prevents actor presets and explicit
  host selection from replacing it for players or NPCs.

## Basis

Read `docs/ROUTER.md`, `docs/specs/gamemodes/duel.md`,
`docs/specs/gamemodes/gamemodes.md`,
`docs/specs/actor-presets/actor-presets.md`,
`docs/specs/npc-behavior/npc-behavior.md`,
`docs/specs/weapons/weapons.md`, `docs/skills/spec-behavior-review-v1.md`,
`docs/regressions/README.md`,
`docs/regressions/gameplay/duel-npc-shared-REG-2026-10-26.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/operations/task-completion/task-completion.md`, and
`docs/workflows/runtime-scenario-validation.md`.

The Duel specification says both Rocket Duel competitors have only set 3 and
that mode roles must not supply another weapon set. The active JSON selected
set 3 but disabled the mode's force-all-actors rule. The server resolver uses
that rule to make the mode set authoritative across actor roles/presets and
host overrides. This is a configuration-owner correction; no code changed.

## Evidence and remaining review

- Source/config evidence: inspected the active mode JSON, shared weapon-set
  definition, role definitions, and server resolver. The requested invariant is
  represented by the corrected JSON setting.
- Validation: reviewed the focused diff. No build or test was run, and no live
  dev-loop run or `events.jsonl` evidence was collected in this session.
- Human gameplay acceptance remains open: launch Rocket Duel, verify both the
  human and NPC resolve to set 3 with only Rocket Launcher and Nothing in their
  loadout, then observe the NPC firing the Rocket Launcher during ACTIVE play.
