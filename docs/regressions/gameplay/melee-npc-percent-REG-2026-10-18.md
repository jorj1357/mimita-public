# Player-to-NPC Melee Percent Reset

Time created: 2026-10-18T17:23:10Z
Time last updated: 2026-10-18T17:23:10Z

Status: ATTEMPTED FIX (1)

Related specification:

- `docs/specs/gamemodes/supersmashbrosmelee.md`, section 58

Related changelog:
`docs/changelog/2026-10-18/20261018_172310-melee-npc-percent-reset.md`

## Regression occurrence 1

### Expected behavior

A confirmed player-to-NPC Melee hit adds the configured percent to the NPC's
persistent Melee state. The state is then published for NPC presentation and
snapshots. Melee uses percent and knockback, not ordinary health damage.

### Actual behavior

`melee.hit` could report positive `percent_added` and `percent_after` for a
player-to-NPC contact, but the NPC update loop subsequently replaced the
published `ServerNpc::meleePercent` from the NPC fighter state. Player-inflicted
percent had only been written to the published mirror, so it could disappear
on the next update.

### Confirmed cause

In `src/network/server-melee-actions.cpp`, the confirmed player-to-NPC branch
updated `target.meleePercent`. The NPC fighter loop later sets
`npc.meleePercent = st.percent`, where `st` is `g_npcMelee[npc.entityId]`.
Those are two values for the same NPC percent, and the latter is the value used
by the update loop. The attack path was writing the mirror instead of the
persistent owner.

### Fix

The hit path now increments `g_npcMelee[target.entityId].percent` and mirrors
that value to `target.meleePercent`, matching the update loop's ownership.

### Evidence and limits

In run `20261018_171459`, the journal contains 9 player-to-NPC confirmed hits,
including `a_ground_punch` at tick 5569 with `percent_added` about 2.998 and
`percent_after` about 2.998. The same run contains NPC-to-player confirmed
hits. This confirms the percent calculation and contact path can run; it does
not prove the visual update or the fix at runtime.

Debug build 2594 compiled 1 translation unit and skipped 554. The exact
published executable passed `--versioninfo`. The currently running game remains
on build 2593, so gameplay persistence and visual acceptance of this fix remain
unverified.
