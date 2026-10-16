# Juggernaut Duel implementation

- Timestamp: 2026-10-16T04:14:20Z (2026-10-16 00:14:20 America/New_York, EDT)
- Result: `IMPLEMENTED_SOURCE_AND_CONFIG; SOLO_RUNTIME_PARTIAL`

## Outcome

Implemented the selectable `juggernaut_duel` mode and updated
`docs/specs/gamemodes/juggernaut-duel.md` with the agreed first-to-20 loop,
owner map, implementation requirements, and current evidence. Players wait in
an empty lobby; the first participant starts the configured three-second
intermission. The mode then uses the shared countdown and GO phases. A solo
human gets one opposing Juggernaut NPC; a second human replaces that NPC at the
next intermission. Both team roles use the Juggernaut movement, actor preset,
and weapon set, with Jason and Clown forced by team. The first competitor to
20 opposing kills wins, results display, scores reset, and the loop repeats.
Active deaths use the shared respawn path with the configured 0.01-second
respawn interval.

The implementation adds a per-mode human join cap and advertised/host capacity
handling, a generic `first_to_score` win condition, team-roster NPC reconciliation,
and generic score/winner HUD support. It does not add a parallel weapon,
movement, damage, or respawn system.

## Evidence

- Canonical `python devscripts/dev-loop.py` published build 2413 from source
generation 3. Exact executable:
  `.dev/builds/2413/mimita.exe` (SHA-256
  `0500a2b4aa4e1ae33213d84eb40001a36ea4203f1c7601024a824e406294b78c`).
- Ran that executable with `--versioninfo`; it reported its executable path.
  That invocation's own `EVENTS_JSONL_PATH` was a separate versioninfo process
  journal; the launched server's live journal was
  `logs/2026-10-16/20261016_041202/events.jsonl`.
- The live server journal confirms `juggernaut_duel` loaded, friendly fire is
  disabled, the `team_roster` NPC policy is active, solo actor 200000 was
  created on team 1, and the human Jason and NPC Clown roles resolved to
  `juggernaut_arcade` / `juggernaut` profiles. One human connected. This is
  partial runtime evidence: a second human, actual death/respawn, 20-kill win,
  visible HUD and avatar appearance, and repeat-round behavior were not exercised.
- After cleanup, the dev loop reported source generation 4 and published build
  2414 successfully; its executable hash matches build 2413. No runtime claim
  is based on build 2414.
- `git diff --check` passed. JSON parsing passed for the mode catalog, new
  gamemode config, roles, HUD layout, and dev-launch mode file. No tests were
  added or run. Build evidence is compilation/linking only; multiplayer and
  visual acceptance remain open.

## Scope and preservation

Preserved pre-existing user edits in `config/accounts/default.json`,
`config/analytics.json`, and `config/audio/music-settings.json`. Removed the
temporary dev-loop launch entry used for the solo runtime check. Stopped only
the client and server launched for this check; did not terminate unrelated
pre-existing processes.
