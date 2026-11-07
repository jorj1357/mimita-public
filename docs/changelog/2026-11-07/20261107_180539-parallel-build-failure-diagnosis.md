# Parallel builds and failed-build diagnosis — 2026-11-07

## Findings

- `.dev/state.json` reports source generation 73, latest successful source
  generation 45, a stale latest artifact from 2026-11-07T17:16:25.830136Z, and
  a failed request at 2026-11-07T17:50:43.610Z after about 7.3 seconds. The
  previous successful artifact was retained; there is no current run.
- The corresponding dev-loop output shows compile attempts for
  `src/mapeditor/map-editor.cpp`, `src/procedural/infinite-world-runtime.cpp`,
  and `src/render/render-player.cpp`, then only a generic `BUILD FAILED`.
  `build.py`'s `compile_cpp_file` returns only `("fail", src)` and the caller
  prints a generic banner; it does not retain the failed process exit code or
  diagnostic text in the build result. Therefore the failing translation unit
  and underlying compiler/tool failure are not proven by this evidence.
- This evidence does not prove simultaneous-build collision. A build lock is
  acquired per checkout; parallel agents need separate worktrees so they do
  not share object files, PCH, executable, or dev-loop state.
- `docs/operations/build-and-exe/build-and-exe.md` recommends separate lanes
  for parallel agents, while the current repository instructions and
  `docs/specs/dev-loop/dev-loop.md` mark mode 2 experimental/do-not-use. Resolve
  this policy conflict before treating parallel lanes as an authorized
  workflow.
- A diagnostic `python build.py` invocation was attempted after the reported
  dev-loop request had failed; it also ended with a generic `BUILD FAILED` and
  no further cause. This was redundant and did not produce live-runtime or
  `events.jsonl` evidence. Future builds should use the dev-loop path.

## Recommended changes

1. Improve build diagnostics so each failed translation unit returns its path,
   exit code, and captured compiler output in the build status/result. Keep
   successful compile output quiet and errors bounded.
2. After policy approval, run each agent in its own worktree/lane with its own
   dev-loop, object/PCH output, executable, and journal. Share only the
   compiler cache. Limit concurrent compiler jobs globally to available CPU
   and memory; begin with one job per lane and tune from measurements.
3. Build and test the merged integration snapshot separately; inspect the
   journal tied to that exact integrated artifact for final behavior evidence.

## Review and validation

- Read the dev-loop, build/EXE, and live-development guidance and inspected
  `devscripts/dev_loop/build.py`, `build.py`, and `.dev/state.json`.
- Read-only dev-loop status confirmed the failed state and stale artifact; no
  game was launched and no runtime journal was produced.
- No source or build-system fix was made in this diagnosis.
