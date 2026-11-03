# Fixed-tick stalls from NPC containment and per-record journal flushes

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-03T19:09:00Z

Status: ATTEMPTED FIX (1) — source changed; runtime review pending

Related specification: `docs/specs/performance/performance.md`,
`docs/specs/debug-logging/debug-logging.md`
Related changelog: pending for this session.

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T19:09:55Z`

Journal: `logs/2026-11-03/20261103_191001/events.jsonl`

### Expected Behavior

The authoritative server runs at a fixed 60 Hz. A fixed tick may not spend
hundreds of milliseconds on NPC containment or logging.

### Actual Behavior

- 707 `performance.server-tick-stall` events over the run.
- Server tick windows recorded `npc_avg_ms` 112–200 ms and `total_max_ms`
  587 ms.
- The diagnostic safe-point search alone averaged 17.7 ms and reached 55.3 ms.
- Every `Important` record and every checked scenario record called `fflush`
  under the cross-process journal mutex.

### Confirmed Cause

Two compounding costs were present:

1. The NPC body-collider containment path described in
   `npc-template-body-collision-REG.md`.
2. `StructuredLogger::writeEvent()`/`writeEventChecked()` flushed the journal
   on every accepted record, so per-tick diagnostics serialized on disk and on
   the shared mutex.

### Corrected Code

- Removed the NPC template body containment path.
- Removed the per-candidate `npc.final-pose.safe-point.begin/end` diagnostics.
- `StructuredLogger` now writes rows without a per-row flush and flushes at most
  once per 100 ms through `flushEvents()`, called from the existing `tick()`.
- Scenario stop paths flush the journal explicitly.

### Proof

Source and build evidence only at this time. Runtime proof must show
`performance.server-tick-window` p99 ≤ 16.6 ms, zero
`performance.server-tick-stall`, and actual Hz ≈60 over a 60-second NPC run.
