# Live C++ dependency import

## Changes

- Added upstream Blink, `cr.h`, and Flecs repositories as pinned Git submodules
  under `external/`.
- Recorded upstream URLs, checked-out commit SHAs, license files, build
  requirements, compatibility limits, and an isolated prototype recommendation
  in `docs/specs/hotreload/live-cpp-dependencies.md`.
- Preserved upstream license notices in each submodule.

## Evidence

- The root checkout uses MinGW `g++` with an incremental object cache and
  `ccache`; the current focused DLL experiment is implemented by
  `src/hot-reload/hot-reload-system.cpp` and exposes the API in
  `src/hot-reload/game-api.h`.
- Blink's checked-out README requires MSVC 2017/C++17 and PDB debug symbols,
  specifically recommending `/ZI`; this does not match the current MinGW path.
- The checked-out `cr.h` documents Windows DLL reload, external compilation,
  load/unload callbacks, static-state transfer, and crash rollback.
- Flecs' checked-out build guide documents its C99 core, C++17 API, and Windows
  `dbghelp` / `ws2_32` linkage for non-MSVC builds.
- No game build or game process was started for this dependency import.

## Scope and proof status

Dependency availability, pinned source revisions, and source documentation are
recorded. No dependency was integrated into the game, and no compile, activation,
runtime, latency, or human acceptance claim is made. Existing concurrent
workspace changes were preserved. The active compiler, build path, hot-reload
implementation, dev-loop, and game architecture were not intentionally changed.

## Limitations

Git submodule registration required removing a zero-byte `.git/index.lock` whose
timestamp was from the prior day; no active Git process was present at removal.
The root worktree already contained extensive unrelated modifications before
this task, so repository-wide clean-status claims are not applicable.
