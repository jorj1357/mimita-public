Time created: 2026-11-03T19:55:00Z
Time last updated: 2026-11-03T19:55:00Z

Purpose:
Remove the uncommitted headless-NPC template body colliders that caused 60 Hz
stalls, floating/reset behavior, and phantom penetration; make canonical
scenario capture journal-only; bound journal flushing; and add domain regression
master ledgers.

## What changed

### NPC behavior
- `src/network/server-npcs.cpp`: removed `ensureNpcTemplateCollisionBody()` and
  its three call sites. Kept the tdmtest3 solid-id fallback in
  `buildNpcWorldCollision()`.
- `src/physics/movement/actor-final-pose.cpp`: removed the uncommitted
  per-candidate `npc.final-pose.safe-point.begin/end` diagnostics, restoring the
  bounded safe-point search without per-candidate journal writes.
- `src/npc/npc.cpp`: unchanged behavior; the existing containment path now
  skips cheaply for headless NPCs with no avatar collision parts.

### Logging
- `src/debug/structured-log.cpp` and `.h`: added `flushEvents()`. Accepted rows
  are appended without a per-row `fflush`; `tick()` flushes at most once per
  100 ms. Scenario stop paths flush explicitly.
- `Important` and checked scenario writes no longer flush individually.

### Scenario recording
- `src/devtools/scenario-recorder.cpp`: canonical client capture no longer opens
  or writes a per-tick scenario sidecar. It writes only the executable journal,
  keeps lifecycle state in memory, and emits `scenario.record.performance` every
  60 frames.
- `src/devtools/scenario-server-recorder.cpp`: canonical capture emits
  `scenario.record.performance` every 60 frames with build/write/total times.
- Scenario stop flushes the journal before ending capture.

### Regressions
- Added `docs/regressions/master/` with ledgers for collisions, performance,
  networking, NPC movement, dev loop, and gameplay, plus `README.md`.
- Full-backfilled all 83 existing regression records into the master ledgers.
- Added detailed records:
  - `2026-11-03/npc-template-body-collision-REG.md`
  - `2026-11-03/server-tick-budget-REG.md`
  - `2026-11-03/tdmtest3-npc-world-fall-REG.md`
  - `2026-11-03/scenario-journal-only-REG.md`
  - `2026-11-03/surface-snagging-REG.md`
- Updated `docs/regressions/README.md` and `docs/ROUTER.md`.

### Specifications
- `docs/specs/npc-behavior/npc-behavior.md`: headless NPCs use the shared
  capsule path plus bounded ground clamp; no template body colliders.
- `docs/architecture/collision/collision.md`: documented the same asset
  boundary and tdmtest3 fallback.
- `docs/specs/dev-loop/scenario-input-recording.md`: canonical capture is
  journal-only; no sidecar.
- `docs/specs/debug-logging/debug-logging.md`: bounded 100 ms journal flush.

## Validation

- `python build.py build-only`: BUILD SUCCESS (7 files compiled, link OK).
- Full `agent-gate.py`: build, focused selftests, lint, JSON sync passed in the
  earlier run; the second run was aborted by the user after the scenario
  `npc_wall_containment_v1` passed and the next scenario began.
- No human visual acceptance was performed.
- No runtime tdmtest3 60 Hz proof was completed in this session.

## Not claimed

- 60 Hz and NPC walking/sliding behavior are source-corrected but not yet
  runtime-proven.
- Surface snagging remains unresolved; the first divergent contact has not
  been traced.
- Scenario playback of new journal-only recordings has no stop-time export yet.
