# Melee NPC attack interval range

- Time: 2026-10-21T03:35:06Z (2026-10-20 23:35:06 EDT)
- Branch: `20261011zt`
- HEAD: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

Made the active `melee_aggressive` behavior profile choose an NPC attack interval from inclusive configurable endpoints. Both endpoints are clamped to 1–30 ticks. If a profile still has the legacy fixed `melee_attack_gap_ticks`, it is used for both endpoints. If only one new endpoint is valid, it supplies both; a maximum below the minimum is raised to the minimum. The selected interval is written to the existing `melee.action.start` event as `attack_interval_ticks`.

The new default values are 10 and 30 ticks, as requested. A profile reload is read at the next attack decision; already selected attack intervals remain in effect until the next attack begins.

## Changed files and exact edits

### `config/behavior-profiles.json:669-670`

Before:

```json
"melee_attack_gap_ticks": 2,
```

After:

```json
"melee_attack_gap_min_ticks": 10,
"melee_attack_gap_max_ticks": 30,
```

The working tree already had other profile edits before this task, including `preferred_range` and move weights. Those were preserved.

### `src/npc/npc-behavior.h:41-43`

Before:

```cpp
int meleeAttackGapTicks = -1;
```

After:

```cpp
int meleeAttackGapTicks = -1; // legacy fixed interval
int meleeAttackGapMinTicks = -1;
int meleeAttackGapMaxTicks = -1;
```

### `src/npc/npc-behavior.cpp:74-96`

Before, the parser exposed only `melee_attack_gap_ticks` as one fixed interval.

After, it reads and clamps `melee_attack_gap_min_ticks` and `melee_attack_gap_max_ticks`, retains the old field as a compatibility fallback, and normalizes a missing or reversed endpoint as described above.

### `src/network/server-melee-actions.cpp:1197-1200,1244-1249`

Before, the decision used one configured `attackGapTicks` value.

After, the server resolves the min/max endpoints and chooses one interval for the attack from the actor ID and current fixed tick. The existing action-start event records the chosen `attack_interval_ticks`.

### `docs/specs/gamemodes/supersmashbrosmelee.md:237`

Before:

```text
The profile exposes ... `melee_attack_gap_ticks` ...
```

After: the requirement names both endpoint fields, defines inclusive 1–30 tick selection, compatibility behavior, and the 10–30 default profile range.

## Pre-existing working tree changes

The repository had many unrelated edits before this task, including changes in Melee actions, NPC behavior, gamemode presentation, movement, and several configs/specs. No unrelated changes were reverted. The targeted `git diff --check` passed; the repository-wide check reports pre-existing trailing whitespace in `docs/specs/gamemodes/juggernaut.md`.

## Validation and evidence

- Comment-aware config load with `devscripts/jsonc.py`: passed; active profile reports min `10`, max `30`.
- Targeted `git diff --check`: passed.
- Canonical dev-loop build request: failed before compilation with `WinError 5`, denied while replacing `.dev/current-run.json.tmp` with `.dev/current-run.json`. The dev-loop status showed the existing build 2722 Melee run active; it was left running.
- Runtime proof: not available because the new source did not produce a fresh executable. The old running executable was not used as proof. The next verification is to resolve the dev-loop state-file access error, build the changed source, then observe `melee.action.start.attack_interval_ticks` over several attacks in the live journal.
- Human acceptance: pending; confirm the NPC attacks between 10 and 30 ticks apart and that editing/reloading the JSON changes subsequent intervals.

## Documents and focused review

Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md`, and `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, `docs/skills/documentation-checker-v1.md`.

- Specification and behavior review: `PASS_WITH_HUMAN_REVIEW`; old fixed-interval language was updated to the new range contract.
- Logging review: existing `melee.action.start` event already records the selected interval; runtime evidence remains pending.
- Documentation review: changed spec wording matches the parser and attack decision behavior by source inspection.
