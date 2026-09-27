# Rocket lifecycle logging and complete JSONL records

Date: 2026-09-24

## Change

- Added rocket lifecycle diagnostics for spawn, lifetime detonation, world
  contact, and NPC contact in `src/combat/weapon-rocket-launcher.cpp`.
- Added a named Windows mutex around the complete `events.jsonl` line write in
  `src/debug/structured-log.cpp`, so client/server writers serialize one full
  record before the next writer proceeds.
- Normalized trailing newlines before writing so a caller cannot accidentally
  create an extra physical JSONL line.

## Evidence

- `logs/2026-09-24/20260924_190608/events.jsonl` contains server records as well
  as client records: `server.started`, `network.server.started`, server ticks,
  NPC registration, and server audio policy activation are present.
- That file currently has 190 physical lines: 184 parse as JSON and 6 are
  malformed. The malformed records are consistent with concurrent writers
  interleaving partial records; no valid structured `rocketlauncherexplode`
  record or server shutdown/fatal record was found.
- `config/weapons.json` already marks `rocket_launcher` as automatic with a
  `0.65` second fire delay. The new lifecycle logs will distinguish repeated
  automatic spawns from one projectile being detonated repeatedly.

## Validation boundary

- `git diff --check` passed.
- No cold executable build or process restart was performed. The logger and
  legacy NPC rocket updater are cold code, so these two changes require the
  normal cold build/restart before they can appear in a new running session.
- The existing hot projectile build from this session remains separate and was
  not replaced by this cold-only diagnostic change.
