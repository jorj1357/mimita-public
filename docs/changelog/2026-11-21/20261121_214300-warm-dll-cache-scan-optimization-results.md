# Warm DLL scan optimization trial

- Changed the game-DLL worker to memoize dependency classification/path resolution and dependency stat results by unique depfile token.
- Verified worker Python syntax and diff whitespace, then requested three game-DLL attempts through the existing root dev-loop.
- Two forced cache-validation passes checked 642 cached objects and reused all objects. The first reduced dependency prepass from 85.309 s to 1.585 s; a second pass measured 0.669 s prepass and 1.033 s total object scan, with 1,698 unique dependency tokens and no stale objects.
- The runs were no-op DLL builds: zero C++ compiles, zero links, generation 432 unchanged. Thus compile/link improvement and in-game behavior were not tested.
- The final attempt took 98.479 s worker total, of which worker time was 1.211 s. Named pre-worker stages totaled 76.585 s, dominated by manifest snapshot (48.146 s) and source snapshot (24.252 s); about 20.7 s pre-worker remained outside those component timers.
- Restored a valid completion marker via the worker. No C++ input, active compiler, main build path, game architecture, EXE, or game process was changed/launched.
- Full before/after evidence and next experiment: `docs/reports/20261121_214300-warm-dll-cache-scan-optimization-results.md`.
