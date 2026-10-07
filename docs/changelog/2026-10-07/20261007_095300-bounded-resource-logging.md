# Bounded resource logging for Juggernaut/NPC diagnosis

- Status: `BUILD_VERIFIED_RUNTIME_SCENARIO_PENDING`
- Timestamp: `2026-10-07T13:53:00Z` (display timezone: `America/New_York`)
- Branch/commit: `afad20a-rebuild` / `00dc25ebe0f4da8d338440024c3a542369c98905`

## Implemented

- `src/perf/perf-frame.cpp` now emits one `performance.resource-window`
  `IMPORTANT` event per 60 captured frames. Each record aggregates frame
  duration, worst frame, allocation count/bytes, peak NPC/effect/projectile
  counts, named timer scopes, process working-set/private/pagefile memory, and
  logger write/flush/mutex-wait time.
- Added explicit timing scopes for `Server::TeamBrain`,
  `Server::NpcSimulation`, `Npc::Navigation`, `Npc::RecastPrepare`, and
  `Npc::RecastQuery`.
- `src/debug/structured-log.*` now counts events, bytes, flushes, flush time,
  and cross-process event-file mutex wait time so logging overhead is visible
  in the same 60-frame record.
- Enabled the `performance` category at `important` in
  `config/debuglogger.json` and linked PSAPI for process-memory sampling.
- Created `mimita-20261007T0952-logging-v1.exe` from the successful build.

## Preserved pre-existing work

The Juggernaut/swarm and Recast behavior changes already present in the
checkout were preserved. They remain in the working tree alongside this
diagnostic patch; this changelog does not claim those gameplay changes as part
of the logging implementation.

## Validation evidence

- `git diff --check`: passed; only expected line-ending conversion warnings.
- Forced build after the incremental builder initially skipped touched units:
  6 C++ translation units compiled, link succeeded, return code 0.
- `mimita.exe --versioninfo`: passed. It created
  `logs/10-07-2026/20261007_095220/events.jsonl` and reported the exact path.
- The journal contains `logger.started`, `versioninfo.executed`, and
  `logger.stopped`; no real Juggernaut match was run in this validation.

## Remaining work

- The existing per-NPC `npc.movement-decision`, `npc.stuck`, and related event
  producers are not yet replaced by a bounded producer-side aggregator. The
  new summary prevents the diagnostic summary itself from growing with NPC
  count, but it does not by itself stop all existing detailed events.
- A real Juggernaut run is still required to observe `performance.resource-window`
  records, correlate them with the first white-screen/stall tick, and decide
  whether the dominant cost is navigation, TeamBrain, simulation, rendering,
  allocations, memory growth, or logger I/O.
- If logger I/O is shown to be the bottleneck, the next step is a bounded
  asynchronous queue or category-specific event aggregation with explicit
  dropped-record counters; this patch intentionally measures the current
  synchronous writer instead of changing its threading behavior.

## Focused review

- Reviewed under `docs/skills/logging-checker-v1.md` and the routed runtime
  scenario/build instructions.
