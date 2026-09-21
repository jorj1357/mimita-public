# v2.1.0 live collaboration foundation and inspection commands

Date: 2026-09-20
Status: foundation implemented; distributed runtime integration remains

## Implemented

- Added generic resource revision and operation types with explicit states.
- Added a priority-aware in-process live operation queue that emits JSONL
  transition events.
- Added server-side revision acceptance, stale-base draft preservation, active
  revision lookup, and rollback pointer behavior.
- Added bounded live revision packet definitions 83 through 89.
- Added `LiveProbe` and `LiveProbeScope` forwarding structured values and
  durations to the existing `events.jsonl` logger.
- Added hot-reloadable probe rules to `config/debuglogger.json`.
- Added `live queue`, `live revisions <resourceId>`, and
  `live rollback <resourceId> <revisionId>` terminal commands.
- Added live-collaboration, revision-protocol, and JSONL-probe specifications.

## Validation

- Canonical `python build_agent.py`: SUCCESS.
- Built executable: `mimita-20260920T111304.exe`.
- `mimita-20260920T111304.exe --live-code-selftest`: PASS.
- `git diff --check`: no patch errors; existing line-ending warnings remain.
- No running MiMITA process was present during the build.

## Not claimed complete

- Revision packets are defined but not yet wired through client/server receive
  handlers.
- File watchers do not yet automatically create revision proposals.
- Artifact transfer/publication is not yet driven by resource revisions for all
  JSON and binary asset kinds.
- Probe sample intervals and slow-only thresholds are parsed but not yet
  enforced by the probe emitter.
- The full two-client simultaneous-edit, preserve-draft, activate, and rollback
  acceptance test remains pending.
