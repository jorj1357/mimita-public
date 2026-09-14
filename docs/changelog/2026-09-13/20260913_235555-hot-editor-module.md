# Hot-reloadable editor module (source complete; one cold bootstrap required)

- EST timestamp: 2026-09-13 19:55:55 EDT (UTC 2026-09-13T23:55:55Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_PENDING` (cold build refused: `mimita.exe` pid 18908 running)

## Goal

Make creation/inspection mode (`modecreate 1`) a dedicated **hot module**: the
selection policy, inspector formatting, and overlay layout are editable live,
without relinking `mimita.exe`; editor state survives hot reload.

## Architecture

Kernel owns ECS/world/spatial-query/UI primitives; the hot module owns policy.

```
fixed tick (kernel)                                  hot module (DLL)
  EditorStateV1 (camera ray, enabled)
  -> LiveEditor::tick(world, state, out)
       ctx.queryRay  (kernel: candidates, sorted) -> rank/filter (HOT selection)
       ctx.inspect   (kernel: POD component snapshot) -> format (HOT)
  <- EditorResultV1 {handled, hitKind, entity, distance, inspection}
  CreationMode::setExternalResult(...)   (kernel cache)
render/UI frame (kernel)
  engineRenderCreationOverlay -> LiveEditor::drawOverlay()
       ctx.drawText / ctx.drawRect / ctx.screenSize    (HOT layout)
  fallback: kernel text overlay if no editor module
```

## What was implemented

### ABI (cold, once) — `src/hot-reload/game-api.h`
- `EDITOR_*` constants; `EditorHitKind`; `EditorCandidateV1`, `EditorQueryV1`,
  `EditorInspectionV1`, `EditorStateV1`, `EditorResultV1`.
- Capabilities: `EditorQueryRayFn`, `EditorInspectFn`, `EditorDrawTextFn`,
  `EditorDrawRectFn`, `EditorScreenSizeFn`, `EditorContextV1`
  (host pointer, tick/generation, capabilities, `permanentStorage`).
- `GameEditorModuleV1 { onTick, onDraw }`.

### Reload-persistent memory (cold, once) — `hot-reload-system.*`
- `permanentStorage_` (64 KiB) allocated in `HotReloadSystem`, exposed as
  `GameMemory::permanentStorage` so hot modules keep state across generations.

### Kernel bridge (cold, once) — `src/live-code/live-editor.h/.cpp` (new)
- Resolves the `"editor"` module by name+size (`LiveModules`).
- Capability impls: `queryRay` (world triangle + entity bounds, nearest N),
  `inspect` (flattened `EntityInspection` -> POD), `drawText`/`drawRect`/
  `screenSize` (wrapping `uiDrawText`/`uiDrawRect`/`uiScreenW/H`).
- `tick(...)`, `drawOverlay()`, last state/result cache.

### Kernel wiring (cold, once)
- `src/sim/simulate-tick.cpp`: per fixed tick, prefer `LiveEditor::tick`; fall
  back to the kernel `CreationMode::updateTick` when absent/declined.
- `src/editor/creation-mode.h/.cpp`: `setExternalResult(...)` applies the hot
  selection to the kernel-owned cache.
- `src/engine/engine-tick-creation.cpp`: `engineRenderCreationOverlay` calls
  `LiveEditor::drawOverlay()` first; kernel text overlay is the fallback.

### Hot module (live, no restart) — `src/hot-reload/modules/editor-behavior.cpp`
- `onTick`: query candidates via `ctx->queryRay`, hot selection policy
  (`preferEntity` + `entityBias`, editable), `ctx->inspect`, fill result.
- `onDraw`: full overlay layout via `ctx->drawText`/`drawRect` (panel, entity,
  domain/legacy/gen/pos, component list, constraint or linked constraint).
- State in `permanentStorage` (`EditorPersistV1`, magic-guarded) — survives
  reloads.
- `game-modules.h` declares `MimitaGetEditorModule()`; `effect-part.cpp`
  publishes it; `hot-modules.json` adds the `editor` module and the two new
  `live-editor.*` cold entries.

## Evidence

- `-fsyntax-only` clean: `live-editor.cpp`, `simulate-tick.cpp`,
  `engine-tick-creation.cpp`, `creation-mode.cpp`, `hot-reload-system.cpp`.
- DLL-side `-DMIMITA_GAME_DLL` clean: `editor-behavior.cpp`, `effect-part.cpp`.
- `hot-modules.json` parses.
- Cold build **not run**: `python build_agent.py` -> `HOT_RELOAD_BOUNDARY_VIOLATION`
  because `mimita.exe` pid 18908 is running. No process was killed.

## Bootstrap (required once)

`game-api.h`, `live-editor.*`, `simulate-tick.cpp`, `creation-mode.*`, and
`engine-tick-creation.cpp` are EXE-owned. One relink installs the ABI + bridge.
After that, all editor edits are hot.

1. Close the running `mimita.exe` (pid 18908) — do not kill it while live.
2. `python build_agent.py` (one cold build).
3. `mimita.exe --creation-selftest`.
4. Runtime: `modecreate 1`, look at world/NPC/rocket/ragdoll limb -> hot overlay.
5. Edit `src/hot-reload/modules/editor-behavior.cpp` (selection policy / layout)
   -> live generation swap, same EXE/world/session.
6. Edit `src/hot-reload/game-api.h` -> boundary violation (expected; ABI is cold).

## Known limitations

- ABI additions are cold; design future capabilities additively within
  `EditorContextV1`/`GameEditorModuleV1` (bump `abiVersion`/`structSize`).
- The kernel fallback path remains until the editor module is present.
- `GameMemory::permanentStorage` is 64 KiB (bump if editor state grows).
- Packet/protocol unchanged.

## Files

New: `src/live-code/live-editor.h`, `src/live-code/live-editor.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`.

Changed: `src/hot-reload/game-api.h`, `src/hot-reload/game-modules.h`,
`src/hot-reload/hot-modules.json`, `src/hot-reload/hot-reload-system.h/.cpp`,
`src/effects/effect-part.cpp`, `src/sim/simulate-tick.cpp`,
`src/editor/creation-mode.h/.cpp`, `src/engine/engine-tick-creation.cpp`.

## Pre-existing edits preserved
All unrelated working-tree changes were preserved.
