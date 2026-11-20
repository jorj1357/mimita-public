# issue-f8b80ddaea17: Launch failure: launch_exception at launch_exception

## Human report (verbatim, immutable)

> TypeError: expected str, bytes or os.PathLike object, not NoneType while creating the client; the fresh client command-queue path was unset after server startup.


## Agent suggestion

### 2026-11-20T09:30:34.371Z — launch-latest-stable-fix

Repair evidence: original launch report 2026-11-20T09-05-30.685Z was still running after a TypeError immediately following server_gameplay_readiness. Reconstructed it from matching run 20261120_090530_01 daemon journal; report now terminal failed with launch_exception/TypeError, last_completed_stage=server_gameplay_readiness, and this issue ID. The catcher is now implemented in current source; normal post-restart GUI launch 20261120_092642 succeeded, but no natural exception recurred to exercise the catch branch.
