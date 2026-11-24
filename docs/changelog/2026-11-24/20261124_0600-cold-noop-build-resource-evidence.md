# Cold no-op build resource evidence

Recorded the automatically sampled cold EXE attempt `exe-build-20261124T004336-39725a8f` in the existing DLL latency and CPU issues.

The attempt took 374.808 seconds, with 345.232 seconds inside `build_script` and 25.002 seconds unattributed. It emitted `Nothing changed. (current)` and published no artifact. Its 288-row sidecar measured host CPU at 99.989% mean and available RAM at a 475,996,160-byte minimum. This is a cold no-op observation, not a DLL latency benchmark and not causal evidence that host pressure caused the duration.

The sidecar could not align fresh server and client journals, and one process was joined to a stale run identity. The issue records that limitation and makes reliable PID/role/journal alignment a required measurement fix before drawing build-pressure conclusions. No code, compiler input, build, or runtime process was changed by this documentation update.
