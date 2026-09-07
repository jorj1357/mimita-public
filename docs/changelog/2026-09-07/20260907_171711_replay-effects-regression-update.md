# Replay Effects and Camera Regression Update — 2026-09-07 13:17 EST

## Scope

Investigation only. Compared the current replay/effects implementation with
`docs/specs/replays/replay-editor-and-export-v2.md` and
`docs/specs/effects/effects.md`, inspected the user-provided replay JSON and
export log, and updated the existing append-only regression record. No gameplay
code was changed.

## Evidence inspected

- Working export: `replays\\exports\\09-07-2026\\13-02-22-clip-duel.mp4`
- Source replay: `replays\\09-07-2026\\13-02-22-replay.json`
- Export log: `logs\\09-07-2026\\ReplayExport_log_130223.txt`
- Current paths: `replay-player-interp.cpp`, `engine-tick-camera.cpp`,
  `weapon-rocket-launcher.cpp`, `replay-player-load.cpp`
- Existing replay regression entries, including the camera-gate change

## Findings

1. The replay specification explicitly covers camera, projectiles, effects,
   kill effects, animations, and replay correctness. The effects specification
   explicitly requires shared live/replay presentation, tick-based lifetimes,
   and `same event + same tick + same seed + same configuration = same visuals`.
2. The later replay JSON has valid moving camera data: 900 scene frames,
   first camera `(4.7011, -24.4159, 87.6256)`, last camera
   `(6.4519, -26.9685, 91.7210)`. This confirms the freecam-gate change can
   produce a working camera, but does not explain the earlier origin clip.
3. The JSON has 8 projectile spawns, but the export log shows the tick-47
   projectile being spawned repeatedly at the same original position. The
   current replay branch creates a new `EffectPart` for each received
   `projectile_spawn`, so duplicate delivery creates intangible duplicate
   rockets rather than one advancing historical projectile.
4. The export path consumes recorded events and calls replay-specific spawn
   branches. This differs from the specification's desired shared normal
   effect path, where replay should reproduce the event through the same
   lifetime/update/fade behavior as live gameplay.
5. The JSON contains three killfeed events at ticks 75, 821, and 822 with
   unknown killer and weapon. The export log shows repeated killfeed chat.
   This is confirmed bad replay data/presentation, but the exact split between
   duplicate recording, cursor reset, and duplicate chat append remains open.
6. The left-leg issue is not covered by a dedicated exact bone-axis contract
   in the replay spec. The replay spec requires player animations and body
   state to reconstruct; the effects spec requires the same replay event/state
   presentation. Current evidence localizes the remaining problem to the
   left-leg transform/basis or local/world conversion, since the right leg is
   correct and the left leg alone remains flat.

## Regression update

Appended an evidence update to the existing unresolved entry in
`docs/regressions/regressions-v1.md` covering missing revolver muzzle sphere,
tracer/beam, non-fading effects, projectile duplication, repeated killfeed,
left-leg orientation, and the later working-camera export.

## Validation

- Read-only source, JSON, and log inspection completed.
- No build was run because no code changed.
- Focused reviews: `docs/skills/spec-behavior-review-v1.md` and
  `docs/skills/logging-checker-v1.md`, both PASS_WITH_HUMAN_REVIEW.
- Remaining proof: trace one effect from live creation through replay event,
  replay consumption, shared effect update, lifetime expiry, and final render;
  trace one projectile and one killfeed event by stable identity; compare left
  and right body-part transforms through recording, JSON load, pose application,
  and rendering.

