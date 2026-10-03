# Body-camera lens reference tuning

- EST timestamp: 2026-10-02 21:19:36
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Reference: user-provided body-camera image in the request.

## Change

Updated `shaders/post.frag` so the camera-configured `lensDistortion` now
resembles the supplied footage more closely: the center stays clearer, the
periphery receives radial softening, and stronger values produce a soft round
dark lens boundary at the corners. The mask is circular/aspect-correct rather
than a rectangular black bar.

Updated the comments in `config/camconfig.json` to describe the new behavior.
The existing `0..100` camera setting remains the control: `0` is neutral,
around `30` is noticeable, and `100` is extreme.

## Validation

- `git diff --check`: passed.
- Safe timestamped build:
  `MIMITA_FORCE_LINK=1 MIMITA_EXE_NAME=mimita-20261002-bodycam-lens.exe
  python build.py build-only` — `BUILD SUCCESS`; executable produced at
  `C:\mimita-v9\mimita-20261002-bodycam-lens.exe`.
- The running fixed-name `mimita.exe` was left untouched.
- No independent GLSL validator is installed.

## Human review required

Use values `0`, `30`, and `100` in the timestamped build and compare the
corner shape, peripheral blur, and center clarity with the supplied reference.
The exact artistic strength may still need tuning after visual playtesting.

Pre-existing unrelated modifications in `config/accounts/default.json` and
`config/analytics.json` were preserved.
