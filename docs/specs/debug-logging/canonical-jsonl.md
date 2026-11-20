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

## Whole-game DLL build outcomes

The dev-loop journal records each terminal DLL attempt as
`dev-loop.game-generation.ready` or `dev-loop.game-build.failed`. Its fields
include `build_attempt_id`, `source_generation`, `revision`, `success`,
`duration_seconds`, `generation`, `dll`, `code_hash`, `request_ids`, the full
`notes` list, `changed_paths`, `timing_jsonl`, `summary_json`, and `error`.
Failed attempts keep the same attempt identity and note list even when no DLL
was published. The referenced per-attempt timing stream and summary live under
`.dev/build-timings/`; the run journal provides the durable searchable outcome
for that dev-loop run.

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

## Agent and scenario records (2026-11-14)

Agent-driven runtime proof uses the same append-only journal and identity rules.
These records are additive; existing logging categories are unchanged.

Required event names:

| Event | Meaning | Required fields |
|---|---|---|
| `agent.command.request` | control side accepted a request | request_id, command, run_id, target instance, build_id |
| `agent.command.accepted` | target accepted the command | request_id, target PID, scenario_id |
| `agent.command.dispatched` | command reached the shared action owner | request_id, source, context, action, tick |
| `agent.command.applied` | action changed state | request_id, tick, before/after or state delta |
| `agent.command.failed` | command rejected or timed out | request_id, reason, first missing event |
| `input.normalized` | any source normalized to an action frame | source, context, action, value, request_id, tick |
| `scenario.started` | runner began a bounded scenario | scenario_id, steps, assertions, map/mode/weapon set |
| `scenario.step` | one step executed | scenario_id, step index, command, tick, result |
| `scenario.assert` | one assertion evaluated | scenario_id, assertion, expected, actual, tolerance, passed |
| `scenario.finished` | exactly one terminal verdict | scenario_id, passed, failure_kind, first failed step/assertion, assertions_evaluated, journals |

Cross-process scenario evidence must identify the server and client journal
paths separately. A client journal containing server-tagged events is not a
substitute for the server journal.

`input.normalized` is the common record for keyboard, mouse, controller, touch,
agent, terminal, replay, and network input adapters. It must not be used to
hide raw device-specific behavior; it records the normalized action actually
consumed by the shared owner.

A scenario is not passed unless `assertions_evaluated` is true and every
required assertion passed.

## 2026-11-14 — Hotness and generation events

This section is append-only. It defines where hot-reload proof appears in the
canonical journal. The normative event contract is
`docs/specs/hotreload/hotreload.md`.

- Category: `HOT_RELOAD` (or the existing `EXECUTABLE` category when the event is
  emitted before the logger has a hot-reload category). Normal path events may
  use the feature category but must include the same fields.
- Required identity fields on every hotness event: `generation`, `exe_pid`,
  `exe_build_id`, `dll_build_id`, and `path` where applicable.
- Build events include the changed/added/removed/renamed source lists, changed
  headers and dependents, compile/link times, DLL path, revision, and code hash.
  This is the live source diff.
- Activation events include old/new generation, old/new DLL, old/new hash,
  activation boundary, quiesce result, state schema/migration result, and
  unchanged PID.
- Runtime path events: `game.transport.handled`, `game.render.path`,
  `game.input.path`, then a rate-limited `game.hotness.summary`.
- Lifetime events: job/callback registration and drain, quiesce timeout,
  unload blocked, and use-after-unload.
- Violations: `game.hotness.violation` at `Errors`. Strict mode refuses
  activation and exits nonzero; use-after-unload aborts the dev process.
- No secrets: never log tokens, passwords, join/progression tickets, or packet
  payload contents. Log hashes, ids, counts, paths, and reasons only.
- Reading order for hotness: pick the exact instance journal, read
  `game.build.candidate` and `game.generation.activated`, then
  `game.hotness.summary`, then the per-path events for the feature under test.
