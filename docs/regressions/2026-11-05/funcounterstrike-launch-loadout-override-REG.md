# Fun Counter Strike launch inherited Captain Falcon weapon set

Time created: 2026-11-05T17:56:10Z
Time last updated: 2026-11-05T17:56:10Z

Status: FIXED IN SOURCE; LIVE JOURNAL PASS; HUMAN VISUAL REVIEW PENDING

Related specification:
`docs/specs/dev-loop/dev-loop.md` — launch-mode server arguments and live proof
`config/gamemodes/funcounterstrike.json` — set ID 30
`config/roles.json` — Fun Counter Strike actor roles

Related changelog:
`docs/changelog/2026-11-05/20261105_125610-rmb-aim-fcs-loadout-debug.md`

## Regression Occurrence 1

### Observed

The `npc-navigation` dev profile specifies mode `ssbm` and explicit weapon set
12 (`captain_falcon`). Launch mode 45 appended `--mode funcounterstrike` and
`--gamemode funcounterstrike` but retained the earlier `--weapon-set 12`.
Because the server treats a concrete `--weapon-set` as a host override, the
Fun Counter Strike game started with Captain Falcon's loadout.

The original server journal proves the mixed configuration:

- `gamemode.match.configured`: mode `funcounterstrike`, set ID 12,
  `weapon_set_source=host_override`.
- `actor.loadout.resolved`: set 12 and `captain_falcon_moveset`.
- `client.actor.spawn_packet_applied`: community set 12.

### Expected Behavior

When dev launch mode 45 starts Fun Counter Strike without an explicit
launch-specific weapon-set choice, the gamemode and actor role own the default
weapon set. The mode's configured set is ID 30, key `funcounterstrike`.

### Cause

`DevLoop.build_server_args()` always added the selected dev profile's weapon
set as a concrete `--weapon-set`, then appended the launch mode's server args.
The server's explicit host-set precedence correctly honored that inherited
profile value, even though the final mode was Fun Counter Strike.

### Fix

Dev launch modes can declare `"weapon_set": "auto"` to omit the inherited
profile override. Launch mode 45 now uses Auto unless a weapon set is
explicitly supplied in the launch request. Other launch modes retain their
profile weapon-set behavior.

The authoritative player spawn journal also emits `actor.loadout.spawned`
with mode, role, set ID/key/name, complete weapon list, spawn generation, and
equipped weapon/slot. This makes the selected set and final equipped item
inspectable together in the same server `events.jsonl`.

### Live Proof

- Dev-loop build ID:
  `2026-11-05T17:52:25.918603Z:de51dc23ec632b6f78efba1575fc50feed5be90a0e427e525d03262bede40d9d`
- Run: `20261105_175404`, Fun Counter Strike, map `dust2cyberiav4`.
- Server journal:
  `logs/2026-11-05/20261105_175404/server-01/events.jsonl`.
- `gamemode.match.configured` records mode `funcounterstrike`, set ID 30,
  source `gamemode_default`.
- `actor.loadout.spawned` records ID 30, key `funcounterstrike`, name
  `Fun Counter Strike`, and the 13-item inventory. After role assignment it
  records role `fun_counter_terrorist` and equipped `revolver`.
- Client journal records community set ID 30 in
  `client.actor.spawn_packet_applied`.
- The live window showed the Fun Counter Strike HUD and Revolver. Visual
  inspection of every inventory item remains pending.
