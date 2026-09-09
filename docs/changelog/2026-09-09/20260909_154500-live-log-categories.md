# Live log category controls

Date: 2026-09-09

## Changes

- Added structured logger categories for `healthbar`, `skybox`, and
  `chat_layout`.
- Added live JSON configuration entries for those categories in
  `config/debuglogger.json`, all disabled by default with file output disabled.
- Gated replay healthbar diagnostics, skybox per-frame render diagnostics, and
  chat layout diagnostics through the live structured logger settings.

## Validation

- `config/debuglogger.json` parsed successfully.
- Modified source files compiled through the canonical build.
- Final executable linking was blocked because `mimita.exe` was locked by
  another process (`Permission denied`).
- The broader migration of every legacy raw-print diagnostic remains pending;
  this change covers the three newly requested categories.
