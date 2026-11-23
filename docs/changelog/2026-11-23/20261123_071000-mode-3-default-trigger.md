# Mode 3 default build trigger

- Changed the root dev-loop default from automatic-on-save builds to Mode 3
  pending batches. Saved compiler inputs are recorded and do not start a build
  until the root coordinator issues `python devscripts/dev-loop.py --request
  batch-build`.
- Existing explicit `build` and `game-dll` requests seal a pending batch.
  `MIMITA_BUILD_TRIGGER_MODE=auto` keeps Mode 1 available as an opt-in fallback.
- Added batch state to dev-loop status, batch sealing and completion records,
  and a dashboard control for the explicit batch request.
- Updated `AGENTS.md` and the dev-loop build-authority specs to describe the
  new default, readiness handoff, and the fact that per-agent readiness flags
  are not yet enforced.
- Verification: Python syntax compilation passed for the two changed dev-loop
  entry files; `dev-loop.py --help` includes `batch-build`; `git diff --check`
  passed. No game build was requested and no compiler was run.
- Runtime activation: the daemon was not restarted during this change because
  its shutdown path closes the managed client. The project default is on disk;
  the already-running daemon continues with its loaded pre-change policy until
  a controlled restart.
- Limit: Mode 3 currently batches and waits for a manual root seal; it does not
  collect or enforce per-agent readiness acknowledgements.
