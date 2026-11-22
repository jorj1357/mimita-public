# C++ latency watcher follow-up and warm no-op measurement

## Scope

Continued the existing source-manifest watcher optimization. No C++ inputs, compiler choice, game architecture, EXE build path, or running game instance were changed. Builds were requested only through the root dev-loop.

## Changes

- The directory watcher now ignores events below paths that source inventory excludes (such as `.dev`, `build`, and `.git`) so unrelated cache/build writes do not advance the source inventory event sequence.
- Watcher/cache tests now wait for asynchronous filesystem notifications before asserting added files, and no longer require a particular cold/warm cache-hit flag on the first call. The observed event timing can legitimately make the first result a retry-backed cache hit.
- Removed an incomplete, unused hash-watcher hook that had not been connected to digest reuse.

## Verification

`C:\Python314\python.exe -m unittest devscripts.quality.test_source_manifest_watcher devscripts.quality.test_source_manifest_cache -v` passed: 3 tests.

`git diff --check` passed for the changed source-manifest and focused test files.

Root dev-loop request `2026-11-22T11:54:42.688Z-dc9e2af7` completed successfully as no-op attempt `game-dll-20261122T115442891332Z-7b03e4`, generation 489. It took 9.596 seconds end-to-end; the caller observed 9.750 seconds. There were 642 skipped translation units, 0 compiled, 0 linked, and 0 published. This validates the warm no-op path only, not changed-source latency or live activation.

Measured breakdown:

- Manifest snapshot: 4.017 s.
  - Source inventory: 26.6 ms for 1,008 paths, watcher cache hit.
  - Owner resolver: 214 ms, cache hit.
  - C++ ownership and digest: 1.552 s, 1,008 file stats, 1.283 s accumulated stat time, 0 files hashed.
  - Header inventory: 23.2 ms for 1,217 paths, watcher cache hit.
  - Header digest: 2.073 s, 1,217 file stats, 3.021 s accumulated stat time, 0 files hashed.
  - Hash cache persistence: 29.1 ms; source mirror update: 42.4 ms.
- Worker launch to first output: about 3.32 s.
- Toolchain setup: 545 ms.
- Object-cache scan: 132 ms; 642 skipped.
- Identity/cache: 42 ms; no-op decision: 19 ms.
- Compile, link, and publish: not run because the input revision was unchanged.

A previous warm no-op measured 8.418 seconds, so the latest 9.596-second result shows variance and does not establish a consistent <=10-second edit loop.

## Remaining work

The largest measured avoidable warm cost is still per-file stat validation: about 4.3 seconds accumulated across the source and header digest passes. Safely reusing digest stamps through filesystem events needs explicit overflow, restart, and same-size/same-time edit protection. After that, run a matched changed-`.cpp` request and prove activation in the running game journal. No runtime activation is claimed here.
