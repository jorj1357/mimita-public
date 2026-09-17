# Client animation state advances on fixed ticks

Date: 2026-09-17

## Outcome

The hot player animation state machine now runs in the client
`postmovement.60` domain instead of the render domain. Its playback and
locomotion clocks therefore advance once per fixed client simulation tick with
`1/60` timing. Pose generation remains render-domain code, so rendering can
still display the latest animation smoothly at any frame rate.

## Evidence

- Changed `src/hot-reload/modules/presentation/animation-policy.cpp` to
  register `hot.animation-policy` in `GAME_DOMAIN_POST_MOVEMENT`.
- `python devscripts/live-build.py --generation 1000001` succeeded.
- The live build did not write or relink `mimita.exe`.
- `git diff --check` passed for the changed source.

## Remaining acceptance

Activate generation `1000001` in the running client and compare animation
phase at different render FPS values while holding the same movement input.
Axis/sign tuning remains in `src/hot-reload/hot-animation-clips.h` and is also
live-editable.
