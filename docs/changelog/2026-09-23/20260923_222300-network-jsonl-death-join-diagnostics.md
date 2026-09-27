# Network JSONL death and join diagnostics

Date: 2026-09-23 18:23:00 America/New_York
Status: PASS_WITH_HUMAN_REVIEW

## Scope

Added bounded, flushed JSONL diagnostics to the existing `LiveEventJournal`.
The journal remains the single append-only output and no unmanaged debug file
was introduced.

## Files changed

- `src/live-code/network-journal.h`: new shared helper for compact network
  lifecycle records.
- `src/network/server.cpp`: server start, server-thread start, sampled server
  ticks, normal thread exit, and caught thread exceptions.
- `src/network/server-damage.cpp`: damage entry and authoritative actor death,
  including attacker, self-damage flag, and respawn timer.
- `src/network/server-projectiles.cpp`: authoritative projectile explosion,
  weapon, owner, victim count, and splash damage.
- `src/network/multiplayer-packets.cpp`: ICE/join progress and join success or
  failure stages.

The pre-existing modifications in `config/accounts/default.json`,
`config/analytics.json`, and `config/audio/music-settings.json` were preserved
and are unrelated to this change.

## Evidence

- `git diff --check`: passed; only line-ending normalization warnings were
  reported for modified Windows source files.
- `python build_agent.py`: `BUILD SUCCESS`; executable
  `C:\mimita-priv-v8\mimita-20260923T222103.exe`.
- `mimita-20260923T222103.exe --server-journal-selftest`: PASS.
- No live multiplayer or rocket self-kill acceptance was performed by Codex.

## Expected JSONL events

- `network.server.started`
- `network.server.thread_started`
- `network.server_tick` once at tick 1 and once per 60 ticks
- `network.ice.stage`
- `network.join.completed` or `network.join.failed`
- `network.projectile.explosion`
- `network.damage.begin`
- `network.actor.death`
- `network.server.thread_exiting` or `network.server.thread_exception`
- Existing `server.player_spawned`, `server.player_respawned`, and NPC
  lifecycle events remain active.

## Human test

1. Launch the new executable and host a server.
2. Join it through the normal browser/ICE flow.
3. Find the newest file under
   `logs/features/live-code/yyyy-mm-dd/live_events_*.jsonl`.
4. Fire the rocket launcher at the floor at close range until self-death.
5. Wait for respawn and observe whether the server remains available.
6. If it dies, stop testing and preserve the JSONL file.

The key question is the last event. If the final event is
`network.actor.death` with no subsequent explosion/kill/respawn event, the
failure is immediately after damage application. If
`network.server.thread_exception` appears, its error identifies the thrown
exception. If the file ends at `network.projectile.explosion` or
`network.damage.begin`, the next missing stage identifies the boundary.

## Required human review

Human review is still required to confirm that the new executable writes the
events during the real ICE join and rocket self-death scenario, and to inspect
the final JSONL event if the server still exits.

