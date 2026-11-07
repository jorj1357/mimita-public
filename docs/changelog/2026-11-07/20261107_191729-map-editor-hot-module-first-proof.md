# Map editor hot module first proof

- Recorded: 2026-11-07 14:17:29 EST (2026-11-07T19:17:29Z)
- Branch: `20261011zt`
- HEAD at start: `64d28fe1`
- Status: PASS_WITH_HUMAN_REVIEW
- Scope: specification and architecture documentation; no C++ implementation changed

## Pre-existing work

The worktree already contained broad unrelated changes across assets,
configuration, devscripts, documentation, shaders, and source files. The live
module spec and live-development architecture files had existing edits from
earlier work; those changes were preserved and extended only where needed to
align the first module goal. `docs/specs/entity-editor/entity-editor.md` was
clean before this task. No branch, commit, or unrelated runtime file changed.

## Changes

- `docs/specs/hotreload/hotreload.md` now makes one `map-editor` module the
  first module goal and moves NPC behavior to migration step 2. It defines the
  module coverage across `src/mapeditor/*.cpp` and editor policy currently
  embedded at engine/input call sites. Those host call sites become thin
  dispatch adapters; map/project documents, file persistence, window/input,
  rendering, geometry, and collision mechanisms stay host-owned.
- The first proof is a property-panel position edit to a map-authored block,
  followed by autosave, reload of the same map, JSON read-back, and visible
  confirmation. It calls for a post-loader source save inside the editor module
  so the same running EXE activates a new generation before handling the edit.
- Added state-preservation rules for unsaved map contents, stable selection,
  undo/redo, clipboard, editor mode, pending edits, and save state. Activation
  occurs on the main thread between editor callbacks. Failure retains the old
  generation and document.
- Specified bounded, process-attributed event evidence. The current
  `map-editor.transform.apply` event must include position changes;
  `map-editor.reload.result` must be added and correlated with autosave and the
  exact map JSON. This is source-checked: current position application does
  not emit the transform event, while autosave already has an owner event.
- Updated the all-systems migration order, save-to-live timing workload, and
  acceptance gates. NPC behavior remains a later module step.
- `docs/specs/entity-editor/entity-editor.md` links the editor behavior and
  migration target to section 5 of the live-module spec.
- `docs/architecture/live-development/hot-kernel.md` and
  `docs/architecture/live-development/hot-cold-audit.md` now state that map
  editor C++ behavior is currently in the normal EXE source path and name its
  first module goal.

## Source and review evidence

- Read `docs/ROUTER.md`, the entity-editor and world-project specs, the
  live-module and live-kernel docs, runtime/journal guidance, and relevant
  documentation/spec review guidance.
- Traced the current property-edit path through
  `src/input/input-commands.cpp::MapEditor::handleTextKey`,
  `src/mapeditor/map-editor.cpp::applyPropertyPosition`,
  `setSelectedPosition`, `nudgeSelection`, `tick`, `autosave`, and `reload`,
  then `src/gamemode/map-config.cpp::MapConfigRegistry::save/load`.
- Confirmed editor callback sites in
  `src/engine/engine-tick-camera.cpp`, `engine-tick-ui.cpp`,
  `engine-tick-render.cpp`, `engine-tick.cpp`, `src/input/input-commands.cpp`,
  and `src/main-init.cpp`. Current reload source includes only the focused
  effect/debug DLL; it does not reload map editor code.
- Read the active Map Editor v1 spec and documented its position properties,
  autosave, explicit save, reload, and `MapConfigRegistry` ownership. Read the
  regression/source-reconciliation material to keep the proof scoped and
  distinguish present source from historic hot-module claims.
- `git diff --check` for the four changed documentation files: PASS (one
  existing line-ending normalization warning on `entity-editor.md`). Relative
  spec links checked to existing files.
- No build or runtime run was made because this task changes documentation
  only. Map Editor module loading, generation event records, autosave/reload
  journal proof, JSON read-back, and visual review remain future acceptance
  work.
