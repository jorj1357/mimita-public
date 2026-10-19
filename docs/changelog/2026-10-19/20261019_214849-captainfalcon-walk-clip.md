# Captain Falcon movement uses the walk clip

- Local time: 2026-10-19 17:48 EDT (UTC-04:00)
- UTC: 2026-10-19 21:48 UTC
- Branch/starting commit: `20261011zt` / `35c9b12a`
- Request: make WASD movement use Captain Falcon's walking animation instead of the other movement clip.

## Change

- Updated `assets/avatars/captainfalcon/animations.json:7`.
- Before: `"run": "captainfalcon_run",`
- After: `"run": "captainfalcon_walk",`
- The existing `locomotion.walk` mapping already selected `captainfalcon_walk`. Runtime selection switches to `states.run` when movement speed exceeds 12, so the config now maps both moving states to the walking clip. Jump, fall, landing, and explicit attack clips remain separately selected.

## Evidence

- The active live journal previously recorded Captain Falcon moving with `moving=true` and `to=captainfalcon_run` at speed above the run threshold.
- After the JSON hot reload, the same live client recorded `anim.set.reloaded` revision 3 and repeated `anim.state.changed` events with `moving=true` and `to=captainfalcon_walk` at the same high movement speeds. This confirms the runtime selected the walking clip for both walk and run movement states.
- The active dev loop was already building unrelated source code and reports an existing compiler error in `src/network/server-melee-actions.cpp`; this config-only change was hot-reloaded without altering that build or stopping the running game.
- Structured logs confirm the selected clip name, not human visual acceptance of the rendered pose.
