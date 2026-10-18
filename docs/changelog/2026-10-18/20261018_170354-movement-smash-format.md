# Movement Smash config format merge

- UTC: `2026-10-18T17:03:54Z`
- Display: `2026-10-18 13:03:54 EDT` (`America/New_York`)
- Branch: `20261011zt`
- HEAD before this edit: `3a89b2de`
- Result: `PASS` for the JSON-with-comments structure and field merge; no live movement acceptance performed.

## Change

- Merged `config/movement-smash copy.json 2026 10 18 .bak` into the already
  modified `config/movement/movement-smash.json` as a formatting and schema
  reference. The dated copy was not edited.
- Kept the active file's existing values, comment field, and purpose header.
  Added all 30 fields that were present in the dated copy but missing from the
  active file, plus its documented comments. Kept all 32 active-only fields.
  The merged file now has 82 unique top-level fields.
- Example additions: `source_units_per_mimita_unit` is now at line 51;
  `jump_input_mode` is at line 189. Before the edit, neither field existed in
  the active file. Existing `ground_acceleration` remains `55.0` at line 68.
- The active file had `jump_strength: 18.0`; the reference's `jump_velocity`
  alias is parsed later and would override that value. Added
  `jump_velocity: 18.0` at line 145 so the new alias does not silently change
  the existing jump strength.
- The dated copy had `speed_limit_enabled: true` even though its nearby comments
  say the global limit is disabled. `applySpeedLimitClamp()` applies this cap
  regardless of movement mode, so the merged file uses `false` at line 320 to
  match the comments and avoid introducing an unintended 50-unit global cap.
- Rewrote the copied `tuning_notes.comment` to describe the merged file and
  note that `movement_mode` remains `mimita`. The reference's numeric tuning
  comments remain as reference notes, as called out near the top of the file.

## Starting state and other edits

- `config/movement/movement-smash.json` was already modified before this
  request; it contained the active MiMITA values and a 66-line format. Those
  values were retained, with the two noted compatibility corrections/additions.
- Other pre-existing changes were left untouched in
  `assets/avatars/captainfalcon/animations.json`, `config/analytics.json`,
  `config/audio/music-settings.json`, `config/command-aliases.json`,
  `config/movement/movement-default.json`, `config/server-presets.json`,
  `config/weapons.json`, `devscripts/dev-loop.py`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/specs/gamemodes/supersmashbrosmelee.md`, and
  `src/network/server-melee-actions.cpp`. Existing untracked changelog and
  regression records were also left untouched.

## Review and validation

- Reviewed `docs/ROUTER.md`, `docs/architecture/json-configuration/json-configuration.md`,
  `docs/specs/movement/movement.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/skills/spec-behavior-review-v1.md`.
- The Melee gamemode selects movement preset `smash`. The shared movement
  parser reads these fields, and the global speed limit is applied after each
  movement step regardless of mode. No code was changed.
- Parsed the final JSON-with-comments file, confirmed there are 82 unique root
  fields with no duplicate keys, and confirmed the target contains the union
  of the active and dated-copy fields. `git diff --check` passed for the target.
- No build or game run was performed. The merged file and its comments are
  validated as data; live reload and movement feel still need in-game review.
