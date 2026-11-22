# C++ latency: warm-cache and snapshot reuse follow-up

Date: 2026-11-22 UTC

## Scope

Continued the edit-to-live C++ latency investigation. Preserved the existing root dev-loop and the healthy live client/server pair. No game source, compiler selection, game architecture, or active run was changed. No compiler or standalone game build was invoked.

## Evidence

- Existing root dev-loop no-op attempt `game-dll-20261122T000630945648Z-c2c8ee` completed successfully in 141.027s and retained generation 437. No compilation, link, publish, or generation activation occurred.
- The attempt spent 48.565s in manifest snapshot and 55.995s in source snapshot. Together these are 106.974s, or 75.9% of the total. Object-cache scan was only 0.113s and skipped all 642 source units.
- The attempt-specific no-op worker time was 0.381s. Its nested `worker_metrics` still carried a prior compile/link workload (12 TUs, 62.757s compile, 22.474s link); these are stale metrics and were not performed in the no-op. The in-source no-op metrics correction was already fixture-validated but not loaded in the daemon.
- A fresh Python interpreter's first `build_manifest` took 6.281s. The same-process six-pass run already recorded in the issue measured 8.101s cold and 0.471–0.512s warm, median 0.493s, with C++/header inventory and owner resolver cache hits.
- Five current-tree same-revision source mirror checks took 41.65–91.29ms, median 45.09ms. Five validated immutable published-snapshot lookups took 1.40–2.23ms, median 1.83ms.
- Syntax checks, packet-registry `--check`, cache invalidation coverage, helper checks, and `git diff --check` passed.

## Changes already present in the shared checkout

`devscripts/source_manifest.py` has process-local inventory and owner-resolution caches with invalidation metadata and directory-set invalidation. `devscripts/dev_loop/game_build.py` has attempt-scoped no-op metrics, unchanged-revision source snapshot reuse, and pruning protection for the reused published snapshot. Packet registry codegen avoids repeated per-name/per-line scans. The current Python daemon predates these edits, so no end-to-end speedup claim is made.

## Limits and next action

Current live runtime is run `20261121_224058`, generation 437, Payload/map `payload1`, with client PID 24192 and server PID 27816. It is not a Juggernaut/AI pressure run. Do not stop this healthy pair merely to load Python changes. The next required step is to establish a controlled daemon update/adoption that keeps and revalidates the same game pair, then repeat matched no-op and one-leaf-source trials with full process/disk/stage telemetry and same-PID activation evidence. Eleven generated-identity wrapper TUs and monolithic link time remain separate high-cost work; no ABI change or linker/compiler switch was attempted here.

## Verification of scope

The active MinGW selection, build entry path, game architecture, and runtime configuration were not changed. No full game build and no additional game process were started.

## Final shared-checkout observation

At final verification on 2026-11-22, the root dev-loop reported a different active run, `20261122_011031`, mode 45 (Fun Counter Strike), server PID 26748 and client PID 25392. The previously observed Payload run/PIDs were no longer current. This changed during concurrent shared-checkout activity; this task did not issue a launch, restart, build, or stop request. The currently reported pair was left untouched. The final status still showed generation 437 and an idle root dev-loop.
