# Generic runtime bootstrap (Phase 0c): whole-project watcher, incremental build, generation manifest

- EST timestamp: 2026-09-13 21:17:50 EDT (UTC 2026-09-14T01:17:50Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_PENDING` (cold build refused: `mimita.exe` pid 24124 running)

## What was implemented (third staged cold build)

### Per-path watcher — `src/project/project-watcher.h/.cpp`
- `ProjectWatcher` now parses `FILE_NOTIFY_INFORMATION` and queues
  `WatchEvent { WatchAction (Added/Removed/Modified/RenamedOld/RenamedNew),
  relative path }`, exposed via `pollEvents(...)`. `poll(outPaths)` still works.
- Larger 16 KiB buffer; recursive; `mutex`-guarded queue.
- `hot-reload-system.cpp` now watches the whole `root_/src` tree (not just
  `src/hot-reload`), while `build/` output is excluded so builds do not loop.

### Incremental hot build — `build_game_dll.py` + `hot-reload-system.*`
- New `--changed <comma list>` and `--obj <stable dir>` arguments.
- `load_dep_edges` + `affected_stems` parse `.o.d` files and recompile only
  changed sources plus sources whose dependencies include a changed file
  (header edits); unaffected `.o` files are reused. Missing/unknown sources are
  always rebuilt. Without `--changed` the previous full-build behavior is kept.
- Kernel computes a per-file hash diff (`diffSourceHashes`) and passes the
  changed list and a stable per-process obj dir
  (`build/hotreload/p<pid>/obj`) so generations reuse objects.

### Generation manifest — `generic-runtime.*` + `network/*`
- `GenericRuntime::manifestHash()` = deterministic hash of the registered type-id
  set (systems, events, schemas, capabilities, requirements, commands, package).
- `CodeGenerationPacket.moduleSetHash` is now populated from it, and
  `logicalCodeHash = codeHash ^ (moduleSetHash * FNV prime)`, in both the server
  announce (`server.cpp`) and client report (`multiplayer-tick.cpp`). This turns
  the previously-stubbed generation-manifest fields into real values.

## Evidence

- `-fsyntax-only` clean: `project-watcher.cpp`, `hot-reload-system.cpp`,
  `generic-runtime.cpp`, `dynamic-components.cpp`, `server.cpp`,
  `multiplayer-tick.cpp`, `phase456-selftest.cpp`.
- `python -m py_compile build_game_dll.py` OK.
- Incremental build proven into a throwaway output:
  `python build_game_dll.py --output build/verify-inc.dll --obj build/verify-inc-obj
  --changed "src/hot-reload/modules/editor-behavior.cpp"` ->
  `incremental=True` + `DLL build success`.
- Cold build **not run**: `mimita.exe` pid 24124 is running (invariant forbids
  relinking/killing). No process was killed.

## Bootstrap (pending)

`project-watcher.*`, `hot-reload-system.*`, `generic-runtime.*`, and the network
changes are EXE-owned. Close the running `mimita.exe`, then `python build_agent.py`
and run the self-tests. After this, add/rename/delete/split/merge of package
sources under the watched tree compiles only affected TUs.

## Pending

- Runtime human proof of live add/rename/delete of editor sources.
- Phase 1: `modecreate` as a package (hot free-fly movement system, hovered vs
  selected, overlap cycling, copy/cut/paste, ChangeSet).
- Multiplayer READY/switch-at-tick-N enforcement using the new manifest hash.

## Files

Changed: `src/project/project-watcher.h/.cpp`,
`src/hot-reload/hot-reload-system.h/.cpp`, `src/hot-reload/generic-runtime.h/.cpp`,
`build_game_dll.py`, `src/network/server.cpp`, `src/network/multiplayer-tick.cpp`,
`docs/gold/2026-09-13-live-runtime-generic-bootstrap.md`.
