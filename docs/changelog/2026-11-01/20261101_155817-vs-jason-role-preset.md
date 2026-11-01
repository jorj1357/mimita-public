# Vs Jason role preset fallback correction

Time created: 2026-11-01T15:58:17Z

## Changes

- `config/gamemodes/vs_jason.json`: changed the mode fallback `actor_preset`
  from `juggernaut_arcade` to `juggernaut_fighter`. This gives roles without
  their own actor-preset reference the existing Fighter setup: 100 FOV and
  `source` movement. Jason's explicit `vs_jason_jason` role preset remains
  `juggernaut_arcade`.
- `docs/specs/open-gameplay-issues.md`: added the root cause and live evidence
  under the existing intake describing the 50 FOV/slow movement symptom.

## Evidence

- Existing source/build identity: dev-loop build 106, executable
  `.dev/builds/0106/mimita.exe`; no source code changed, so no build was run.
- `python devscripts/gamemode_json_sync.py --check`: passed.
- Before the correction, journal `logs/2026-11-01/20261101_155118/events.jsonl`
  showed the loaded `vs_jason` mode, human role `elite_fighter`, movement
  preset `source`, and mode preset `juggernaut_arcade`.
- After relaunch, journal `logs/2026-11-01/20261101_155626/events.jsonl`
  showed mode preset `juggernaut_fighter`, human role `elite_fighter`, and
  `source` movement. The client resolved its local actor identity and the
  mode reached active play. Jason retained `vs_jason_jason`, `vs_jason`
  movement, `vs_jason_chase`, and chainsaw-only loadout.
- Version info from build 106 reported
  `EVENTS_JSONL_PATH=logs/11-01-2026/20261101_105716/events.jsonl`; this was
  a separate version-info invocation, not the gameplay run journal.
- The runtime journal verifies config and role resolution. Direct visual
  confirmation of FOV and movement remains outstanding.
