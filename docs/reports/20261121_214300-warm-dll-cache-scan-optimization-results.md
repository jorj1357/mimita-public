# Warm DLL cache scan optimization results

## Outcome

Two safe memoization changes were made in `devscripts/game_build_worker.py`:

1. Cache dependency classification and project-relative path resolution once per distinct depfile token, rather than repeating `is_external_dependency` and `dependency_relative` (which calls path resolution) for every translation unit occurrence.
2. Cache dependency `stat` results by distinct token during object freshness checks, so repeated includes do not repeatedly construct and normalize `Path` values before reaching the existing stat cache.

Freshness comparisons, missing-dependency behavior, PCH checks, object checks, compile selection, linker mode, and DLL publication were not weakened or changed. Both edits were exercised through the root dev-loop worker. The current completion marker was validly regenerated after each forced cache-validation pass.

## Before/after measurements

| Measurement | G432 baseline | First optimized forced scan | Second optimized forced scan |
|---|---:|---:|---:|
| Attempt ID | `game-dll-20261121T201417234689Z-2d9df4` | `game-dll-20261121T213722107270Z-52f7a2` | `game-dll-20261121T214024518362Z-449ec0` |
| Worker object-cache scan wall | 98.861 s | 32.248 s | 1.033 s |
| Dependency prepass | 85.309 s | 1.585 s | 0.669 s |
| Dependency path/fanout walk | 84.749 s | 0.877 s | 0.475 s |
| Dependency path normalization/resolution | Dominated the prepass | 0.821 s | 0.441 s |
| Object freshness checks | 13.274 s | about 30.4 s | 0.164 s summed per-object checks |
| Freshness-check phase wall | Included in scan | about 30.4 s | about 0.181 s |
| Compiled / skipped | 12 / 630 | 0 / 642 | 0 / 642 |
| Link | 11.022 s | Not run | Not run |

The first optimization removed repeated path classification and canonicalization. That reduced the prepass from 85.309 s to 1.585 s, but exposed repeated path normalization inside freshness checks as a new 30-second bottleneck. The second optimization removed that repetition: the freshness pass checked 1,698 unique dependency tokens and completed in about 0.164 s of summed dependency checks. The complete object scan was 1.033 s, a 95.8× reduction (about 98.95%, or 97.828 s saved) versus G432.

The first separate request, `2026-11-21T21:33:20.596Z-29fe5ab4`, took the complete-marker no-op path and did not exercise the scan; it took 115.424 s worker-attempt total, almost entirely before the worker. The forced scan attempts were no-op DLL builds: they verified all 642 cached objects, compiled zero units, linked nothing, left generation 432 active, and refreshed the cache marker. The second forced-scan attempt took 111.153 s overall; the third took 98.479 s worker total and 98.919 s at the dev-loop boundary.

## What this establishes and what it does not

**Established:** the warm dependency scan can fall from about 99 seconds to about one second without skipping dependency freshness checks, using memoization over the small set of unique paths. The dev-loop accepted and completed all requests successfully. `python -m py_compile devscripts/game_build_worker.py` and `git diff --check` passed.

**Not established:** this sample did not compile C++ or link a DLL, so it does not show a faster compile, faster link, changed generation, or live NPC behavior. The build output explicitly reports `no_op=true`, `compiled=0`, and generation 432. Do not treat the 98.919-second no-op attempt as comparable to G432's 172.333-second edit attempt: the latter compiled 12 units and linked a new DLL.

## New dominant cost: preparation before the worker

On the last forced scan, the named dev-loop stages were: resident-state codegen 0.695 s, packet-registry codegen 3.458 s, manifest snapshot 48.146 s, manifest validation 0.034 s, and source snapshot 24.252 s. These named stages sum to 76.585 s; about 20.7 s of the 97.268-second pre-worker interval is not assigned to these stage duration fields. The overall attempt was 98.479 s even though the actual worker took only 1.211 s. A prior request in this same experiment had a 29.806-second manifest snapshot and a 17.142-second source snapshot, showing large run-to-run variation.

This makes pre-worker accounting and manifest/snapshot work the immediate end-to-end bottleneck. The attempt did not capture aligned CPU, memory, or disk measurements, so attributing the long and variable snapshots to the PC, antivirus, storage, or filesystem load would be speculation. Capture full process-tree and system resource samples on the next run.

## Recommended next experiment

Use one controlled one-`.cpp` edit to force a real compile and full DLL link through the same root dev-loop. Record exact save-to-worker, pre-worker stage boundaries, scan, compiler, link, publish, and activation times. Compare against G432's 14.177-second `player-ai.cpp` compile, 18.425-second compile-batch wall (including 11 identity-invalidated wrappers), and 11.022-second link. In the same attempt, verify the new scan remains near one second and record whether wrapper churn persists. The runtime should be an existing pinned process; do not start another game instance.

Then optimize in this order:

1. Eliminate repeat manifest/source-snapshot work for unchanged files, while retaining content integrity and an uncertainty fallback. Target under one second for warm pre-worker setup; report any residual explicitly.
2. Remove compile-time generation/revision invalidation from folded whole-game wrappers, retaining standalone module identity validation. Target zero wrapper compiles for a leaf `.cpp` edit.
3. Profile actual edited TU compilation. Keep the current PCH until measurements show whether it helps. Splitting should be accepted only when it reduces the hot TU's include/template cost without adding more rebuild or link work.
4. Benchmark a real one-object incremental link or Blink patch path separately. The existing GNU full link remains about 11 seconds; this scan optimization does not affect it.

## Integrity and scope

The test deliberately altered only the generated `complete.json` cache marker revision to make the dev-loop validate dependencies again. The worker rewrote it with the correct source revision and generation after the successful pass; the final file reports generation 432 and revision `34dcd763673bcdefec8ee47995ddeaa575c40438882b45a54270d16656b6fde7`. No C++ inputs were modified for these runs. No compiler, build script, or worker was launched directly; no EXE build or additional game process was started. The active compiler and game architecture were not changed.
