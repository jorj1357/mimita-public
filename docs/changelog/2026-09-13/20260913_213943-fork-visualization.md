# Fork visualization (generic wire box + fork enumeration)

- EST timestamp: 2026-09-13 21:39:43 EDT (UTC 2026-09-14T01:39:43Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_BLOCKED` (cold build refused: `mimita.exe --server` pid 26248 running; a parallel torture test also holds an intentional syntax error in `banana-system-renamed.cpp`)

## Goal

Make recorded fork edits visible: duplicate/transform/delete show up in-world,
generically and hot, without a per-op renderer.

## What was implemented

### Generic primitives (ABI, cold)
- `EditorDrawWireBoxFn` (`drawWireBox`): generic 3D wire box at an arbitrary
  center/size (`game-api.h`).
- `EditorForkOpCountFn` / `EditorForkOpFn` (`forkOpCount`/`forkOp`): enumerate
  the recorded fork ops for visualization.
- `EditorContextV1` grew (append-only), so a module loading into the older
  running EXE sees `structSize < sizeof` and degrades safely.

### Kernel bridge (cold) — `src/live-code/live-editor.cpp`
- `editorDrawWireBoxCap` draws 12 edges via the existing weapon-line debug
  primitive (reused), then flushes.
- `editorForkOpCountCap`/`editorForkOpCap` project `CreationMode` `PatchOp`s into
  `EditorForkArgsV1`.
- `editorForkCap(DUPLICATE)` now carries source bounds through to the patch.

### Editor data (cold) — `src/editor/creation-mode.*`
- `WorldObjectRef.size` and `PatchOp.size` added; `duplicate` records the source
  bounds so the visual matches the real object.

### Hot visualization — `src/hot-reload/modules/editor-behavior.cpp`
- `onDraw` iterates `forkOpCount`/`forkOp` and draws a colored wire box per edit:
  duplicate = green, transform = yellow, delete = red, other = grey (default
  1m box when bounds are unknown). Clipboard paste carries `size` too.

## Evidence

- `-fsyntax-only` clean for `live-editor.cpp`, `creation-mode.cpp`, and the DLL
  `editor-behavior.cpp`.
- Hot DLL link could **not** be re-verified this pass: a parallel live test
  intentionally left a syntax error in `src/hot-reload/modules/banana-system-renamed.cpp`
  (its rollback proof), so a full package link fails until that test removes it.
- Cold build refused (server running). No process killed.

## Pending

- Cold bootstrap to install the ABI/bridge (needed for the wire boxes to appear).
- Base-geometry hiding/mesh override: a delete/transform currently shows a marker
  box over the original; truly hiding/replacing rendered geometry needs a
  renderer per-object override (next).
- Runtime human proof of visible edits.

## Files

Changed: `src/hot-reload/game-api.h`, `src/live-code/live-editor.cpp`,
`src/editor/creation-mode.h/.cpp`,
`src/hot-reload/modules/editor-behavior.cpp`.
