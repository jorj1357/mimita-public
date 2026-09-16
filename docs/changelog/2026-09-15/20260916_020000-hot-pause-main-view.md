# Hot pause Main view (per-view ownership)

Date: 2026-09-16 02:00 EST (UTC 2026-09-16T06:00:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Pause menu Main-view composition + action meaning. Cold keeps the modal/focus
mechanism and the Settings/Help/ConfirmLeave views (per-view ownership).

## PAUSE AUDIT
Owner: `gui/menus/pause-menu.cpp` (namespace `PauseMenu`), views Main/
ConfirmLeave/Settings/Help. Actions: resume (close), settings (cold settings
view), help (cold help view), discord (ShellExecute), invite (clipboard+
notification), leave -> confirm -> leaveRoom (duel/queue/net shutdown). Esc
toggles via `PauseMenu::toggle`/`handleKey`.

## PAUSE VISIBILITY / ESC PATH
Generic `HOT_PAUSE_STATE_COMPONENT` / `HotPauseStateV1` (viewHash + visible)
bridged from the cold modal in `engine-tick-ui.cpp`. Physical Esc stays cold;
hot policy composes the view.

## PAUSE ACTIONS
- resume: hot button -> pending `pause.resume` -> `PauseMenu::requestAction`
  -> `close`.
- settings: hot button -> pending `pause.settings` -> cold view Settings (reuses
  the existing cold settings view; hot settings is separate).
- leave: hot button -> `pause.leave` -> cold ConfirmLeave view (cold).
- quit: not present in shipping pause menu (not invented).
- confirmations: cold ConfirmLeave preserved.
- discord/invite: hot buttons -> pending -> cold ShellExecute/clipboard.

## PAUSE OWNERSHIP
- claim default?: Yes for the Main view when the modal is open.
- cold owner status: cold `PauseMenu::render` yields only for the Main view;
  other views render cold. Exactly one owner per view.

## PAUSE GENERATION SAFETY
Visibility/view is a dynamic component; a generation that stops claiming leaves
no claim, so cold renders (no trapped invisible pause).

## SERVER BROWSER
Not migrated this pass (next).

## OTHER UI RE-AUDIT
Cold: server browser, replay browser, avatar creator, help view, login, overlays.

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet (server browser remains).

## AUDIO POLICY
Not started.

## NEW ABI / PRIMITIVES
`HotPauseStateV1` (`PauseMenuState`) + `PauseMenu::viewHash/requestAction`
(cold bridge). WHY GENERIC? Modal visibility + logical view id + logical action
ids; reusable pattern, no pause-specific rendering ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard,
**pause Main view**, tool presentation. Still YES: server browser, remaining
menus, audio policy, resource generations.

## LIVE-PROOF DEBT
Interactive pause/server-browser appearance, audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Server browser; then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/pause-menu.cpp` (new),
`src/hot-reload/modules/ui/ui-actions.cpp`, `src/gui/menus/pause-menu.h`,
`src/gui/menus/pause-menu.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
