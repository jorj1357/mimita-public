# Payload roster NPCs did not replicate and both teams used one spawn

- Recorded: `2026-10-25T15:29:08Z`
- Status: FIXED IN SOURCE; RUNTIME AND HUMAN VISIBILITY CONFIRMED FOR PAYLOAD 1
- Changelog: [`../../changelog/2026-10-25/20261025_152908-payload-npc-spawn-authoring.md`](../../changelog/2026-10-25/20261025_152908-payload-npc-spawn-authoring.md)

## Observed

In Payload, NPCs were reported invisible. The server listed 23–47 NPCs and the
client sometimes listed models loaded, but roster NPCs did not appear at their
team spawns. A startup NPC (`1000`) could appear while roster NPCs did not.

Separately, NPCs and players from both teams were seen at the untagged
`spawnpoint_1` position instead of the authored attacker/defender spawns.

## Expected

- Roster NPCs use unique ids that cannot collide with player ids.
- Attacker and defender actors spawn at the map's `attacker_spawn` and
  `defender_spawn` entities.
- A duplicate-id snapshot is never silently responsible for hidden NPCs.

## Cause

1. `buildObjectiveRoster` reserved the `100000+` roster id range only when the
   NPC mirror map was empty. A normal GUI server starts with one startup NPC, so
   roster ids began at `0` and collided with player ids. The client rejects an
   entire snapshot with duplicate entity ids
   (`src/network/snapshot-chunks.cpp`, `duplicate-entity`).
2. The cleanup loop only erased ids `>=100000`, so low-id roster bodies were
   never retired and accumulated on rebuild.
3. `config/gamemodes/payload.json` did not declare `spawn_strategy`. The default
   `Gamemode::spawnStrategy` is `"shared"`, which returns the single map anchor
   before the resolver reads team spawn groups. Both teams collapsed onto the
   untagged `spawnpoint_1`.

## Wrong code

- `src/network/server-gamemode.cpp`: `if (npcs.empty() && d.roundNextNpcId == 0)
  d.roundNextNpcId = 100000;`
- `src/network/server-gamemode.cpp`: cleanup condition `it->first >= 100000`
- `config/gamemodes/payload.json`: no `spawn_strategy` key

## Corrected code

- `src/network/server-gamemode.cpp`: objective roster ids always seed at
  `100000`; allocation skips players/startup NPCs; tracked
  `objectiveRosterNpcIds` are retired and the range is reused after cleanup.
- `src/network/server-packets.cpp`: duplicate snapshot ids are dropped and
  logged as `server.snapshot.duplicate-entity`.
- `src/network/multiplayer-tick.cpp`: failed chunk reassembly is logged as
  `client.snapshot.reassemble-failed` with the reason.
- `config/gamemodes/payload.json`: `"spawn_strategy": "team_shared"`.
- `src/network/server-gamemode.cpp`: `gamemode.spawn-resolved` and
  `gamemode.spawn-groups-resolved` diagnostics; untagged points are refused for
  a team that declares spawn tagging.

## Proof

- Build `3197` runtime journal
  `logs/2026-10-25/20261025_151724/events.jsonl`:
  - roster ids `100000..100022`
  - `gamemode.spawn-resolved` `fallback=authored-team-group`,
    `candidate_count=1`
  - team A `(-312.59,-28.12,1150.36)`, team B
    `(-292.56,-64.83,2256.30)`, separation `1106.74`
  - `remote_npcs=24`, `alive=24`, `dead=0`, `model_loaded=24`
  - zero `server.snapshot.duplicate-entity` and zero
    `client.snapshot.reassemble-failed` events
- Human observation on the same run: all NPCs spawned and were visible.
- Final source build `3212`: `--payload-selftest` PASS,
  `--payload-authoring-selftest` PASS, `--map-config-selftest` PASS.
