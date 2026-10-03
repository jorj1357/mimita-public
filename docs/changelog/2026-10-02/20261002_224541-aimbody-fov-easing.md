# Aim-body RMB FOV easing

Time: 2026-10-03T02:45:41Z

## Requested result

Holding the right mouse button now has an optional, smooth FOV transition owned by `aimbody.json`. Releasing RMB returns the FOV over the same configured duration. The transition uses the normal `camconfig.json` FOV as its baseline and only applies during the normal player camera, not replay playback or the replay editor.

## Configuration

`config/aimbody.json` now contains:

```json
"fov": {
  "enabled": true,
  "multiplier": 0.5,
  "duration": 0.5,
  "easing": "ease_in_out"
}
```

Supported easing values are `linear`, `ease_in`, `ease_out`, `ease_in_out`, `exponential`, and `bounce`. The multiplier is clamped to `0.05` through `1.0`, and duration is clamped to `0.01` through `10.0` seconds. Aim-body hot reload picks up changes through the existing reload path.

## Evidence

- Source: `AimBodyConfig` parses, stores, saves, and evaluates the new FOV settings.
- Source: `engineTickCamera` applies the eased multiplier after camera-mode selection and advances the blend on both press and release.
- Focused build: `src/entities/aimbody-config.cpp` compiled successfully; the existing camera object was current and skipped by the incremental build.
- Repository hygiene: `git diff --check` reported no whitespace errors.
- Full build: BLOCKED by unrelated pre-existing errors in `src/network/community-match-client.h`, where `objective()` references undeclared `mObjective`. No executable or runtime/human acceptance was claimed.
