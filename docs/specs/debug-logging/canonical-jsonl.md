# Canonical per-executable JSONL logging

MiMITA uses one run directory with one authoritative structured journal per
executable process:

```text
logs/<yyyy-mm-dd>/<run-id>/
  run.json
  dev-loop/events.jsonl
  server-01/events.jsonl
  client-01/events.jsonl
  client-02/events.jsonl
```

`run.json` is the manifest. It maps every `instance_id` to its role, PID,
executable, build ID, launch ID, bind/port, room code, and journal path.

See `docs/specs/dev-loop/multi-instance.md` for the manifest and instance
registry contract.

## Record identity

Every record identifies its process with at least:

- `run_id`
- `instance_id`
- `role` (`server`, `client`, `dev-loop`, `preflight`, ...)
- `pid`
- `launch_id` when the launcher supplies one
- `build_id` when available
- `tick` and UTC `wall_time`

Records remain append-only and searchable side by side. Cross-instance
correlation uses `run_id`/`instance_id`, actor/entity/projectile IDs, and ticks;
it must not rely on two processes sharing one file.

## Rotation

Inside an instance folder, rotate `events.jsonl` to `events-NNNN.jsonl` before
it crosses the configured per-file limit, then continue in a fresh
`events.jsonl`. Readers read sealed segments in numeric order and then the
active file as one chronological instance stream.

`EVENTS_JSONL_PATH` and `--versioninfo` report the exact instance journal for
that process.

## Quota

The run-level quota covers the whole run folder. The manifest records each
instance journal's current byte count. A write that would exceed the run quota
is rejected or summarized; the active instance journal and live process files
are never deleted.

## Standalone launches

A standalone process that does not set `MIMITA_EVENTS_FILE` keeps a
per-process `events.jsonl`. The run-manifest convention applies to the dev loop
and any launcher that supplies per-instance paths.

## Reading order

1. Open `run.json` and pick the exact `instance_id`/PID/journal.
2. Read that instance's `logger.started` and build identity.
3. Read only that instance journal unless cross-process correlation is needed.
4. For cross-process work, read each instance journal by UTC time and match
   `run_id`/`instance_id`, actor/entity IDs, and ticks.
5. Never infer a process or journal from a similarly named older file.
