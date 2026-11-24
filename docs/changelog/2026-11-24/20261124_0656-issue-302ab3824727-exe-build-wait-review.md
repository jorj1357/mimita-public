# exe-build wait review (issue-302ab3824727)

Reviewed wait `exe-build:26:2026-11-24T06:21:08.234Z` (60.546 seconds, stage `linking`) against attempt `exe-build-20261124T012108-f9d70640`, published `2026-11-24T06:22:17.620103Z`, run `20261124_053435`.

Measured from `build/build-timing.json` and `build/build-metrics.jsonl`: total 64.854 seconds; compile 34.999; link 3.240; optional presentation DLL failed (exit 1) in 21.570; staging 0.018. The `linking` stage is the phase at detection, not the cost driver. The 03:30:44Z attempt compiled only four files yet still took 60.996 seconds, so the flat 60-second budget sits below the observed non-noop EXE floor. In the last eight recorded attempts the optional presentation DLL failed in six (10.5-32.6 seconds each), and the 05:49:50Z no-op spent 335.702 seconds in that failing sub-build.

The 63-point resource sidecar recorded host mean CPU 95.528%, peak 100%, and 535,056,384 bytes minimum available memory; runtime alignment was unavailable, so contention is not proven to have caused the compile wall.

Wake `wake-e8b7a1914af1` exists once for this issue (two launch attempts of the same request, no duplicate queue entry). The dev-loop journal for run `20261124_053435` is missing, so the `dev-loop.wait.over_budget` and reviewer-wake events were not retrievable.

Evidence and review are appended to issue-302ab3824727, the exe-build numbers are cross-linked to issue-136d4553ff92, and build-coordinator was asked to agree on the smallest next check. No source edit, build, runtime change, process control, or runtime proof.
