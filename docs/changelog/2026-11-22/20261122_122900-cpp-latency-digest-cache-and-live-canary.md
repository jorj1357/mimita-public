# Event-backed source digests and one-TU live canary

## Objective and preserved state

Continue the edit-to-live latency investigation using the shared root dev-loop. The existing MinGW toolchain, LLD selection, game architecture, EXE build path, object cache, and running game server were preserved. No second checkout or build authority was used. The comment-only C++ canary is a latency measurement touch, not a gameplay change.

The active long-running goal remains unproven. The gold handoff already covers continuation across activations and optional-tool fail-open behavior; this session appended the new evidence to the existing latency and goal-continuity issues.

## Source digest cache change

`devscripts/source_manifest.py` now maintains an in-process source/header event cursor. Warm snapshots reuse cached digests without file stats for paths with no filesystem event. A touched `.cpp` or header is statted and rehashed even when its size and timestamp match the cache. A process restart, unavailable watcher, or event-journal overflow validates every cached stamp; mismatches are rehashed. If an event arrives while a snapshot is being assembled, the snapshot retries before returning. A lost watcher history falls back to a full inventory scan.

Overflow-path testing found an existing telemetry exception: `_cached_inventory` set `changed_directories` to `None` for watcher-history loss and then called `len(None)`, which could fail manifest creation. The fallback now reports `watcher_history_unavailable` and returns full-scan telemetry instead of raising.

The test fixture now verifies warm zero-stat digest reuse, same-size/same-timestamp edit invalidation, restart baseline validation, watcher-overflow fallback, and add/rename/delete inventory updates.

## Verification

- Python 3.14 `py_compile` passed for the source manifest and cache test.
- Python 3.14 focused run passed: **17 tests**, including the source-cache, watcher, and game-build snapshot suites.
- `git diff --check` passed for the changed files.
- After the final watcher-fallback fix, the root dev-loop was restarted through its control path. The existing game server PID 26748, run `20261122_011031`, remained healthy and was not relaunched.

## Measured results

| Attempt | Result | Key timing |
|---|---|---|
| `2026-11-22T12:27:14.988Z-27601aba`, `game-dll-20261122T122715155098Z-bc1729` | No-op, generation 490; 642 skipped, 0 compiled or linked | 11.438 s after daemon restart; manifest 6.078 s while validating persistent inventory/stamps; 2,225 stats and 0 file hashes |
| `2026-11-22T12:27:54.243Z-1648b4c6`, `game-dll-20261122T122754546805Z-8ee42b` | No-op, generation 490; 642 skipped, 0 compiled or linked | 6.125 s warm; manifest 0.938 s; 2,225 event-cache hits; 0 stats and 0 hashes |
| `2026-11-22T12:12:28.232Z-36a229a2`, `game-dll-20261122T121228440102Z-3aba91` | Published generation 490; 641 skipped, 2 compiled | 80.609 s build attempt; existing server activated generation 490 81.797 s after request acceptance |

The canary edited one comment in `src/effects/zombie-tower-effects.cpp` (239 bytes). The root build compiled that TU in **30.209 s**, plus the generated identity provider in **1.526 s**. The journal for PID 26748 recorded `game.generation.activated` at `2026-11-22T12:13:50.029Z`, DLL `build/game/generation-490/game-g490.dll`, old generation 489; subsequent `game.hotness.summary` events reported generation 490. This proves a comment-only C++ edit compiled, published, and activated live. It does not prove a gameplay behavior change.

The build trace exposed substantial previously hidden work before the compiler:

- Source snapshot: 2,899 hardlinks across 588 directories; snapshot metadata recorded 1.148 s directory preparation, 9.999 s hardlink creation, and 1.816 s walking/path preparation.
- Snapshot pruning: deleting one old snapshot took **11.248 s**. The snapshot contains 2,899 linked files across 588 directories.
- Manifest: 1.049 s in the changed-source attempt after digest reuse.
- Worker launch to first output: 3.386 s.
- Changed-source preflight: 30.515 s, including the 30.209 s canary compile.
- Object-cache scan: 2.978 s; 641 translation units skipped.
- LLD link: 10.102 s for 642 objects, 666 arguments, and 480,395,963 input bytes.
- Publish: 0.737 s.

Ordinary source compilation is forced to include `src/pch.h`; `src/pch.h.gch` is 121,165,108 bytes. The changed canary's depfile contains 194 entries. A prior worker comment records that the 258-byte identity TU took 5.26 seconds with the full PCH and bypasses it now, taking 1.526 seconds. PCH load cost is a strong hypothesis for the tiny canary's 30.209 seconds, but the compile-time process CPU was not sampled concurrently, so this attempt does not separate PCH cost from system pressure.

A separate five-second sample after the build reported 12 logical CPUs, host CPU 61.8%, server PID 26748 at 473.1% of one logical core (39.43% of total host capacity), dashboard 2.61% of host, and dev-loop daemon 0.42%. The server journal showed 19 NPCs and 0 players in Fun Counter Strike mode. This is measured CPU pressure, but it is not the requested Juggernaut/AI-controller workload and was sampled after compilation, so it does not establish why the compiler took 30.209 seconds.

## Next highest-value work

1. Reconcile the open coordination conflict on `devscripts/game_build_worker.py`, then compare full-PCH and per-TU lightweight/no-PCH compilation through the root dev-loop.
2. Reconcile the open conflict on `devscripts/dev_loop/game_build.py`, then prototype immutable base snapshots with small changed-file overlays or move snapshot pruning off the pre-worker path with separate measured housekeeping. A single delete cost 11.248 s; creating the snapshot cost about 13 s.
3. Sample host/process CPU concurrently with compilation in the requested Juggernaut/AI workload. Do not attribute compile delay to the game based on the later FCS sample.
4. Reduce the 10.102 s LLD full-link cost and 2.978 s object-cache scan while preserving the prior generation on failure.
5. Run a real NPC behavior edit after those changes and prove the resulting behavior, not only generation activation, in the exact live journal.

