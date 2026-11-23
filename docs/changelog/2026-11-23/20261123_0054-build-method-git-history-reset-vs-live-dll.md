# Git history: old reset build method vs current live DLL reload

**Date:** 2026-11-23 UTC
**Context:** `ctx-abca4d34dd82`
**Issue:** `issue-136d4553ff92` (cross-linked `issue-ed3ac775283d`)
**Agent:** `build-wait-reviewer`

## Question

The user asked whether git history shows what the old "reset build method" was
like compared with the current live DLL reload, and whether it is worth
continuing to pursue live DLL reload.

## Git evidence

### Old reset method

- `29edfe2c` (2026-09-26) added `devscripts/dev-loop.py` (410 lines). Its build
  path is `python build.py build-only` -> publish `mimita.exe` to
  `.dev/builds/NNNN` -> `restart_latest()` -> `launch_latest()`, which
  Popen-launches a fresh server and client. It has an `--auto-restart` toggle and
  `stop_processes()` terminates the pair (terminate, then kill after 3 s) before
  relaunch.
- Its commit message: "better hot reload its quick i dont even care its nto
  while exe runs its fine" -- i.e. a fast incremental build with a process reset
  was accepted at the time. The loop still compiled only changed TUs (ccache +
  the ~121 MB PCH) and then linked `mimita.exe`; it never performed same-process
  activation.
- `3a89b2de` (2026-10-18) later updated the loop.

### Current live reload

- Live same-process reload is a separate, older line (`18ec8f80`, `4acaead8`,
  `e95e530a` "hot rle"; `4e82e338` mid-2026).
- It became the primary loop via `devscripts/game_build_worker.py` +
  `--request game-dll`: publish a game-DLL generation and let `ModuleHost` swap
  it into the live server PID. Generation 546 was active in server PID 29536
  with 67 NPCs and 0 hotness violations.

## Comparison

- Shared by both: the changed-TU compile and a link. The old target was
  `mimita.exe`; the current target is the ~483 MB / 642-object game DLL.
- Different: the current loop costs ~0.4-0.5 s to publish plus same-PID
  activation and keeps connected clients and live state. The old loop terminated
  the process, relaunched, reconnected, and lost the live scenario.

## The remembered ~5 s

Stored evidence shows the ~5.9-6.2 s attempts are warm no-op DLL builds
(`compiled=0`, `skipped=642`, `link=0`). The fastest successful changed-source
DLL attempt across 335 summaries is 19.86 s; no stored changed-source attempt is
<=10 s. The remembered ~5 s is therefore a warm no-op or small warm TU with no
live load, not an arbitrary edited file.

## Verdict

Keep pursuing the live DLL reload; do not revert to reset as the default.

- Reverting does not remove either real floor: heavy changed-TU compile
  (22-151 s measured) and the monolithic link (~8 s quiet, ~60 s loaded). It
  only trades live activation for restart, reconnect, and state loss, and
  removes just the ~0.5-1 s publish/activate seam.
- Keep the incremental EXE/reset loop as the documented fallback for edits that
  cannot be hot-applied safely (object layout, globals, ABI).
- The route to <=10 s is not the reset method: it is coarse modules (small DLL
  link) or the deferred hot behavior layer
  (`docs/specs/hotreload/hot-layer-latency-and-language.md`). Live reload is also
  the feature the user actually wants: edit `.cpp` with other clients connected,
  no reset.

## Scope and verification

Read-only git, docs, and `.dev/build-timings` review. No source, build,
compiler, dev-loop, or game process was changed or started. Build and runtime
claims above are quoted from existing stored evidence; this session produced no
new build or runtime proof.

## References

- Commits: `29edfe2c`, `3a89b2de`, `4e82e338`
- `.dev/build-timings/*.json`
- Prior answers to `ctx-c1a0c3306a13` at 2026-11-23T00:12:04Z and 00:12:36Z
- `docs/specs/hotreload/hot-layer-latency-and-language.md`
