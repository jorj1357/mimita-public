# Incremental game-DLL builds recompiled every source

- Recorded: 2026-11-12
- Status: FIXED (source/build); runtime proof not required for this build-system regression
- Related spec: `docs/specs/hotreload/hotreload.md`
- Related gold reference: `docs/gold/2026-11-12-game-dll-incremental-skip.md`
- Related changelog: `docs/changelog/2026-11-11/20261111_215740-source-manifest-stage0-4-scaffolds.md`

## Symptom

A no-change run of `devscripts/game_build_worker.py` recompiled all 575 `game`
sources and printed `[CXX ]` for every file. The diagnostic full build took about
11 minutes even though only a handful of files had changed (or none).

## Bad pattern

`game_build_worker.py::dep_is_stale` did not match the real GCC `.d` format:

1. It split the `.d` line on the first `:`. The first colon is the Windows drive
   letter (`C:`), so the target object path was treated as a dependency.
2. It replaced `\` with spaces, shredding Windows paths into bogus tokens.
3. It treated `-MP` phony targets (`C:\...\header.h:`) as dependencies, and each
   one failed `stat`, forcing a rebuild.
4. It had no zero-byte `.o`/`.d` check and no explicit PCH check.
5. Its object directory was `build/game/generation-N/obj`, so every new
   generation started with an empty cache anyway.

The result was two independent full-recompile triggers: a parser that always
returned `stale`, and a cache that never survived a generation.

## Correction

- Persistent `build/game/objects/<flags-hash>/` cache shared across generations.
- `.d` parsing that joins `\`+newline, strips the `.o:` target, splits on
  whitespace, skips tokens ending in `:`, and keeps backslashes inside paths.
- Zero-byte `.o`/`.d` treated as stale.
- Shared PCH checked with `>=`.
- Dependency mtimes compared with `>=`, matching `build.py::source_changed`.
- Parallel all-core compile, high priority, and live `[SKIP]` / `[CXX ]`
  progress.

## Evidence

| Run | Compiled | Skipped | Compile | Link |
|---|---:|---:|---:|---:|
| First full | 575 | 0 | ~11 min | failed (diagnostic) |
| First rerun | 10 | 565 | 18.2 s | 12.0 s |
| No-change rerun | **0** | **575** | **0 s** | 15.3 s |

The no-change run prints 575 `[SKIP]` lines and zero `[CXX ]` lines. The link
still fails with the known 389-symbol resident/vendored worklist; that is separate
from the build-system regression.

## Prevention

- Keep the skip rules aligned with `build.py::source_changed`.
- Keep the object cache outside `generation-N/`.
- Before accepting a build-system change, run a no-change build and assert
  `compiled=0` / `skipped=575`.
- Do not parse `.d` files by splitting on the first colon or by replacing
  backslashes with spaces.
