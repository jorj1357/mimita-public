# Ragdoll JSONL Process Ownership Diagnostics

Time: 2026-10-06T18:18:00Z

## Outcome

Clarified canonical JSONL ownership for live ragdoll investigations. A shared
journal can begin with a server record and still contain client records; the
diagnostics now make that explicit and identify the exact executable for each
logger startup.

## Source changes

- `src/debug/structured-log.cpp`
  - `logger.started` now includes `process_role` and the absolute executable
    path.
- `src/game/game-cli.cpp`
  - `--versioninfo` now prints `PROCESS_ROLE=client|server` and includes the
    role in `versioninfo.executed`.
- `src/devtools/dev-log-commands.cpp`
  - `log_open` now reports records by process, startup identities, death-event
    count, ragdoll-event count, and an explicit warning when neither event
    family is present.
- `docs/regressions/2026-10-06/ragdoll-jsonl-process-ownership-REG.md`
  - Records the recurring client/server journal ambiguity and prevention rules.

## Build evidence

- Executable: `mimita-20261006T-log-ownership-v2.exe`
- Build status: SUCCESS
- Compiled: 1 source file
- Tests: none added or run, per the live-runtime investigation scope.

## Runtime evidence

`mimita-20261006T-log-ownership-v2.exe --versioninfo` printed:

```text
PROCESS_ROLE=client
EVENTS_JSONL_PATH=logs/10-06-2026/20261006_141704/events.jsonl
```

The corresponding `logger.started` record contained the executable path,
`process=client`, and `process_role=client`.

The supplied journal
`logs/10-06-2026/20261006_141256/events.jsonl` contained both a server and a
client `logger.started` record, but no `death.*` or `ragdoll.*` records. A
fresh live Counter-Strike death using the newly named executable is still
required to prove the complete death-to-ragdoll event chain.

## Follow-up render/lifetime diagnostics

- `src/ragdoll/ragdoll-mode.cpp`
  - logs `ragdoll.corpse.render_submitted` the first time each live corpse
    reaches `renderNetworkPlayer`;
  - logs `ragdoll.corpse.position_sample` every 60 corpse simulation updates,
    including age, position, velocity, fade, and active count;
  - logs `ragdoll.corpse.evicted` when the active corpse cap removes the oldest
    corpse.
- `src/ragdoll/ragdoll-mode.h`
  - stores per-corpse lifetime sample ticks and render-log state.

The source has a hard active-corpse limit of 12. The new eviction event makes
that limit visible in the live journal instead of silently dropping the oldest
corpse.

Follow-up build:

- Executable: `mimita-20261006T-ragdoll-render-trace-v1.exe`
- Build status: SUCCESS
- Compiled: 13 source files
- Tests: none added or run; live runtime evidence remains required.

## Second-order render diagnostics

- `src/render/render-player.cpp`
  - adds bounded ragdoll-stage events for renderer absence, dead-state gates,
    and entry into the inner player renderer;
  - records shader ID, model readiness, physical body/mesh counts, fallback
    mesh counts, and corpse position.
- `src/entities/player-render.cpp`
  - records which actual render path was selected: physical body meshes,
    fallback render mesh, or capsule;
  - records drawable part, batch, and vertex counts so a successful outer call
    cannot be mistaken for an actual draw.
- `src/ragdoll/ragdoll-mode.cpp`
  - includes username and match team in corpse diagnostics so teammate and
    enemy deaths can be correlated.

Follow-up build:

- Executable: `mimita-20261006T-ragdoll-render-trace-v2.exe`
- Build status: SUCCESS
- Runtime identity probe: `PROCESS_ROLE=client`,
  `EVENTS_JSONL_PATH=logs/10-06-2026/20261006_143024/events.jsonl`.
- Tests: none added or run; this remains a live executable investigation.
