# Dev-loop segmented-log readiness fix

- EST timestamp: 2026-10-10 19:07:03 -04:00
- UTC timestamp: 2026-10-10T23:07:03Z
- Branch: `2026-10-10-Z-Tower`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Finding

Mode 10 did launch the server correctly. The supplied run loaded
`assets/maps/zombietower4.glb`, registered room `T8FV89A`, and continued at
approximately 60 Hz with zero players. The client was never launched because
`dev-loop.py::wait_for_logger_started()` opened only the configured base path
`events.jsonl`. Recent StructuredLogger segmentation renamed the active stream
to `events-000001.jsonl`; that file contained the server's matching
`logger.started` record. The dev-loop therefore reported `server_logger_missing`
and closed a healthy server.

## Change

`devscripts/dev-loop.py:973-1003` now scans the configured base file and all
`events-*.jsonl` segments in the same run directory when checking the exact
child PID/process-role readiness event. Log rotation remains enabled; the
orchestrator now follows the segmented journal contract.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- `git diff --check -- devscripts/dev-loop.py`: passed.
- Focused readiness probe against
  `logs/2026-10-10/20261010_230558/events-000001.jsonl` for server PID `12044`:
  passed.
- Build: not run; only Python orchestration changed.
- Runtime: the supplied server run is direct evidence of the old false-negative
  readiness path. The active dev-loop was not restarted or terminated.
- Human review: restart the dev-loop, select mode 10, press `1`, and confirm
  the client launches after the server's segmented `logger.started` event.

## Pre-existing work

All unrelated Zombie Tower, entity, logging, configuration, and changelog
changes were preserved.
