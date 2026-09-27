# Phase 1 — modecreate as a hot package (selection, overlap, clipboard, combat suppression)

- EST timestamp: 2026-09-13 21:27:28 EDT (UTC 2026-09-14T01:27:28Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `SOURCE_COMPLETE / BOOTSTRAP_PENDING` (cold build refused: `mimita.exe` running)

## Context

Phase 0a/0b (generic runtime, dynamic components, capability resolution, event
emit, package-declared domains) and most of 0c were already in the tree. A
parallel live session had introduced `src/hot-reload/hot-package.h`
(`HotPackageBuilder` + self-registering `MimitaHotPackage::*Registrar`), a hot
`movement.main` stub, and live rename/add torture files (`banana-*`). Phase 1 is
built on that.

## What was implemented

### Editor package (hot) — `src/hot-reload/modules/editor-behavior.cpp`
- Registers the `modecreate [0|1]` command through
  `MimitaHotPackage::CommandRegistrar` (no aggregator edit, no EXE slot).
- Creation-mode state lives in the kernel shared-state area
  (`GameSharedStateV1` at the start of `permanentStorage`), so the editor and
  gameplay policy coordinate without a new ABI field.
- **Hovered vs selected**: hovered updates every tick; selection is committed
  only on `CTRL+LMB` (or `select`), persisted in shared state.
- **Overlap cycling**: the full `queryRay` candidate stack is kept; mouse wheel
  cycles candidates; the overlay lists `[i/N]`.
- **Clipboard**: `CTRL+C` snapshots the selected identity; `CTRL+V` duplicates
  through the existing `EditorForkFn` (recorded in `CreationMode`/ChangeSet);
  `CTRL+X`/`DEL` deletes from the fork. Copy policy is read from component
  schema metadata, not hardcoded.
- Overlay shows mode, hovered/selected, candidate stack, status, inspection.
- Still defines `MimitaGetEditorModule()` (fixed-tick/overlay hooks).

### Combat suppression (hot) — `src/hot-reload/modules/rocket-behavior.cpp`
- Reads the shared creation-mode flag from `permanentStorage` and suppresses
  `GAME_EVENT_FIRE_INTENT` (`outFire = 0`) and `GAME_EVENT_DAMAGE_POLICY`
  (`outDamage = 0`) while create mode is active — no kernel change.

### Input + ABI (cold, for the next bootstrap)
- `EditorInputV1`: added `selectPressed` (CTRL+LMB), `cyclePrev`/`cycleNext`
  (mouse wheel), `moveUp`/`moveDown`.
- `live-editor.cpp` polls CTRL+LMB and the shared scroll offset for wheel.
- `GameplayContextV1`: added kernel `permanentStorage` for cross-module shared
  state; `GameSharedStateV1` + `GAME_MODE_FLAG_CREATION` added to `game-api.h`.
- `terminal.cpp`: hot-package commands now take precedence over builtins, so a
  hot `modecreate` overrides the cold one.
- `hot-reload-system.cpp` initializes the shared-state magic at startup.

### Hot-swap safety (important)
Growing `GameplayContextV1`/`EditorInputV1` would over-read when the new DLL
loads into the currently-running (older) EXE. Guards:
- `EditorContextV1` grew (`editorAbiVersion`), so a module loaded into an older
  EXE sees `structSize < sizeof` and degrades to the legacy path; all v2/new
  input reads are gated on `hasV2`/`structSize`.
- `rocket-behavior` only reads `permanentStorage` when
  `context->structSize >= sizeof(GameplayContextV1)`.
- Shared-state access requires the kernel-initialized magic, so an older kernel
  yields no shared state instead of a crash.

## Evidence

- `-fsyntax-only` clean (DLL) for every `src/hot-reload/modules/*.cpp` including
  the live-renamed `banana-system-renamed.cpp`/`banana-events.cpp`, and (EXE)
  `live-editor.cpp`, `live-behavior.cpp`, `terminal.cpp`, `hot-reload-system.cpp`.
- Hot package DLL link success with 9 sources
  (`build_game_dll.py --output build/verify-p1.dll`), confirming the
  `modecreate` command + editor module + suppression link.
- Cold build **not run**: `python build_agent.py` refused (`mimita.exe` running).
  No process killed.

## Pending / left

- Cold bootstrap to install the Phase 1 ABI/input/terminal changes.
- Free-fly/noclip: `movement.main` is still a stub; moving the real movement step
  hot (design A) is the next piece so create mode can free-fly the body.
- Runtime human proof: `modecreate 1`, `CTRL+LMB`, wheel cycling, `CTRL+C/V/X`,
  combat suppression, then live edits of editor source files.

## Files

Changed: `src/hot-reload/modules/editor-behavior.cpp`,
`src/hot-reload/modules/rocket-behavior.cpp`, `src/hot-reload/game-api.h`,
`src/live-code/live-editor.cpp`, `src/live-code/live-behavior.cpp`,
`src/devtools/terminal.cpp`, `src/hot-reload/hot-reload-system.cpp`.
