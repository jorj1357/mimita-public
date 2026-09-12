# Project layer and behavior bindings: live project versions, history, and events

- EST timestamp: 2026-09-12 14:23:13 EDT (UTC 2026-09-12T18:23:13Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (primitives pass; one bootstrap cold build pending)

## Task

Implement phases 0-3 of the general live-runtime plan: project-tree/content
primitives, authoring surfaces (socket for agents, terminal for humans), glob
hot sources so add/delete/rename is live, project history (undo/redo/checkpoint),
behavior bindings, and a kernel event queue. Then explain what phases 4-6 need.

## What was added

### Phase 0 — project primitives (`src/project/`)
- `project-types.*`: `ContentId { algorithm, digest }`, `ChangeOp`, `ChangeEntry`,
  `ChangeSet`, `ProjectVersion`, `StateSchema`.
- `blob-store.*`: immutable `.mimita/store/<algorithm>/<digest>` store; deletion
  of a path never deletes a blob.
- `project-tree.*`: scan/filter, whole-tree hash, diff with add/delete/modify/
  rename/move detection (rename = same dir, move = different dir, inferred from
  identical content), manifest storage, and checkout.
- `project-history.*`: record/undo/redo/checkpoint/restore/diff, persisted to
  `.mimita/history.jsonl`; blob manifests so any version is recoverable.
- `dependency-graph.*`: `.d`-file parser and `affected(changed)` traversal.
- `state-schema.*`: schema registry + package-supplied migration dispatch.
- `project-control.*` + `project-control-server.*`: one command surface shared by
  the `project` terminal command (humans) and a loopback TCP line server
  (agents); `project serve [port]` / `project stop`.

### Phase 1 — glob hot sources
- `src/hot-reload/hot-modules.json` gained `"globs": ["src/hot-reload/modules/*.cpp"]`.
- `build_game_dll.py` expands globs; `HotReloadSystem::loadManifest` expands
  globs and re-resolves them each poll, so live add/delete/rename of hot files
  changes the package source set and triggers a rebuild. `hot-modules.json` is
  no longer limited to exact file lists.

### Phase 2 — history ↔ runtime generation
- `ProjectHistory` operates by tree hash and can check out any version. Undo of
  the project changes the tree on disk; the loader then rebuilds a runtime
  generation (the existing rollback command remains for code-only rollback).
- Code-only rollback and project undo are distinct, as required.

### Phase 3 — behaviors and events
- `src/ecs/components.h`: `BehaviorBinding` + `BehaviorBindingsComponent`
  (behavior referenced by id/hash, shared by many entities, max 8 per entity).
- `src/hot-reload/game-api.h`: `GameEmitEventFn` capability typedef.
- `src/live-code/live-behavior.*`: bounded kernel event queue and
  `emitEvent` capability; `dispatchDamagePolicy` now drains nested events FIFO
  after a dispatch; `dispatchEvent` and `enqueueEvent` added.

### Tests
- `--project-selftest` (also run standalone) covers blob dedupe, tree scan,
  add/modify/delete/rename diff, history record/undo/redo/checkpoint/restore,
  `.d` dependency affected set, and schema migration.
- `--hot-authoritative-selftest` fixed to compare the kernel result to the
  behavior's own output instead of assuming magnitudes (removed the `>500`
  pin; the developer intentionally tunes the value).

## Evidence

- `python build_game_dll.py` -> success (4 hot sources; globs expanded).
- `-fsyntax-only` clean for all new `src/project/*` files,
  `src/live-code/live-behavior.cpp`, `src/terminal/project-commands.cpp`,
  `src/main-systems.cpp`, `src/game/game-cli.cpp`.
- Standalone primitives harness -> `[PROJECT SELFTEST] PASS` (18/18 checks).
- `--live-code-selftest` PASS.
- Full EXE cold build **not** performed: two `mimita.exe` processes running;
  `build_agent.py` refused with `HOT_RELOAD_BOUNDARY_VIOLATION`.

## Why the cold build is needed

The project primitives, control server, glob refresh, behavior bindings, and
event queue are EXE-owned mechanisms. A running process cannot gain new call
sites, struct fields, or a new lifecycle. The next phase moves behavior and data
behind these primitives so future changes are content/behavior, not new C++.

## Files

New: `src/project/project-types.h/.cpp`, `blob-store.h/.cpp`,
`project-tree.h/.cpp`, `project-history.h/.cpp`, `dependency-graph.h/.cpp`,
`state-schema.h/.cpp`, `project-control.h/.cpp`,
`project-control-server.h/.cpp`, `project-selftest.h/.cpp`,
`src/terminal/project-commands.h/.cpp`,
`docs/architecture/live-development/project-layer.md`.

Changed: `src/ecs/components.h`, `src/hot-reload/game-api.h`,
`src/hot-reload/hot-modules.json`, `src/hot-reload/hot-reload-system.cpp`,
`src/live-code/live-behavior.h/.cpp`, `src/live-code/live-authoritative-selftest.cpp`,
`src/main-systems.cpp`, `src/game/game-cli.cpp`,
`docs/architecture/live-development/hot-kernel.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Pre-existing edits preserved

The developer's current `rocket-behavior.cpp` proof value (`outDamage = 123` for
explosions) and all other unrelated working-tree changes were preserved.
