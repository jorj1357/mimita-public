# Extreme camera lens distortion controls

- EST timestamp: 2026-10-02 21:26:59
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Focused skill: `docs/skills/spec-behavior-review-v1.md`

## Change

Removed the old `0..100` behavior ceiling for `lensDistortion`. The camera
config now accepts values up to `10000`, so values such as `359` reach the
shader as stronger distortion instead of being flattened at 100.

Added hot-reloadable controls in `config/camconfig.json`:

- `lensDistortionCurve`: where the warp concentrates from center to edge.
- `lensDistortionZoom`: post-lens image magnification.
- `lensDistortionEdgeMode`: `black`, `vignette`, `clamp`, `repeat`, or `circle`.
- `lensDistortionEdgeRadius`: start of the circular edge treatment.
- `lensDistortionEdgeSoftness`: width of the soft transition.
- `lensDistortionEdgeDarkness`: edge darkening amount.
- `lensDistortionPeripheralBlur`: outer-lens blur amount.

The camera config remains the source of truth; the setup loop forwards all
values into the existing fullscreen PostFX shader. `shaders/post.frag` now
uses the curve and zoom during the warp, supports selectable edge handling,
and keeps the center clearer than the periphery.

## Validation

- `git diff --check`: passed.
- Safe timestamped build:
  `MIMITA_FORCE_LINK=1 MIMITA_EXE_NAME=mimita-20261002-extreme-lens.exe
  python build.py build-only` — `BUILD SUCCESS`; executable produced at
  `C:\mimita-v9\mimita-20261002-extreme-lens.exe`.
- The running fixed-name `mimita.exe` was left untouched.
- No independent GLSL validator is installed; rendered visual review remains
  required.

Pre-existing unrelated modifications in `config/accounts/default.json`,
`config/analytics.json`, and the untracked `docs/specs/20261002plan.md` were
preserved.
