# Build cache, progress reporting, and hot subsystem boundaries

Date: 2026-11-09 (UTC)
Lane: `hot-npc-net-20261109`

## Purpose

Reduce repeated cold EXE builds and make build progress observable, while
finishing the hot networking/NPC subsystem boundary work started in this
session.

## Source changes

- `build.py`
  - Writes `build/progress.json` with explicit phases: `setup`, `pch`,
    `compiling`, `linking`, `staging`, `build_complete`, `failed`.
  - Flushes compile/phase lines and reports current translation unit.
  - Records `scheduler_wait_s`, `pch_s`, `compile_s`, `link_s`, and
    `staging_s` in `build-timing.json` / `build-metrics.jsonl`.
  - Uses relative project source/object/include paths so compiler inputs are
    stable across worktrees.
  - Sets `CCACHE_BASEDIR` to the worktree root and `CCACHE_NOHASHDIR=1`.
  - Builds the PCH from a relative path with `-fno-working-directory` and
    `-ffile-prefix-map=<worktree>=.` so the PCH does not embed the lane path.
  - Records ccache direct-hit/miss deltas and cumulative ccache counters.
- `devscripts/dev-loop.py`
  - Reads `build/progress.json`; exposes `active_build_phase` and
    `active_build_progress`.
  - Shows compile/link/staging phase text instead of a stale last `[CXX]` line.
- `devscripts/dev_loop/build.py`
  - Runs the build unbuffered.
  - Shares `CCACHE_DIR`; sets `CCACHE_BASEDIR=<lane root>` and
    `CCACHE_NOHASHDIR=1`.
  - Publishes progress before/after artifact publication.
- Hot subsystem work from this session remains included:
  - `network-policy`: server phase-budget policy event `709`.
  - `npc-perception`: new module, event `509`, capability `6`.
  - Reconnect accept carries `mapId`; client preserves the previous map and
    re-arms map readiness.
  - Docs updated for the hot networking/NPC target.

## Build evidence

- Before this fix, lane cold build:
  - `compiled=624`, `skipped=0`, `elapsed_s=699.798`, `cold_build=true`.
  - `player-animation.cpp` was only the last displayed `[CXX]` line; the build
    continued through link and other work.
- After progress/cache wiring, incremental/no-op build:
  - `compiled=0`, `skipped=624`, `elapsed_s=17.193` (15:29 local artifact).
- Controlled ccache probe:
  - First compile after deleting `npc_npc.o`: `compiled=1`,
    `ccache_misses=1`, `compile_s=25.359`.
  - Repeat compile after deleting the same object: `ccache_hits=1`,
    `ccache_misses=0`, `compile_s=2.063`.
  - This proves the normalized ccache path works in the lane.
- Progress evidence:
  - `build/progress.json` reported `pch`, `compiling`, current file, compiled
    count, and phase seconds during live builds.
  - The status no longer depends only on the last `[CXX]` line.
- Final coordinator no-op validation after the last `build.py` edits:
  - `compiled=0`, `skipped=624`, build time `4.628s`, phase seconds
    `compile_s=2.401`, `scheduler_wait_s=0.008`, `staging_s=0.020`.
  - `python syntax ok`, module ownership audit `PASS`, and `git diff --check`
    produced no new errors.

## Cross-lane validation

The PCH determinism fix is implemented. A full two-lane cold/warm comparison
was not completed because another lane held the machine-wide EXE slot and the
lane watcher restarted daemons during the test. The shared cache still showed
`direct_cache_hit=1` only; further cross-lane validation is required after the
machine has one controlled build slot.

## Runtime evidence

No new gameplay/runtime acceptance is claimed in this pass. The earlier lane
artifact `2026-11-09T20-02-31.475480Z` and mode-56 journals remain separate
evidence; reconnect/map-sync behavior must still be exercised in a fresh run.

## Open follow-up

- Run one controlled lane A cold seed, then a fresh lane B build, and confirm
  `ccache_hits` approaches the full translation-unit count.
- Confirm the PCH hash is identical across two worktrees after the
  normalization change.
- Run mode 56 and inspect `network.map-sync.state`, snapshot/reconnect events,
  and NPC perception generation fields.
