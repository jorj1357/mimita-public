# Resource-pressure server setup validation method

- Reviewed the existing `issue-be39a4f5ee17` runbook and the isolated test-run owner (`devscripts/dev_loop/parallel_test_runs.py`, `devscripts/dev-loop.py`).
- Appended a code-grounded method: baseline, build-concurrent, bounded CPU, and bounded memory scenarios; pinned EXE plus pinned game generation; stage elapsed/budget events; host resource sampling; safety caps and cleanup verdicts.
- Confirmed the current gates: an unpinned isolated run is refused while an EXE build is pending, while a pinned EXE + pinned game generation is the supported test-while-building path. Fixed readiness waits are 25/30/60/45/45 s and currently report a pressure timeout like a hard failure.
- Recorded live state at 2026-11-21T16:00Z: `game_build.status=building` (generation 423, source generation 1), `build_pending=false`, `build_status=published_stale`. That is a natural build-concurrent window; baseline and injected-pressure rows remain `NOT_TESTED` while free physical memory is below the 2 GB reserve.
- Consolidated tasks after a mutual cancellation race with the 15:51 opencode session: final open tasks are `rpsv-host-sampler`, `rpsv-bounded-load`, `rpsv-stage-timing`, `rpsv-proof-matrix` (unassigned), and `server-load-baseline` (opencode). `server-load-safety-gate` and `server-load-pressure-matrix` remain cancelled as duplicates.
- No code was edited, no build was requested, and no isolated or stress run was started. Source, runtime, and human acceptance evidence remain separate and unclaimed.
- Next: human approval to implement the bounded sampler/load helper and stage-timing classification; then run S0-S3 on one pinned artifact when host reserve is restored.
