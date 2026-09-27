# Specification update: freeze, generic audio, actor presets, and frame JSONL

Date: 2026-09-20
Status: specification updated; implementation not changed

## Changed

- Freeze now suppresses horizontal and vertical velocity while active.
- Freeze strength decays exponentially over 300 fixed ticks (5 seconds at
  60 Hz), with `exp(-5.0f * elapsedTicks / 300.0f)` as the initial hot policy.
- Gameplay actions emit generic presentation events. Actions do not call
  sound-specific functions. One generic audio presenter resolves asset,
  volume, pitch, speed, offset, looping, duration, spatial, and overlap data.
- Actor movement presets use stable IDs and one shared resolver, allowing JSON
  value tables and hot C++ formulas without creating thousands of files or
  separate movement implementations.
- Client/server generations, hashes, switch state, and position provenance are
  required in networked JSONL diagnostics.
- Frame timing is measured every frame and summarized once per 60 ticks with
  contributors sorted from greatest to least cost.

## Evidence and limits

- `git diff --check` was run; an unrelated pre-existing trailing-whitespace
  warning remains in `docs/specs/weapons/weapons.md`.
- No gameplay source, audio source, runtime logger, or build was changed.
- Runtime proof that the current implementation satisfies these updated rules
  remains pending.
