# Melee Local Hit Result And Knockback Overwrite

Time created: 2026-10-19T22:36:00Z (2026-10-19 18:36:00 EDT, America/New_York)
Time last updated: 2026-10-19T22:36:00Z

Status: FIXED (source and runtime journal; human feel acceptance pending)

Related specification:

- `docs/specs/gamemodes/supersmashbrosmelee.md`, sections 24, 26, 27, 28, 59, 60

## Regression occurrence 1

### Expected behavior

An NPC hit on a player updates that player's visible Melee percent and applies
the accepted knockback once. WASD may steer strongly but must not zero or
replace the knockback impulse. Confirmed feedback uses `+N%` and never applies a
second impulse during reconciliation.

### Actual behavior

Three separate owners disagreed:

1. The local player never copied `meleePercent` or shield state from its own
   authoritative snapshot, so its percent meter stayed at 0.
2. `movement-smash.json` used `movement_mode: "mimita"` (`Override`), which
   folded and cleared external impulse and then overwrote horizontal velocity
   from WASD.
3. Confirmed Melee feedback did not carry the accepted impulse and suppressed
   the world-space damage number.

The server did produce `melee.hit` events for NPC-to-player contacts, but the
client could not present or feel them.

### Fix

- Added `MovementWalkMode::Compose` (`movement_mode: "smash"`): WASD still
  overwrites steering velocity; external knockback stays separate, decays
  normally, and is never cleared by input.
- Added local snapshot percent/shield fields and applied them during
  reconciliation as absolute values.
- Accepted Melee feedback now carries `knockX/Y/Z` and `power`; the local victim
  applies the impulse once, deduplicated by reliable event ID.
- Confirmed Melee feedback reuses the world-space damage-number renderer with
  `+N%`.

### Evidence and limits

Final run `logs/2026-10-19/20261019_223206/events.jsonl` contains
`melee.hit` for NPC actor 1000 against player 1, followed by
`melee.client.impulse_applied` once per accepted event with the same impulse.
Natural shield break applies the configured `5.0` m/s upward impulse once.
Human playtesting is still required for steering feel and the absence of
hitstun.
