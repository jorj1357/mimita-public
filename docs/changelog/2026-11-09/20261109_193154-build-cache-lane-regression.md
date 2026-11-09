# Build cache lane wiring and stale progress regression

- Date: 2026-11-09 14:31 EST
- Scope: document the supplied 11m40s cold lane build, clarify ccache wiring,
  and align module-worker cache/temp setup with the lane dev-loop.

## Changes

- Added the user-provided build trace and cache counters as Occurrence 3 in
  `docs/regressions/performance/build-time-REG-2026-11-08.md`.
- Updated the performance regression index and gold build reference with the
  distinction between a stale last-file label and actual compile progress.
- Configured `module-build-worker.py` subprocesses to use the coordinator's
  contained `.dev/ccache`, lane-root `CCACHE_BASEDIR`, and lane-local temp
  directory, matching the EXE lane's cache setup.
- Corrected the self-contained workspace guide from the obsolete
  `.dev/shared-agent-ccache/` path to `.dev/ccache/`.
- Audited hardcoded paths in the lane/build workflow. Lane, scheduler, cache,
  and temp roots are derived from coordinator/worktree roots. Remaining
  absolute paths are external machine toolchain/GLFW fallbacks and an optional
  dashboard font; these do not define lane build or cache roots.

## Evidence and limits

- Supplied trace: 699.798 seconds, 624 compiled, 0 skipped, cold build, link
  started at 19:03:25Z and succeeded at 19:03:41Z.
- Supplied ccache counters: 0 hits, 2,466 misses, 35 errors. The shell
  `ccache -p` output was not proven to be the lane worker's effective config.
- Source inspection: the EXE lane already passed shared cache settings;
  module builds did not. The module-worker environment is now configured but
  has not been exercised in a build.
- No build or runtime test was run. The regression records the full-build
  scope and apparent-stall evidence separately from the earlier priority/jobs
  diagnosis; it does not attribute the cold rebuild to low priority.

## Preserved working tree

Other pre-existing source, config, documentation, and Blender submodule edits
were left intact.
