# Melee Percent And Knockback Scaling

Time created: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/New_York)
Time last updated: 2026-10-19T22:36:00Z

Status: FIXED (source and runtime journal; human feel acceptance pending)

Related specification:

- `docs/specs/gamemodes/supersmashbrosmelee.md`, sections 25, 26, 29, 60

## Regression occurrence 1

### Expected behavior

An accepted unshielded Melee hit adds exactly the move's configured
`base_percent`, independent of the target's current percent. Target percent and
contact force scale knockback only. The shared percent-to-knockback curve is
editable in `config/melee.json` under `force.knockback_curve`.

### Actual behavior

The server computed `percentScale = 1 + target.meleePercent / 100` and multiplied
both percent and knockback by it, then also multiplied percent by contact
`force`. There was no shared curve. Live journal
`logs/2026-10-19/20261019_205156/events.jsonl` shows NPC `a_air_kick` adding
`3905`, `4899`, `5696`, and later `11976` percent in successive hits.

### Fix

`src/network/server-melee-actions.cpp` now computes one shared result:

- `percentAdded = action.basePercent * shieldMultiplier` only;
- knockback uses the pre-hit target percent through
  `MimitaMeleeRules::Config::curveDistance()` and
  `movementReferenceImpulseForDistance()`;
- `force`, move base impulse, vertical fraction, min/max impulse, and the
  shield multiplier are applied to knockback only;
- `config/melee.json` contains the required 0/0.5 m, 100/10 m, 200/50 m curve.

### Evidence and limits

Final runtime journal `logs/2026-10-19/20261019_223206/events.jsonl` contains
repeated NPC-to-player hits with `percent_added: 3.0` and `20.0` at different
target percents. Player-to-NPC hits in
`logs/2026-10-19/20261019_215612/events.jsonl` show `percent_added: 20.0` and
`3.0` while `curve_distance_m` increases with target percent. Human playtesting
of the curve's feel remains required.
