# One-source DLL compile/link trial

- Added a behavior-neutral measurement comment to `src/effects/zombie-tower-effects.cpp` after queuing build note `59b04f6eb844`.
- Root dev-loop request `2026-11-21T21:50:15.046Z-84e61847`, attempt `game-dll-20261121T215015817067Z-d501fb`, succeeded and published generation 433.
- The 158-byte source compiled in 36.304 s; the 11 generation-identity wrappers also rebuilt; 630 objects were skipped/reused. Cache scan took 4.903 s, GNU link 61.718 s, outer attempt 395.540 s.
- Host sample during pre-worker snapshot: CPU 98.4% average over 12 logical CPUs, 1.27 GB RAM available. This sample did not cover the compile/link phases, so it does not establish their individual resource cause.
- The publish phase took 29.101 s, including 27.793 s to reparse 642 depfiles. Worker `publish_seconds` and `total_seconds` underreported outer time by about 35.9 s.
- The source save also led to automatic request `auto-1`, which built no new DLL and completed no-op on generation 433 after 377.503 s of repeated preparation. It was left to the root dev-loop.
- No EXE build or explicit game launch was requested. Full evidence and follow-up proposals: `docs/reports/20261121_220437-one-source-compile-link-trial.md`.
