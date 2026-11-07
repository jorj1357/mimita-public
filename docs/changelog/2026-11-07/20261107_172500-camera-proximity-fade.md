# Camera proximity fade

## Change

- Replaced the informal camera note with a routed camera behavior spec for
  local proximity fading.
- Added `proximityFade.enabled`, `startDistanceMeters`, and
  `endDistanceMeters` to the active camera config with enabled 1 m / 2 m
  defaults.
- Added validation in `CamConfig`: malformed blocks, non-finite distances,
  negative start distance, or an end distance not greater than the start
  reject the candidate and keep the prior settings.
- Applied the linear surface-distance alpha multiplier in the shared scene
  shader, set the camera uniforms for world and actor draws, and enabled/restored
  alpha blending around those rendering paths.

## Evidence

- Source review: base world meshes, streamed world geometry, and posed actor
  meshes use the shared scene shader and now receive the configured fade.
- `git diff --check` passed for the tracked implementation changes before the
  final review.
- The active camera JSONC parsed after comment removal, and the configured
  alpha formula returned 0, 0, 0.5, 1, 1 at 0.5, 1, 1.5, 2, and 3 meters.
- `python devscripts/config_selftest.py` failed on existing avatar schema
  errors across `assets/avatars/*/avatar.json`; it did not pass the repository
  config gate.
- The canonical dev-loop build was requested twice. Both requests failed
  before publishing a new executable. The dev-loop output tail only showed
  skipped sources and `BUILD FAILED`; modified camera and renderer translation
  units had object files produced, but linking was not confirmed.
- No current executable was published or launched for this change. Runtime
  journal evidence and human visual acceptance remain outstanding.

## Limits

- The shared shader covers static and streamed world geometry plus actor body
  geometry. Specialized renderers that do not use this shader, such as separate
  effect or overlay paths, are not covered yet.
- Transparency sorting and visual artifacts have not been checked in a live
  run.
