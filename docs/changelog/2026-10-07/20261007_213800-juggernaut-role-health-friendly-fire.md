# Juggernaut role health and friendly-fire enforcement

Date: 2026-10-07

## User-visible result

- Juggernaut NPC snapshot health is now restored from the actor's resolved role in `config/roles.json` during roster creation, automatic map changes, manual map changes, and respawn-all. The configured `juggernaut_mode_juggernaut` role remains 2,000 HP; this change does not alter balance values.
- Juggernaut same-team player damage is rejected at the authoritative damage owner when `friendly_fire` is false. The team lookup falls back to the authoritative gamemode roster when a live player mirror has not received its team yet, preventing hits, deaths, kill credit, and kill-heal from teammate shots.
- Blocked same-team damage emits a structured `server.damage.rejected` diagnostic at verbose level with attacker, victim, team, source, and requested damage fields.

## Source owners

- `src/network/server-gamemode.cpp`: one role-health application helper replaces lifecycle-local `health = 100` resets.
- `src/network/server-damage.cpp`: authoritative same-team damage rejection now resolves teams from the mode roster as a fallback.
- `config/roles.json`: source of the current Juggernaut role health value (2,000).
- `config/gamemodes/juggernaut.json`: `friendly_fire: false` remains the mode policy.

## Validation

- `python build_agent.py` with `MIMITA_EXE_NAME=mimita-20261007T-role-health-ff-v1.exe`: BUILD SUCCESS; 1 translation unit compiled; newly named executable linked.
- `mimita-20261007T-role-health-ff-v1.exe --versioninfo`: PASS; journal `logs/10-07-2026/20261007_213732/events.jsonl`.
- Source audit: no remaining `npc.health = 100` assignments in the reviewed Juggernaut gamemode lifecycle owner.
- Live two-client teammate-fire acceptance is still pending; the build and version journal prove the artifact identity, not the visual/multiplayer result.
