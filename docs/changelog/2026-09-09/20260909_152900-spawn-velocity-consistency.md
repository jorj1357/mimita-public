# Spawn velocity consistency

- Date: 2026-09-09
- Scope: Fix inconsistent player and NPC spawn velocity application.
- Specification reviewed: `docs/features/spawnvelocity/spawnveloc.md`
- Skill reviewed: `docs/skills/spec-behavior-review-v1.md`

## Change

- Normal player respawn now computes the impulse from the newly assigned spawn yaw.
- NPC respawn now applies the configured spawn impulse instead of always setting velocity to zero.
- Duel, gamemode, map-change, and bomb respawn player paths now apply the same configured impulse instead of bypassing it with zero velocity.
- The authoritative server now polls `config/spawnvelocity.json`, so a saved JSON change applies to later spawns without restarting the server.

## Evidence

- Source: `src/network/server-players.cpp`, `src/network/server-npcs.cpp`, `src/network/server-gamemode.cpp`, `src/network/server.cpp`.
- Build: `python build_agent.py` completed with `Status: SUCCESS`; 6 files compiled and `mimita.exe` linked.
- Runtime: not yet tested with repeated player/NPC respawns.
- Human acceptance: still required; verify that look direction and impulse match on every spawn path.

## Regression record

- No new regression entry appended. This change addresses the existing spawn-velocity inconsistency described in the feature record.
