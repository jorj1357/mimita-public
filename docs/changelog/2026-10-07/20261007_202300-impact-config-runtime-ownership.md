# Keep impact-decals JSON persistent during gamemode overrides

Date: 2026-10-07
Branch: afad20a-rebuild
Result: PASS_WITH_HUMAN_REVIEW

## Scope

Clarified the ownership boundary between persistent impact-decals JSON and
Counter-Strike runtime blood enablement.

## Fix

- `config/impact_decals.json` is no longer included in the gamemode's
  whole-file backup/restore list.
- Counter-Strike blood enablement remains an in-memory override only.
- If `impact_decals.json` is hot-reloaded during a match, the active runtime
  blood override is reapplied without discarding the other edited settings.
- When the match ends, the loader reads the current JSON value again instead
  of restoring a boolean captured from an older match-start snapshot.

This keeps JSON as the persistent source of truth for effect tuning while
letting the gamemode temporarily control only the one field it owns.

## Validation

- JSON parsing and impact-distance checks passed.
- `git diff --check` passed.
- Canonical build succeeded with `Compiled: 10`, return code `0`.

## Human review still needed

Edit an impact-decals value while a Counter-Strike match is active, trigger a
config reload, end/reset the match, and confirm the edit remains in the file
and is used after the runtime blood override clears.
