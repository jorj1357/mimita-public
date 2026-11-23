# Source-only marker validation microbenchmark

- Session ID: session-1dce9820fb67
- Goal: continue the active Mode 3 edit-to-live latency objective; not achieved.
- No compiler, build worker, dev-loop build request, or game process was started.
- Read-only benchmark used the generation 554 object cache: 642 marker objects,
  55 external dependencies. Twelve calls to the implemented
  `source_only_cache_marker_valid()` all returned true.
- Each call took 65.936–114.492 ms; median 85.700 ms. The synthetic source
  revision/path exercises the source-only eligibility and prior-marker checks,
  assuming changed-TU freshness was already proven. It does not include
  compilation, link, publication, runtime activation, or full root-dev-loop
  setup.
- This is a stage microbenchmark, not an integrated build result. The previous
  full object-cache scan was 2.525 s, changed-source compile wall was 20.516 s
  for the 15-source batch, link was 3.840 s, and request-to-server activation
  was 46.387 s. No estimate is reported as an observed end-to-end improvement.
- The active run's journal tree remains at 6,999,999,734 / 7,000,000,000
  bytes, so I preserved the running pair and deferred live build integration.
- Validation command: inline Python read-only benchmark of
  `game_build_worker.source_only_cache_marker_valid`; 12/12 valid results.
- Runtime/build proof: none for this microbenchmark; generation 554 is only the
  source cache measured, not a new activation.

Next measurement after canonical runtime journaling is writable: request one
source-only build through the root dev-loop, record the real scan mode/time,
then repeat with the matched 15-source batch and verify activation in both
existing process journals.
