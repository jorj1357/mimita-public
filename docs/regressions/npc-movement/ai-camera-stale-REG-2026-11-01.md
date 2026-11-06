# AI body aimed at enemies while the camera stayed fixed

Time created: 2026-11-01T22:55:00Z
Time last updated: 2026-11-01T22:55:00Z

Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PASS

Related specification: `docs/specs/dev-loop/dev-loop.md`,
`docs/architecture/terminal-commands/terminal-commands.md`

## Observed

In live AI mode the player body/arms sometimes faced an enemy but the camera
kept the old direction.

## Cause

`PlayerAiController` wrote `lookYaw`, `lookPitch`, and `camForward` into
`InputFrame` and the network packet, but never updated `THE_CAMERA`.
`engineTickCamera()` rebuilt `camera.front` from the unchanged yaw/pitch and
`syncPlayerYawFromCamera()` pulled the body back to that stale camera.

## Correction

AI now applies a smooth bounded yaw/pitch turn to the real camera before fixed
simulation, then uses the camera yaw/pitch/front for the frame. Body, arms,
camera, and server input agree.

## Evidence

Build 185/187 `player.ai.state` records `camera_body_error_deg`; live run
`logs/2026-11-01/20261101_224843/events.jsonl` showed a maximum 0.56 degrees and
settled at 0.0. Human visual confirmation remains required.
