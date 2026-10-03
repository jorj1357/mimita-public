# Camera-configured body-cam lens distortion

- EST timestamp: 2026-10-02 21:10:37
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`
- Related specification: `docs/specs/visuals/visuals.md`

## Change

Added `lensDistortion` to `config/camconfig.json` with a documented `0..100`
scale: `0.0` is neutral, approximately `30.0` is noticeable body-cam
fisheye, and `100.0` is extreme.

`CameraConfigData` now owns and validates the value in
`src/config/camera-config.h` and `src/config/camera-config.cpp`. The setup
loop forwards the camera-owned value to the existing fullscreen PostFX pass
through `PostFX::setCameraLensDistortion`.

`shaders/post.frag` now applies aspect-correct radial barrel distortion so the
effect is circular instead of stretched by widescreen aspect ratios. The
existing clamp-to-edge texture sampling keeps the entire viewport filled and
does not create black bars.

## Validation

- `git diff --check`: passed.
- Safe timestamped build:
  `MIMITA_FORCE_LINK=1 MIMITA_EXE_NAME=mimita-20261002-lens-distortion.exe
  python build.py build-only` — `BUILD SUCCESS`, executable produced at
  `C:\mimita-v9\mimita-20261002-lens-distortion.exe`.
- The first fixed-name link attempt was blocked because an accidentally
  launched existing `mimita.exe` held its file lock. That process was left
  untouched; the timestamped build succeeded.
- No GLSL validator is installed, so shader syntax was not independently
  validated outside the runtime shader loader.

## Human review required

Run the timestamped build and compare `lensDistortion` values `0`, `30`, and
`100` while looking at screen edges and corners. Confirm the intended body-cam
look, no black bars, and acceptable edge stretching at the extreme setting.

Pre-existing unrelated modifications in `config/accounts/default.json` and
`config/analytics.json` were preserved.
