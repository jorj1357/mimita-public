# High-damage number presentation

## Request

Reuse the existing damage-number GUI effect so damage values at or above 200
are pure white and remain visible for 30 client ticks, with the behavior
controlled by JSON.

## Specification review

- Existing owner: `HitEffects::onHit` creates the damage-number request;
  `EffectPartSystem::spawnDamage` creates the pooled billboard effect;
  `effect-part-render.cpp` already renders it.
- New behavior: only the existing critical/high-damage branch changes. Values
  below the configured threshold keep the normal color and lifetime.
- Configuration owner: `config/hitfx.json` owns the threshold, white color, and
  high-damage lifetime. `criticalLifetimeTicks=30` means 30 nominal client
  ticks at 60 Hz, or 0.5 seconds.
- No new GUI system or duplicate damage-number renderer was added.

## Changes

- Added `criticalDamageThreshold` and `criticalLifetimeTicks` to
  `DamageNumberConfig`.
- Added hot-config parsing for both fields.
- Changed high-damage selection from a hard-coded `damage >= 100` to the JSON
  threshold.
- Set the current threshold to 200 and the current high-damage color to
  `[1.0, 1.0, 1.0]`.

## Evidence

- `config/hitfx.json` parsed successfully.
- `git diff --check` passed.
- `python build_agent.py` with the local compiler and GLFW paths: SUCCESS;
  `mimita.exe` linked successfully.
- `mimita.exe --snapshot-chunk-selftest`: PASS.
- A live visual hit was not performed in this session.
