# Hot C++ pause menu only during gameplay

Date: 2026-09-16

## Request

During gameplay, running and equipping weapons must show no standalone main-menu
overlay. Pressing Esc should open the pause menu, whose normal presentation is
owned by hot-reloadable C++ code rather than JSON.

## Change

- Disabled the standalone `hot.main-menu` compositor by default in
  `src/hot-reload/modules/ui/ui-actions.cpp`.
- Kept explicit `uiscreen screen.main-menu` enablement available for isolated
  hot-UI tests.
- Updated the hot UI self-test to enable that screen explicitly before testing
  its composition.
- The Esc pause path remains owned by `src/hot-reload/modules/ui/pause-menu.cpp`
  for its normal pause view; the cold modal mechanism still controls when Esc
  opens and closes it.

## Evidence

- Source evidence: the normal runtime default is now `g_menuEnabled = false`;
  the pause compositor remains a separate registered hot UI system.
- Build evidence: `python build_game_dll.py` was started; completion output was
  not available before this record was written.
- Human/runtime acceptance still required: run the current game, confirm the
  world has no menu while moving/equipping weapons, press Esc, and confirm the
  C++ hot pause menu appears and edits activate without restarting.
