# JSONL Root Path Fix

Time: `2026-09-29T13:13:49Z`

## Request

Keep the v9 collision investigation in the v9 repository logs. Do not route
the event stream to the old v8 checkout or to an executable-adjacent folder.

## Change

`src/debug/structured-log.cpp` now resolves the default structured-log run
directory from the current v9 working directory. A timestamped executable in
`.dev/builds/<id>` therefore writes to:

`C:\mimita-v9\logs\MM-DD-YYYY\<run>\events.jsonl`

The existing `MIMITA_EVENTS_FILE` override remains available for an explicitly
shared client/server destination.

## Evidence

- Existing v9 runtime text output showed the previous incorrect destination:
  `.dev/builds/0381/logs/2026-09-29/20260929_090538/events.jsonl`.
- `python build_agent.py` completed with `Status: SUCCESS`.
- `.dev/builds/0382/mimita.exe --collision-selftest` returned
  `[COLLISION SELFTEST] PASS`.
- Live human verification is still pending because no game process was running
  to produce a new `logger.started` record after the path correction.

## Related regression

`docs/regressions/2026-09-29/slope-edge-snag-REG.md`
