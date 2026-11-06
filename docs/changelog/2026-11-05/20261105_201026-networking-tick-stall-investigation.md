# Network reconnect and tick-stall investigation

Time created: 2026-11-05T20:10:26Z
Time last updated: 2026-11-05T20:10:26Z

Branch: `20261011zt`  
HEAD at start: `a992f9a4ebb165eb403e6892f953e8adc87dc7c6` (no commit created)

## Purpose

Trace the reports of fixed-tick slowdown, failed reconnect, possible journal
flush cost, and possible build memory pressure; apply only fixes supported by
the evidence; organize active regression details into domain folders.

## Findings and code changes

The surviving Nov. 4 run `logs/2026-11-04/20261104_151006/` has 28 server
`performance.server-tick-stall` events over 100 ms. Its worst initial stall
took 1967.379 ms at tick 0, with 1924.435 ms attributed to NPC work. The server
journal's `server.npc-collision-world.lazy-built` event measured 1856 ms for
5463 triangles at `src/network/server.cpp:1021`. Later stalls in that run are
split between NPC work and weapon contact. This proves the first-tick map
collision build caused the initial slowdown; it does not explain the later
stalls or prove that the server process crashed. The original Nov. 3 journal
named by the older performance record is absent. The Nov. 4 run also showed
very low logger flush cost, so per-record `fflush` is not supported as the
cause of its stalls.

Moved `buildNpcWorldCollision(npcWorld, world)` to immediately after map load
and before `netStartup()` in `src/network/server.cpp:543`; removed the lazy
build branches. This avoids charging map preparation to the first fixed tick.
The fresh run still recorded four stalls above 100 ms; its first was 291.880
ms at tick 225, split across 133.346 ms contact/weapon work and 121.292 ms NPC
work. Those overruns remain unresolved.

Fixed snapshot-starvation transitions. `mpUpdateConnectionHealth()` now passes
snapshot starvation into `mpNextConnectionHealth()` at
`src/network/multiplayer-packets.cpp:1909` and `:1941`; the state machine at
`src/network/connection-health.cpp:34` moves both `Connected` and
`WeakConnection` into `Reconnecting` when authoritative snapshots exceed the
stale threshold. Previously, a stale snapshot could select `WeakConnection`
while unrelated traffic kept the packet heartbeat recent, bypassing a fallback
that only acted when the selected state equaled the previous state.

## Regression organization

Moved 95 active detail files from date folders into the flat domain folders:
collisions (18), performance (7), networking (13), NPC movement (8), dev loop
(18), and gameplay (31). The six domain ledgers became their folder `README.md`
indexes and include concise recurring-problem / evidence-backed-fix summaries.
Added the previously unindexed vehicle-entry record to gameplay, classified
scenario-journal capture under dev loop, and kept the cold-build debt record in
dev loop. Preserved the root cold-build template and
`archive-ignore/regressions-v1.md` as non-active material. Updated the router
and moved-record references; corrected four old links in gold documents.

Both detailed records preserve chronology and distinguish evidence from
unresolved causes:

- `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`
- `docs/regressions/networking/frozen-client-dead-REG-2026-11-04.md`

## Build and runtime evidence

The existing dev loop published current build
`2026-11-05T20:05:47.562559Z`, SHA-256
`9a89b2f24e8f01e6306d5847a4832cbf0b191f9156d21ff527d212003fdf0183`.
Its exact executable's `--versioninfo` reported
`EVENTS_JSONL_PATH=logs/11-05-2026/20261105_150608/events.jsonl`. The real
dev-loop run used per-instance journals under
`logs/2026-11-05/20261105_200615/`, with matching server/client build IDs.

The server recorded `npc.collision-world.summary` at `20:06:17.542Z` before
gameplay ticks and no `server.npc-collision-world.lazy-built` event. In the
same run, client-02 used configured bad-connection preset 10 (Immediate 5
second blackout): it logged a 6017 ms snapshot timeout and
`Connected -> Reconnecting` at `20:07:14.103–.104Z`, then
`Reconnecting -> Connected` at `20:07:14.189Z`; the server accepted the
reconnect at `20:07:14.107Z`.

At the sampled time, Windows reported 4531 MB free of 16025 MB. Server process
memory was 154.7 MB working set / 140.2 MB private bytes; client processes
were 553.3/847.9 MB and 534.3/574.5 MB. This does not support memory pressure
in this run. Build-time peak memory was not measured, so build RAM remains
unproven as an incident cause.

The controlled dev-loop run was stopped after journal inspection. The server
crash cause and the later tick overruns are unresolved. Human visual review is
pending; no human acceptance is claimed.

## Validation

- Dev-loop build status: successful, current source, artifact ID above.
- Exact executable `--versioninfo`: captured its `EVENTS_JSONL_PATH` above.
- Runtime: inspected the exact run's server and client journals; startup
  collision preparation and blackout reconnect behavior matched expectations.
- Regression migration: 95 unique active details, one domain destination per
  file, zero remaining dated detail files, all six README entry counts agree,
  and all regression markdown links resolve.
- `git diff --check`: no whitespace errors. No test suite was added or run.
- Human review: pending.

Pre-existing workspace edits, including unrelated asset/configuration changes
and the dirty `devscripts/blender-ai` submodule, were preserved.
