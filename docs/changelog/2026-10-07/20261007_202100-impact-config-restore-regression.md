# Prevent gamemode backup from restoring stale impact decals

Date: 2026-10-07
Branch: afad20a-rebuild
Result: PASS_WITH_HUMAN_REVIEW

## Cause

`git-push-v3.py` only selects a branch, stages files, commits, and pushes. It
does not copy or reset `config/impact_decals.json`. The live game was doing
the overwrite: `CommunityMatchClient` backed up the entire impact-decals JSON
when a gamemode visual override started, then restored that old snapshot when
the match reset. A game session that started before the new impact settings
could therefore restore the old JSON after the settings had been committed.

## Fix

Removed `config/impact_decals.json` from `SettingsBackup`'s whole-file backup
list. Blood enablement is a runtime override, so restoring the entire file is
unnecessary and can destroy user-authored distance, debris, and client-feedback
settings. Camera and ragdoll backups remain unchanged.

Restored the impact configuration with blood debris, client feedback, blood
render distances, bullet-hole render distances, and world-crack render
distances. The three distance groups use `1250.0 / 1000.0 / 1250.0` for max,
fade-start, and fade-end.

## Validation

- JSON parsing succeeded.
- Confirmed debris and client feedback are enabled.
- `git diff --check` passed for the changed files.
- Forced the changed `settings-backup.cpp` translation unit to compile after
  the incremental build initially skipped it.
- Canonical build completed successfully: `Compiled: 1`, `Skipped: 548`,
  return code `0`.

## Human review still needed

Run the newly built client through a gamemode start and match reset, then
confirm `config/impact_decals.json` remains unchanged and the restored effects
are still present.
