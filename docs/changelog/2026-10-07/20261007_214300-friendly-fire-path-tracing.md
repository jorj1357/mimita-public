# Friendly-fire path tracing

Date: 2026-10-07

## Finding

The mode policy was loading correctly, but generic player-to-NPC hitscan damage directly subtracted `ServerNpc.health` in `server-attack.cpp`. That path did not use the player-only `applyServerDamage` friendly-fire gate. The same class of bypass existed in direct projectile NPC damage, projectile splash damage, Godball, and Spy Knife NPC damage.

## Change

- Added a shared authoritative `serverFriendlyFireBlocks` decision owner using the gamemode roster team map with player/NPC mirror fallback.
- Routed direct player-to-NPC hitscan, projectile, Godball, and Spy Knife damage through that decision.
- Added `server.damage.policy_check` journal events with path, actor type, resolved teams, policy state, and blocked/allowed result.
- Promoted same-team rejection diagnostics to the important journal level so they are visible in normal `events.jsonl` output.

## Validation

- Did not launch the executable or run a gameplay scenario, per request.
- `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261007T-friendly-fire-trace-v2.exe`: BUILD SUCCESS; 1 translation unit compiled and the named executable linked.
- The first trace build failed at link because the new helper was initially placed inside a translation-unit-local namespace; it was moved to the exported owner and the v2 build passed.

## Next journal interpretation

For a teammate shot, the expected record is `server.damage.policy_check` with `same_team: true`, `friendly_fire: false`, and `blocked: true`, followed by no `server.damage.applied` or death event for that victim. If `blocked: false`, the teams/actor IDs reveal the assignment problem. If `blocked: true` but health still changes, the remaining path name identifies an uninstrumented damage owner.
