# Canonical client/server JSONL logging

MiMITA uses one authoritative structured log stream per test session:

```text
logs/<yyyy-mm-dd>/<yyyymmdd_hhmmss>/events.jsonl
```

The stream is append-only and is shared by the client and dedicated server
when the launcher supplies the same `MIMITA_EVENTS_FILE` path to both
processes. Every record identifies its side with `process` (`client` or
`server`) and includes the process ID. Server and client ticks, entity IDs,
actor IDs, projectile IDs, connection/request IDs, event names, results, and
errors remain searchable side by side in the same file.

Before the active `events.jsonl` would exceed 100,000,000 bytes, it is sealed as
`events-NNNN.jsonl` and a fresh `events.jsonl` continues the run. Readers must
read sealed segments in numeric order and then the active file as one
chronological run. The active path remains `events.jsonl`, so
`EVENTS_JSONL_PATH` and launcher contracts are unchanged.

`StructuredLogger` is the single writer. The old `LiveEventJournal` API is
kept as a compatibility facade for existing hot-code call sites, but it now
emits into `events.jsonl`; new runs must not create
`logs/features/live-code/live_events_*.jsonl`. Existing live-code files are
historical evidence and are not rewritten.

The active `config/debuglogger.json` remains the hot-reloadable filtering
authority. Category changes affect future records without restarting the game.
The shared-file bridge and launcher environment are process/runtime plumbing,
so installing that behavior requires a cold executable build once; changing
the category selection does not.

For a local NPC crash test, inspect one file in this order:

1. `logger.started` for both sides and their PIDs.
2. `server.started`, connection/ICE stages, and the first matching client
   connection stages.
3. actor/NPC spawn, weapon/projectile spawn, impact, damage, death, and
   respawn records, matching `entity_id`, `actor_id`, and `projectile_id`.
4. tick fields immediately before the first error, disconnect, shutdown, or
   missing expected stage.

The old per-process `events.jsonl` files remain valid for standalone launches
that do not set `MIMITA_EVENTS_FILE`; the shared convention applies to the
V4 session launcher and any future launcher that passes the same path to all
participating processes.
