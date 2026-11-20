# issue-63dccbd8f8b6: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-20T08:40:30.769Z — launch-latest-stable-fix

Build friction evidence: game-dll-20261120T083155607100Z-c98479 generation 399 succeeded in 190.043 s. Worker object-cache scan alone took 87.589 s (scan parse 79,977 ms), plus preflight 25.269 s, compile 50.658 s, link 31.15 s. This exceeded the 60-second wait budget; build succeeded but no behavior proof until generation 399 runtime is exercised.

## Agent suggestion

### 2026-11-20T08:47:26.322Z — launch-latest-stable-fix

Recurrence in current integration build game-dll-20261120T084525661582Z-9bd491, generation 400: changed-source preflight completed at 08:45:59.980Z after 17.056 s, then object_cache_scan remained active beyond 60 s; at 08:47:12.740Z scan snapshot elapsed 106.8 s. Previous same-day attempt measured object scan 87.589 s, so the repeated >1-minute scan is an avoidable build-friction recurrence. Build is root-dev-loop owned and still in progress; inspect scan snapshot/timings after completion.
