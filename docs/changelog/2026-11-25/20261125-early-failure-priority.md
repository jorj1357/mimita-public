# Earlier failure reporting for changed C++ files

**Date:** 2026-11-25 UTC  
**Issue:** [issue-136d4553ff92](../../issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md)

## Change

Mixed DLL builds now queue directly changed `.cpp` translation units first in
the existing parallel compile wave, followed by stale translation units whose
depfiles reference changed headers, followed by unrelated stale units. Each
stale translation unit is still compiled once. This adds no serial preflight
wave, does not raise the worker count, and keeps the existing GCC hard-error
streaming cancellation path.

## Why

The root worker already has a 300-second absolute worker timeout and cancels a
compiler/sibling work after a recognized GCC/MinGW hard error. The extra
fail-fast opportunity was ordering: mixed batches had changed-header
dependents first, but directly edited `.cpp` files were not explicitly put at
the front. Scheduling them first should reveal errors in the user's edits
before unrelated stale work occupies the same bounded worker pool.

This does not make every failure instantaneous. An unresolved symbol is only
known at link time, and pressure can lengthen a valid compile. The 300-second
limit bounds the worker phase, not necessarily total request-to-failure time:
pre-worker stages and slot wait have separate timing/bounds. No 19-minute
attempt-wide deadline was added.

## Validation

- `python -m unittest devscripts.quality.test_game_build_failfast_priority devscripts.quality.test_game_build_fast_diagnostic_abort` — 6 passed in 0.326 s.
- `python -m py_compile devscripts/game_build_worker.py devscripts/quality/test_game_build_failfast_priority.py` — passed.
- Scoped `git diff --check` — passed.
- Root dev-loop status at 2026-11-25T03:50:57Z showed no active build and no pending batch. No build, runtime control, or DLL publication was requested.

## Remaining proof

The new order has focused unit coverage but no real failing root DLL attempt
has yet proven diagnostic-to-worker-exit latency. Observe the next naturally
failing mixed batch and compare the first hard-error timestamp with worker
termination. Do not inject a broken source into a shared build to create this
proof.
