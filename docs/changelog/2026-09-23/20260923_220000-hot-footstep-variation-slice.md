# Hot footstep variation slice

Date: 2026-09-23
Status: hot DLL built; default cadence/variation corrected; leg-contact trigger remains follow-up work

## Change

- `src/hot-reload/modules/presentation/effect-composition.cpp` now chooses the
  walk sound and applies volume/pitch variation from a deterministic actor/tick
  mix. The active hot audio policy owns the variation; the cold audio bridge
  still only plays the resulting command.
- Removed the second `effect.footstep.sound` dispatch from `Player::updateAudio`.
  The single `effect.movement.footstep` event now owns the sound and VFX.
- The default hot C++ policy now uses an afad20a-style random walk variant per
  existing 0.35-second cadence, preventing immediate repeats while retaining
  volume/pitch variation.
- The legacy `effect.footstep.sound` compatibility request is now consumed
  silently; only `effect.movement.footstep` emits the walk audio. Immediate
  random repeats are allowed.

## Evidence

- `git diff --check`: passed.
- `python build_game_dll.py`: `DLL build success`.

## Remaining

- The current trigger is still the legacy `Player::footstepTimer` cadence.
- Left/right animation contact events are not yet exposed by the hot animation
  state, so this slice does not claim “one sound per foot-ground contact.”
- No live human acceptance was performed.
