# Projectile rifle visible history trail

Date: 2026-09-29 20:05:48 EDT
Branch: current working branch

## Scope

Implemented the client-side projectile-rifle visibility and fixed-tick history trail requested in this session. Existing unrelated working-tree edits were preserved.

## Changes

- `src/network/multiplayer-context.h`: `NetworkProjectile` now owns a bounded client-only `trailHistory` of position samples and its sampling accumulator.
- `src/network/multiplayer-projectiles.cpp`: records rifle positions at the 60 Hz gameplay cadence and renders historical copies behind the current projectile. Each copy shrinks, darkens, and fades according to weapon JSON values. The current rifle projectile remains rendered through the shared projectile renderer.
- `config/weapons.json`: added `projectileTrailHistoryTicks`, `projectileTrailHistoryRenderSamples`, `projectileTrailHistorySizeMultiplier`, `projectileTrailHistoryDarkenMultiplier`, and `projectileTrailHistoryAlphaMultiplier` to `projectile_rifle`.

## Evidence

- `config/weapons.json` parsed successfully with Python's JSON parser.
- `git diff --check` passed; only existing line-ending warnings were reported.
- `python build_game_dll.py` returned `[HOT RELOAD] DLL up to date, skipping`; this slice is outside the active hot DLL boundary.
- Two `mimita.exe` processes were already running. No process was stopped, restarted, or relinked.

## Human review still required

An intentional cold build is required before this C++ slice can enter the running executable. After launching the resulting timestamped build, verify that the rifle projectile is visible and that changing the five history-trail JSON values changes the live trail pattern. This session did not claim visual acceptance.
