# Task

- Task ID: server-tick-npc-log-investigation
- Summary: Trace the Zombie Tower server slowdown and add bounded fixed-tick stage diagnostics.
- Status: implemented and built; live Zombie Tower acceptance pending
- Date, time, timezone: 2026-10-08T14:20:32-04:00, America/New_York
- Branch: afad20a-rebuild
- Base commit: 47247ac473390ac2407dfb02d08b8029bbccae38
- Final commit: uncommitted working tree

# Pre-existing changes

- Exact status output: working tree already contained unrelated changes in config, docs, engine, gamemode, main systems, and network files.
- Files not created or modified by this session: all pre-existing files outside `src/network/server.cpp`, `src/network/server-npcs.cpp`, `src/npc/npc.cpp`, this changelog, and the cold-build record.

# Requested behavior

Investigate the reported 3.8 Hz server rate with approximately 56 NPCs, identify the first expensive owner, and add runtime evidence sufficient to distinguish NPC simulation from snapshot/network work.

# Specification alignment

- Current specification paths: `docs/specs/networking/networking.md`, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`.
- Exact requirements: authoritative gameplay remains fixed at 60 Hz; diagnostics use the existing `StructuredLogger` and canonical `events.jsonl`; build evidence is separate from runtime and human acceptance.
- Why the change follows the specification: it preserves the fixed tick and snapshot contract while removing unbounded important-level per-tick diagnostics and adding one bounded stage summary per second.
- Conflicts or decisions: NPC snapshot batching was not implemented because source and journal evidence identified logging/flush pressure as the first likely bottleneck; stage timings will falsify or confirm that conclusion in a fresh run.

# Exact implementation changes

## `src/npc/npc.cpp`

- `emitMovementDecision`: per-NPC/per-fixed-tick `npc.movement-decision` is now `VERBOSE`, so the normal important journal cannot turn movement observation into a blocking flush workload.

## `src/network/server-npcs.cpp`

- `simulateSharedNpcs`: the once-per-second per-NPC position dump is now one aggregate count/living/dead line.
- `makeNpcEntity`: removed the per-snapshot dead-NPC `printf`; snapshot health and all wire fields are unchanged.

## `src/network/server.cpp`

- Added bounded `performance.server-tick-window` records with total, NPC simulation, snapshot/send, and gamemode average/max milliseconds plus player/NPC counts.
- No simulation ordering, fixed delta, packet schema, or send rate changed.

# Diagnostics

- Owner/category: authoritative server fixed-tick loop; `PERFORMANCE`; canonical `events.jsonl`.
- Input: each real fixed server tick and current player/NPC counts.
- Output: one aggregate timing event per wall-clock second.
- Rate limiting: movement diagnostics are no longer important-level flushes.

# Validation

- Focused skill paths: `docs/skills/logging-checker-v1.md`, `docs/skills/efficiency-checker-v1.md`, and `docs/workflows/runtime-scenario-validation.md`.
- Commands: `git diff --check`; `python build_agent.py`; `mimita.exe --versioninfo`.
- Build: SUCCESS; changed server/NPC units compiled and linked.
- Version-info journal: `logs/10-08-2026/20261008_141949/events.jsonl`.
- Live acceptance: not performed; the existing user-owned server was an older `.dev\\builds\\1694\\mimita.exe` process and was not restarted.

# Measured evidence

- Before: user report `[SERVER PERF] hz=3.8 loopAvg=1301.589ms`; inspected 14:11 server run had 2,945 `npc.movement-decision` and 156 `npc.nav-plan-failed` events.
- After: not yet measured in a fresh Zombie Tower run on the changed executable.
- Next evidence: compare `performance.server-tick-window` stage timings and client snapshot arrival against the 3.8 Hz baseline.

# Regression review

- Regression entry appended: no; this is a user-reported performance failure, not a newly introduced regression.

# Human acceptance

- Visual, gameplay, and multiplayer acceptance remain unverified.
