# Build time and cold-build frequency

Time created: 2026-11-08T22:45:00Z
Time last updated: 2026-11-08T22:45:00Z

Status: UNRESOLVED — build time is still far above target

Related specifications:
- `docs/architecture/live-development/module-migration-plan.md`
- `docs/architecture/live-development/self-contained-workspace.md`
- `docs/specs/hotreload/hotreload.md`
- `docs/specs/dev-loop/agent-lanes-build-authority.md`

Related changelog:
- `docs/changelog/2026-11-08/20261108_202357-lane-build-ledger-and-module-private-headers.md`

---

## Regression Occurrence 1

### Observed

Date: 2026-11-08

A lane host rebuild took roughly **20 minutes** and was still compiling. Earlier
attempts took about 26 minutes before failing. The build was a 588-file debug
rebuild after shared-header changes.

Measured contributing facts:

- `map_common.h`: **276** dependent translation units.
- `map-authored-geometry.h` / `map-config.h`: about 61 dependents.
- Debug flags: `-Og -g1`.
- Parallel compilers: 4–8, depending on lane configuration.
- Free physical memory dropped to roughly **0.6–1.1 GB**.
- Two legacy outside lanes were compiling at the same time because each had its
  own scheduler root.
- The build log is written only when the build completes, so the dashboard can
  look frozen while compilers are active.

### Expected Behavior

- Normal module/private-header/config changes: **no EXE build**.
- Ordinary incremental host changes: seconds, not tens of minutes.
- Cold/warm boundary build target: **2–3 minutes**.
- Ideal: effectively 0 seconds for normal work, with a cold EXE boundary only
  for kernel/process/device/transport/packet-schema/loader/public-ABI changes.

### Actual Behavior

- A wide-header change forces a large fraction of the 588 TUs to recompile.
- Multiple lanes can build simultaneously because legacy lanes use separate
  scheduler roots.
- Memory pressure turns 4–8 parallel debug compilers into tens of minutes.
- Build-time history was not recorded in a machine-readable tracker.

### Confirmed Cause

1. Wide-header fanout, especially `map_common.h`.
2. Scheduler root divergence across legacy lanes.
3. Too few compiler jobs and low free memory.
4. No persistent build-time/cold-build counter.

### Corrected Code / Mitigation

Implemented in the lane:

- OS-level single-EXE-build mutex (`Local\MiMITA-EXE-Build`) in
  `devscripts/build_exclusive.py`, acquired by `build.py`.
- Shared coordinator scheduler root resolution in `devscripts/build_scheduler.py`.
- `MIMITA_COORDINATOR_ROOT` propagation from `agent-lane.py`.
- Lane dev-loop scheduler-root marker and restart on mismatch.
- Default EXE jobs raised to 8; maximum 12.
- GLB authored v2 state moved to the narrow
  `src/map/map-glb-authored.h` sidecar instead of `map_common.h`.
- Build metrics appended to `build/build-metrics.jsonl` and latest timing in
  `build/build-timing.json`:
  - elapsed seconds
  - compiled/skipped counts
  - cold-build flag
  - jobs
  - lane
  - scheduler root
  - link reason

### Proof Required

- Two lanes attempt a build; only one EXE build compiles; the other waits.
- `build/build-metrics.jsonl` records every successful/no-op build.
- A GLB-authored edit recompiles only the narrow sidecar consumers, not 276 TUs.
- A warm incremental host build target: under 2–3 minutes.
- No-op: under a few seconds and no new artifact.
- Normal module/private-header/config edit: no EXE artifact change.

## Build Time Tracker

Machine-readable history lives at `build/build-metrics.jsonl` (gitignored).
Each line records `elapsed_s`, `cold_build`, `compiled`, `jobs`, `lane`,
`scheduler_root`, and `result`.

Human summary:
- 2026-11-08: ~20 minutes (slow, unacceptable).
- Target: 2–3 minutes for a warm boundary build.
- Ideal: effectively 0 seconds for normal work; cold EXE builds only for
  genuine kernel/ABI/process boundaries.
