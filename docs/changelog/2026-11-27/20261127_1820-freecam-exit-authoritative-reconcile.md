# Freecam exit installs the authoritative transform

Written 2026-11-27 by `rocket-league-input-fix` for
`issue-0c39edc4f9b8` (human report: leaving freecam after taking many hits
leaves the local position stale and the server still resolves hits at the
freecam entry position).

## Source change

- `src/engine/engine-tick-replay.cpp`: on the `FREECAM_ENABLED` true -> false
  transition (gameplay freecam, not replay playback), call
  `MimitaNet::mpApplyAuthoritativeTransform` before the fixed-tick simulation
  loop. Guarded by `mpContext.active && connected && hasLocalServerPosition`
  and no pending authoritative spawn.
- The transform install is the single existing owner for position, velocity,
  yaw, ground, and external-impulse clear, so the client no longer resumes from
  and re-reports the frozen entry pose, and the damage impulse backlog is
  bounded to one reconcile instead of being replayed at once.
- Added bounded diagnostics: `freecam.enter` and `freecam.exit.reconciled`
  carry local/camera or frozen/authoritative positions, `position_error_m`,
  `pending_victim_health`, `pending_knockbacks`, discarded external impulse,
  `server_tick`, and `server_epoch`.

## Evidence

- Build: game generation 801, `game-g801.dll` sha256 `c3d0af07...`, EXE build
  `2026-11-27T17:45:19.700118Z`.
- Runtime: isolated TDM run `20261127_180429`, client journal
  `logs/2026-11-27/20261127_180429/client-01/events.jsonl`.
- `freecam.enter` tick 1450; `freecam.exit.reconciled` tick 1471 with
  `position_error_m=0.858`.
- Same-epoch clean exit `freecam.exit.reconciled` tick 23814 (`server_epoch` 8
  unchanged): frozen `(12.513,-116.724,286.293)` vs authoritative
  `(12.613,-116.689,286.288)`, `position_error_m=0.106`, zero pending
  victim-health/knockback.
- `proof record issue-0c39edc4f9b8` -> `proven` (`freecam.exit.reconciled`).

## Caveats

- NPC fire killed the client repeatedly in the test map, so the exact
  "hurt but alive with a large backlog" human scenario was not reproduced
  end-to-end; the bounded catch-up is the external-impulse clear inside
  `mpApplyAuthoritativeTransform`.
- The optional rare lightning/effect presentation for a saturated backlog
  remains unclaimed; the acceptance also names human visual confirmation.
