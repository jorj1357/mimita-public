# Canonical scenario capture had two sinks

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-03T19:09:00Z

Status: ATTEMPTED FIX (2) — full scenario file restored under
`devscripts/scenarios/user/`; journal keeps compact summaries only; runtime
review pending

Correction 2026-11-03: journal-only canonical capture made recordings hard to
find and let log quota failures abort them. The full recording now lives in
`devscripts/scenarios/user/` with the server sidecar merged at stop. The journal
keeps only start/stop/invalid summaries. `devscripts/scenario_export.py`
recovers the journal-only recordings already captured.

Related specification: `docs/specs/dev-loop/scenario-input-recording.md`,
`docs/specs/debug-logging/debug-logging.md`
Related changelog: pending for this session.

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T19:09:55Z`

Journal: `logs/2026-11-03/20261103_191001/events.jsonl`

### Expected Behavior

Each executable writes one canonical `events.jsonl`. Canonical scenario capture
writes every fixed tick to that journal. There is no second per-tick scenario
sink and no sidecar merge.

### Actual Behavior

The client built and wrote both a per-tick scenario JSON object and a canonical
journal record. The server canonical path wrote the journal, while the old
non-canonical path could still open a sidecar. Capture cost was also hidden
inside the generic `Replay` scope.

### Confirmed Cause

The scenario recorder retained the legacy sidecar writer after the canonical
journal path was introduced. The per-tick `tick` JSON and sidecar write were
redundant with the journal payload.

### Corrected Code

- `ScenarioRecorder::captureTick()` no longer writes the per-tick scenario file
  for canonical capture; the file is not opened.
- `writeIdentity()` and lifecycle writes are journal-only when no sidecar is
  open.
- `scenario.record.performance` is emitted every 60 frames with capture cost.
- Scenario stop flushes the journal.

### Proof

Source change only at this time. Runtime proof must show contiguous
`server_tick`/`client_tick` ids, no `scenario.record.capture_failed`, and
capture build+write p99 within the agreed budget.
