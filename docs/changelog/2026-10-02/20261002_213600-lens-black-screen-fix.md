# Lens distortion black-screen safety fix

- EST timestamp: 2026-10-02 21:36:00
- UTC timestamp: 2026-10-03T01:36:00Z
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Finding

- Severity: high
- Type: observed post-process regression
- Specification: camera lens distortion is a presentation effect controlled by
  `config/camconfig.json`; enabling it must not remove the rendered scene.
- Code path: `PostFX::bindFBO()` renders the world into `mColorTex`, then
  `PostFX::render()` executes `shaders/post.frag` and samples that texture
  using `applyLensDistortion()`.
- Actual behavior: with a nonzero lens value, the fullscreen pass could receive
  invalid or driver-dependent out-of-range sample coordinates and replace the
  scene with black pixels; the HUD remained visible because it is drawn after
  the post-process pass.
- Expected behavior: valid pixels remain visible at every nonzero strength;
  extreme values may stretch or repeat the image but must not blank the frame.
- Evidence: user screenshots show the HUD/UI present while the 3D scene is
  black with lens distortion enabled and visible again when it is disabled.

## Fix

`shaders/post.frag` now:

- bounds the GPU-side strength used for arithmetic while keeping the JSON
  setting uncapped for user-facing tuning;
- detects NaN/absurd warped coordinates and falls back to the current pixel;
- falls back to the original rendered pixel when a warped sample resolves to
  black even though the source pixel is non-black.

This keeps the existing camera JSON controls and edge modes unchanged.

## Validation

- `git diff --check`: passed.
- Timestamped build:
  `MIMITA_FORCE_LINK=1 MIMITA_EXE_NAME=mimita-20261002T213500-lens-black-fix.exe`
  `python build.py build-only`: `BUILD SUCCESS`; executable linked at
  `C:\mimita-v9\mimita-20261002T213500-lens-black-fix.exe`.
- The existing fixed-name `mimita.exe` was not stopped, replaced, or unlocked.
- A separate GLSL compiler is not installed, and the desktop computer-control
  surface did not expose a targetable game window in this session. Visual
  runtime acceptance with a nonzero value remains required.

## Preserved work

The earlier lens-distortion implementation and user-tuned camera values were
preserved. Unrelated existing changes in `config/accounts/default.json`,
`config/analytics.json`, `config/audio/music-settings.json`, and the untracked
`docs/specs/20261002plan.md` were not edited.
