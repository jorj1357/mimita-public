# Force Punch confirmed damage and held-arm pose

## Status

PASS_WITH_HUMAN_REVIEW for the held-arm presentation; Force Punch NPC damage
is confirmed by live server log evidence.

## Changes

- Added the gold behavior record
  `docs/gold/2026-09-26-force-punch-npc-damage-gold.md`.
- Added a `force_punch` idle weapon pose in `config/animations.json`.
- While Force Punch is equipped, the right arm uses a forward presentation
  with Euler Z rotation `90` degrees and a small forward translation.
- The pose uses the existing animation weapon-pose owner and remains separate
  from the authoritative damage/contact path.

## Validation

- `config/animations.json` parsed successfully.
- `Server_log_215258.txt` repeatedly records Force Punch NPC damage of 250 and
  Force Punch kill events.
- Visual pose appearance still requires observing the rebuilt client.
