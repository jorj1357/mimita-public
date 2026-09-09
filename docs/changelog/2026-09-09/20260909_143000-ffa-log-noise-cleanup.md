# FFA logging cleanup

Date: 2026-09-09

## Changes

- Updated the FFA feature record with the latest logging findings and a
  searchable, event-focused diagnostic naming policy.
- Removed unconditional per-loop `[LOOP TOP]` output from the server.
- Removed unconditional empty-ICE-poll output.
- Removed unconditional per-packet `[NET RX]` output from the client.
- Documented the remaining healthbar, skybox, projectile, and grenade verbose
  output as candidates for categorized, throttled debug-only logging.

## Validation

- No live server/client acceptance was performed in this documentation and
  logging cleanup pass.
