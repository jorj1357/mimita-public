# Long-range hit FX tuning

- Status: `PASS_WITH_HUMAN_REVIEW`
- Timestamp: `2026-10-07 13:48:14 -04:00` (America/New_York)
- Branch: `afad20a-rebuild`
- Scope: Move blood and tracer distance fade/culling to explicit JSON values.

## Changes

- `config/impact_decals.json`, active `blood` group:
  - `renderDistance`: `1250.0`
  - `renderFadeStartDistance`: `1000.0`
  - `renderFadeEndDistance`: `1250.0`
- `config/weapon-tracers.json`, active `defaults` group:
  - `renderDistance`: `1250.0`
  - `renderFadeStartDistance`: `1000.0`
  - `renderFadeEndDistance`: `1250.0`
- `src/config/weapon-tracers-config.h/.cpp` now own and load the tracer
  distance policy with the same defaults.
- `src/effects/effect-part.cpp` stores the tracer weapon ID on the effect.
- `src/effects/effect-part-render.cpp` uses the tracer JSON policy instead of
  the generic hard-coded 40 m cull for tracer effects. The existing blood
  renderer already used the blood JSON policy; the missing active values are
  now explicit in `impact_decals.json`.

## Validation

- JSON/config structure was inspected; the repository accepts JSON comments in
  these active configuration files.
- `git diff --check` passed for all files changed by this session.
- Forced canonical build compiled 3 affected files and linked successfully:
  `BUILD SUCCESS`, `Compiled: 3`, `Skipped: 546`.
- `mimita.exe --versioninfo` passed and wrote
  `logs/10-07-2026/20261007_134809/events.jsonl`.
- Live Counter-Strike visual acceptance remains pending.

## Pre-existing work preserved

Unrelated worktree changes in configuration, NPC/server code, weapon/UI code,
diagnostic logging, planning documents, and earlier changelogs were preserved.

