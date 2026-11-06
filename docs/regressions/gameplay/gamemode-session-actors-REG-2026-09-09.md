# Gamemode Session and Actor Lifecycle Regression

Time created: 2026-09-09T14:30:00Z
Time last updated: 2026-09-09T14:30:00Z
Status: ATTEMPTED FIX (1)

## Observed behavior

During human FFA playtesting on 2026-09-09, an NPC was counted inconsistently
as a killer or victim. After a map change or a second countdown, NPCs could
remain at the previous map position, while the visible countdown remained at
`3` and the client stayed frozen. An NPC kill could be displayed as
`unknown killed admin with unknown`, and a deleted NPC could remain in the
leaderboard. A new server could also inherit countdown/presentation state from
the stopped server.

## Expected behavior

Players and NPCs are generic participants. They use the current map spawn,
the same authoritative damage/score path, reliable membership updates, and
the same leaderboard/killfeed presentation. A new server session accepts its
first state independently of the old server. The lifecycle is
`INTERMISSION → COUNTDOWN 3 → COUNTDOWN 2 → COUNTDOWN 1 → GO → ACTIVE`, with
no stale countdown state.

## Exact wrong code

The old NPC-to-player damage confirmation populated the player-only attacker
field with zero:

```cpp
queueServerDamageConfirmedEvent(
    sock, players, tick, totalPacketsOut, 0, *nearest, damage, result,
    realHit, realNormal, knockback, ServerDamageSource::Hitscan, hitWeapon);
```

The old FFA leaderboard fallback also generated a generic placeholder instead
of using the registered actor name:

```cpp
"NPC %u", sorted[i].first
```

## Corrected code

The damage event now carries actor-neutral attribution:

```cpp
event.attackerEntityId = attackerNpcId != 0 ? attackerNpcId : attackerPlayerId;
event.attackerEntityType = attackerNpcId != 0 ? ENTITY_NPC : ENTITY_PLAYER;
```

The server NPC path supplies its real ID, the participant registry supplies
`NPC-<id>`, and the client resolves that identity from the remote NPC table.
The session teardown and new-connection paths call the general match-client
reset, which clears replicated state, leaderboard, and killfeed state.

## Confirmed cause

The cause was confirmed by tracing the event data flow: NPC damage reused a
packet schema and presentation branch that only had a player attacker ID, so
the receiver could not identify the NPC. Map transitions likewise updated
player-facing spawn state without consistently updating the simulated NPC
body's position and respawn position. Session state lived longer than the
connection that created it.

## Fix and proof

Attempted fix (1) adds actor-neutral damage attribution, canonical NPC names,
registered participant names for FFA leaders, shared NPC map-spawn resetting,
and a connection/session reset that clears match presentation state. Edited
translation units compiled and the canonical executable linked successfully
with `Status: SUCCESS`. Human live acceptance of repeated countdowns, NPC
killfeed text, deletion removal, and stop-server/start-server isolation is
still required before this record can become a solution.

## Related changelog

`docs/changelog/2026-09-09/general-gamemode-engine.md`
