# NPC team outlines and avatar replication

Date: 2026-10-07

## Change

- Expanded Juggernaut match actor identity replication from 32 to 64 entries,
  preventing actors beyond the old cap from being treated as unknown enemies.
- Expanded snapshot avatar-name replication from 16 to 24 bytes so valid NPC
  folders such as `abusivegirlheadless` are not truncated into invalid names.
- Added structured avatar bind, load-ready, and load-failed events so the
  client can distinguish missing identity, pending async work, invalid avatar
  data, and successful preparation.

## Evidence

- `mimita-20261007T-npc-outline-avatar-replication-v2.exe` build succeeded;
  6 translation units compiled and 542 were reused.
- `--versioninfo` succeeded with
  `EVENTS_JSONL_PATH=logs/10-07-2026/20261007_223137/events.jsonl`.
- No live game session was launched, so in-game outline colors and avatar
  appearance still require human visual/multiplayer acceptance.
