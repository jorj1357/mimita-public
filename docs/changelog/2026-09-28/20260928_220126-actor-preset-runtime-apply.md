// 2026-09-28T22:01:26Z
/* purpose
* record runtime actor-preset loading, apply, and reset correction
* does NOT claim live visual or multiplayer acceptance
*/

# Task

- Summary: Make actor presets load reliably, make `actor_preset N` apply a
  reversible runtime layer, and add `actor_preset_reset`.
- Status: CODE_COMPLETE / JSON_VALIDATED / BUILD_VERIFIED /
  HUMAN_RUNTIME_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T22:01:26Z, ISO 8601 UTC; display timezone
  America/New_York.

# Changes

- `src/gamemode/match-roles.cpp`: actor-preset loading now resolves the
  relative directory from the current working directory or executable-adjacent
  repository locations, reports the exact missing path/cwd, and can be retried
  from the command path.
- `src/network/community-match-client.*`: added an in-memory actor-preset
  layer. Applying a preset snapshots FOV, player FOV, perspective, avatar, and
  movement config; reset restores those exact values. No JSON is written.
- `src/config/movement-config.*`: added runtime-only replacement of the active
  movement values, preserving the selector and movement preset files.
- `src/terminal/actor-commands.cpp`: `actor_preset_list` and numeric/string
  selection retry preset loading; `actor_preset N` now applies the selected
  local runtime layer; `actor_preset_reset` restores the saved settings.

# Validation

- JSON validation passed for the actor preset, Counter-Strike gamemode, and
  roles configuration.
- `python build.py build-only` completed with `BUILD SUCCESS`.
- Focused diff whitespace validation passed; line-ending warnings are existing
  repository normalization behavior.

# Runtime review still required

- Restart the running build so the new code is loaded, then run:
  `actor_preset_list`, `actor_preset 1`, `actor_preset_current`, and
  `actor_preset_reset`.
- Confirm the list includes `[1] counter_strike`, FOV changes to 70, first
  person is forced, movement changes to `counterstrike`, and reset restores the
  prior FOV/perspective/movement/avatar.
- Weapon inventory remains server-authoritative: Counter-Strike gamemode
  actor spawning applies `counterstrike_rifles`; a client-only command cannot
  bypass that authority. Verify the weapon set in a live host/match.
- The screenshot's prior `no presets loaded` state was not live-retested after
  the correction.

# Routed documents and focused skills

- `docs/ROUTER.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
