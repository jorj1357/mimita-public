# v2.0.6 tool animation migration

## Change

Migrated the existing hot C++ tool phase tables into reusable JSON phase sets in
`config/animations.json`. Canonical revolver, shotgun, rocket launcher,
grenade launcher, swordsword, and spyknife phases are represented in JSON;
other tools reference those sets. The hot loader accepts `phaseSet` references,
keeps C++ as the default, and falls back per tool when JSON data is absent or
invalid.

## Evidence

- `config/animations.json` parses successfully.
- Hot build succeeded as `build/hotreload/mimita-live-g20260921.dll`.
- No executable or cold runtime was rebuilt.
- Visual v2.0.6 parity is not claimed because `mimita.exe` is absent and no
  human playtest was possible.

## Remaining discrepancy

Player locomotion/aim-body animation and weapon hitbox authority still need
separate owner traces. This migration only moves tool animation phase data.
