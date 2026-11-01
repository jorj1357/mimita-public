# Weapon hot reload re-scanned the animation library per melee action

Time created: 2026-11-01T22:55:00Z
Time last updated: 2026-11-01T22:55:00Z

Status: IMPLEMENTED IN SOURCE; FREEZE REDUCED; LONG-RUN VISUAL PENDING

Related specification: `docs/specs/weapons/weapons.md`,
`docs/specs/debug-logging/debug-logging.md`

## Observed

Live play froze for roughly one to two seconds around weapon configuration
loading.

## Cause

Weapon hot reload parsed every weapon JSON, then every melee action re-read and
re-parsed the entire 115-file animation library to prove clip-id uniqueness.
Move sets were parsed twice, and `WeaponConfig` re-read the same weapon root.

## Correction

Added a per-generation clip timeline index (`clipId -> timeline`) with duplicate
ids invalid, removed the duplicate move-set parse, reused the parsed weapon root
for viewmodel config, skipped unchanged move-set publishes/events, and added
`Config::WeaponDefinitionRead` / `Config::MeleeClipIndexBuild` timing scopes.

## Evidence

Build 185/187 `weapon.melee.moveset_loaded` records appear during startup and
unchanged hashes no longer republish. A clean long-run freeze measurement
remains required; if the remaining reload still exceeds frame budget, move the
parse to a background snapshot applied at the next safe tick.
