# Duel NPC Shared Weapon Set

Time created: 2026-10-26
Time last updated: 2026-10-26T00:49:56Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/duel.md`

Related changelog:
`docs/changelog/2026-10-26/20261026_003225-duel-outline-team-identity.md`

---

## Regression Occurrence 1

### Observed

The user asked for Duel NPCs to have the same weapons as the human player and for both teams to use one weapon set. Duel declares `weapon_set_id: 1`, but the previous team role references used generic roles with different loadout policies.

### Expected Behavior

Duel players and NPCs on RED and BLUE resolve the same active mode weapon set and weapon IDs.

### Actual Behavior

`config/roles.json` defines `elite_fighter` with `weapon_set: standard` and `elite_npc` with `weapon_set: elite_arsenal`. Those overrides superseded Duel's mode set for the referenced roles.

### Specification

`docs/specs/gamemodes/duel.md` § Actor presets and standard loadout:

> The gamemode's `weapon_set_id: 1` supplies the Stable weapons set because the roles do not override it.

### Cause

Duel used generic roles that carry their own loadout policy rather than its authored Duel-specific roles. Separately, an NPC role without a weapon-set override could retain only a subset after filtering its generic inventory against the mode set.

### Attempted Fix 1

Both teams now reference the Duel-specific `duel_monster_test_red/blue` player roles and corresponding `_npc` roles. These roles share `duel_standard` and leave `weapon_set` unset, making mode set 1 the shared source.

`serverResolveActorSpawnProfile` now fills an absent role/preset weapon list from the active `communityWeaponSetId`. NPC spawn applies that resolved list through the existing `npcApplyLoadout` owner.

Build 3363 succeeded and the dev loop launched Duel NPC Elite with the server configured for `--weapon-set 1`. The `npc.grenade-availability` event for actor 1000/team 0 at `2026-10-26T00:47:41.118Z` lists all ten weapons from Stable set 1. The current client and server report matching build 3363 identities. Human gameplay acceptance remains pending.
