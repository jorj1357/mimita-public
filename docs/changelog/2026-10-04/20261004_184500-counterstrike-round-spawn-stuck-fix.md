# Counter-Strike round, spawn, and NPC stuck recovery

## Request

Fix Counter-Strike one-life round resolution, preserve CT/T identity through
death, prevent generic maps from putting both teams on one spawn anchor, stop
the client from locally respawning a directly connected round-mode player, and
give policy NPCs a bounded repeated-jump recovery when they are stuck on a
ramp, crate, or corner.

## Root causes found

- `updateActorStates` changed a dead actor's saved `matchTeams` entry to the
  Spectator team. That removed the dead side from the elimination check and
  could also change the side used when rebuilding the next round.
- `DeathSystem` only recognized `DuelQueue` as a network match. A direct
  community-server Counter-Strike connection could therefore locally respawn
  a dead player even though the server had one-life rules.
- When a map had generic spawn nodes but no explicit CT/T node names, the
  team spawn resolver intentionally used one shared fallback anchor.
- Policy NPC stuck recovery only turned and repathed. It did not attempt the
  requested repeated jump escape.

## Changes

- Preserve the original playing team while setting the actor state to
  Spectating in one-life rounds.
- Treat an active replicated community mode as authoritative for local death
  handling, alongside the existing DuelQueue check.
- For a two-team mode with at least two generic map spawn points, use the first
  point for CT and the last point for T until explicit `spawnpoint.CT` and
  `spawnpoint.T` tags are authored.
- When a policy NPC is stuck on the floor, request a repath and issue a jump
  using the existing policy jump gate and local movement correction.

## Evidence

- `python build_agent.py`: BUILD SUCCESS; relinked `mimita.exe`; 3 translation
  units compiled.
- `mimita.exe --counterstrike-acceptance-selftest`: PASS.
- `mimita.exe --spawn-tag-selftest`: PASS.
- `mimita.exe --npc-targeting-selftest`: PASS.
- No live human match acceptance was performed in this session.
