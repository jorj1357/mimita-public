# Mode 3 batch-build specification

**Date:** 2026-11-23 UTC  
**Issue:** `issue-136d4553ff92`

## Work recorded

- Replaced the previous save-triggered Mode 3 proposal in
  `docs/specs/dev-loop/dev-loop.md` with a coordinated batch lifecycle:
  collect saved edits without compiling, wait for affected owners to mark ready,
  seal an exact revision, build once through the root dev-loop, publish and
  activate safely, then run pinned proof after owned compiler/linker children
  exit.
- Specified Mode 1 as the active operational behavior until Mode 3 is
  implemented and accepted. Mode 2 remains retired historical lane guidance.
- Set the <=10-second target against oldest included edit through decisive
  runtime evidence, including readiness wait and every build/activation/proof
  phase. Documented limits: batching alone does not eliminate compile/link
  costs or provide arbitrary C++ object-layout migration.
- Added Mode 1/Mode 3 matched trial requirements and append-only per-attempt
  CPU, memory, paging, disk, process-tree, live workload, compiler lifetime,
  activation, and event-journal evidence to
  `docs/specs/dev-loop/build-time-observability.md`.

## Verification

- `git diff --check` passed for the documentation changes.
- Cross-referenced the existing build queue, latency, observability, hot-reload,
  and authority specs; no code, build configuration, running process, or game
  architecture was changed. No build or runtime test was started.
- The implementation remains future work: current Mode 1 still automatically
  responds to saved compiler-input changes.
