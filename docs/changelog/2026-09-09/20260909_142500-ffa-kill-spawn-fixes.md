# FFA kill queue and spawn fixes

Date: 2026-09-09

## Changes

- Replaced the fragile single pending-kill overwrite path with an ordered actor-neutral queue in the server gamemode runtime.
- Routed authoritative player and NPC lethal damage into queued gamemode events with actor kind, weapon identity, event ID, correlation ID, and server tick.
- Rebuilt community spawn inventories on managed spawns so a selected restricted weapon set cannot inherit an unrestricted prior inventory.
- Routed NPC death reset through the current gamemode map spawn anchor, preventing old-map respawn positions after map changes.
- Kept NPC names canonical in the client NPC damage/kill presentation fallback (`NPC-####`).
- Synchronized `npc_delete_all` with the authoritative gamemode participant, score, death, team, and name snapshots.

## Validation

- JSON parsing passed for the good-map pool, gamemode GUI, Tab leaderboard, killfeed, weapons, and FFA configuration.
- Source compilation reached the link step through `python build_agent.py`.
- Final linking was blocked because `mimita.exe` was in use (`Permission denied`); no live acceptance was performed.
- `git diff --check` reported pre-existing trailing whitespace in `docs/features/TEMPLATE.md`; no unrelated formatting was changed.
