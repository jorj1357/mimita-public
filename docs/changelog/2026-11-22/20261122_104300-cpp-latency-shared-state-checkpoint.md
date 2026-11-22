# C++ latency goal checkpoint: shared build state and fail-open boundary

- Recorded: 2026-11-22 10:43 UTC
- Work session: `work-9048ff11df0e`
- Agent: `cpp-latency-linker-investigation`
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Active goal: reduce measured C++ edit-to-live latency without weakening source freshness, DLL publication, or runtime proof.

## User direction

The user asked to keep pursuing this goal across repeated continuation turns and to make optional failures stop failing the whole build. The exact quote is already recorded as context `ctx-fc9b2656cbcd`; it was linked by prior issue intake to issues `issue-91e3384b219b`, `issue-136d4553ff92`, and `issue-ae248907f1f1`.

## Current evidence

- Latest available no-op game-DLL attempt: `.dev/build-timings/game-dll-20261122T102920924092Z-06eba6.json`.
- Request-to-result time: 13.484s; worker time: 2.000s; object-cache scan: 0.125s; compiled 0 of 642 translation units; skipped 642; link and publish time were zero.
- This leaves about 11.5s outside the worker by subtraction. Earlier same-day phase readings attributed time to root manifest/setup, source enumeration, and worker startup/output. Those detailed readings predate the concurrent source-manifest and build-path edits now in the checkout and must be repeated before selecting the next optimization.
- Dev-loop status showed the shared server PID 26748 alive in run `20261122_011031`, latest generation 486. The working tree also contains concurrent build-script, source-manifest, codegen, and C++ edits. No build was requested here because it would combine those unrelated edits into a poor comparison baseline.

## Failure policy confirmed

The optional presentation-DLL subprocess inside EXE setup may fail open: retain its failure, elapsed time, and retry state, then continue the required EXE build. Required compilation, linking, generated-code consistency, source coherence, cache freshness, and candidate game-DLL publication remain fail closed. Publishing a stale or partial DLL would make a failed edit appear active.

The fail-open implementation is documented in `docs/changelog/2026-11-22/20261122_071623-optional-dll-fail-open.md`. Its failure fingerprint uses listed source/tool inputs and metadata; transitive-header coverage remains a known risk. The issue discussion now calls for separate forced-failure checks of the optional presentation DLL and the required candidate DLL, while ensuring the old published generation remains intact after the latter fails.

## Coordination and next step

Updated the existing latency and continuous-goal issue discussions with the no-op timing and failure boundaries. The shared checkout has active agents and dirty compiler inputs; do not request another build until the root dev-loop and these edits reach a stable point. Then capture a fresh no-op and one-source attempt with full pre-worker stages, matching build identity, and concurrent process CPU measurements. Continue from the largest measured stage and retain this goal as active until the end-to-end latency target and live activation evidence are met.

No compiler or game build was invoked in this checkpoint. No runtime behavior or activation is claimed.
