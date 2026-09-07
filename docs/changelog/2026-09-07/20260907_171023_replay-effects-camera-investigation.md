# Replay Camera and Effects Investigation Update — 2026-09-07 13:10 EST

## Scope

Investigation only. Inspected the recent replay regression updates, the replay
and effects specifications, the user-provided successful export, its replay
JSON, export logs, and current source paths. No gameplay or replay source code
was changed.

## User-provided artifact

The user reported that this export has a working camera:
`C:\mimita-priv-v8\replays\exports\09-07-2026\13-02-22-clip-duel.mp4`.
The corresponding source replay inspected was
`replays\\09-07-2026\\13-02-22-replay.json`.

## Findings

1. Camera: the replay JSON has 900 scene frames. Its first camera position is
   `(4.7011, -24.4159, 87.6256)` and its last is `(6.4519, -26.9685,
   91.7210)`. The later successful export therefore captured a moving camera.
   The earlier `(0,0,0)` behavior is intermittent/clip-specific and remains
   unresolved as a capture initialization problem.
2. Projectile events: the JSON contains 8 `projectile_spawn` events at ticks
   47, 103, 598, 662, 712, 753, 806, and 856. It does not contain a spawn
   event on every tick.
3. Projectile replay: `ReplayExport_log_130223.txt` shows the tick-47 spawn
   being processed repeatedly at the same original position. The current
   playback branch at `src/engine/engine-tick-camera.cpp:1032-1051` creates a
   new `EffectPart` for every received spawn event, so duplicate delivery
   produces duplicate rockets from one historical origin.
4. Killfeed: the JSON contains three events at ticks 75, 821, and 822, all
   with unknown killer and weapon and victim `admin`. The export log shows
   repeated killfeed chat lines. This proves the saved killfeed data is not a
   clean single historical kill; it does not yet prove that one JSON event is
   independently appended five times.
5. Left leg: the existing quaternion-hemisphere change only addresses `q` vs
   `-q` sign ambiguity. The persistent left-leg-only flat orientation remains
   localized to a likely left-leg transform/basis or local/world conversion
   issue, not a general Z-up failure. The exact failing stage still needs
   comparison of recorded quaternion, JSON quaternion, `applyReplayPose()`,
   and renderer bone basis.

## Regression update

Appended an evidence update to the existing unresolved regression in
`docs/regressions/regressions-v1.md` rather than creating a duplicate issue.
The record now separates camera intermittency, projectile duplicate delivery,
killfeed data/delivery, and left-leg orientation.

## Documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/replays/replay-editor-and-export-v2.md`
- `docs/specs/effects/effects.md`
- `docs/regressions/regressions-v1.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW
- `docs/skills/logging-checker-v1.md`: PASS_WITH_HUMAN_REVIEW

## Validation and remaining work

- Read-only JSON statistics and targeted log searches completed.
- No build was required because no code changed.
- `git diff --check` reports only the pre-existing trailing-whitespace note in
  the informal login text at `docs/regressions/regressions-v1.md:387`.
- Next technical proof: correlate one projectile's event identity through
  `ReplayPlayer::update()`, `takeTriggeredEffects()`, and each effect spawn;
  correlate the killfeed events through `takeTriggeredKillfeedEvents()` and
  chat append; then compare left/right leg transforms through recording,
  loading, pose application, and rendering.

