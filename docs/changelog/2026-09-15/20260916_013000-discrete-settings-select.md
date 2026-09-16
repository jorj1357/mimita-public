# Discrete settings SELECT + resolution/graphicsPreset

Date: 2026-09-16 01:30 EST (UTC 2026-09-16T05:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Discrete settings (resolution, graphicsPreset) via a generic SELECT widget and
the generic setting seam. Settings is now hot for all user-facing values.

## DISCRETE SETTINGS
- SELECT primitive: `GAME_UI_SELECT` — shows the current option label; click
  emits `VALUE_CHANGED` with the next index. Backend stores only bounded value
  data + the id for the frame (no cached pointers).
- option representation: `GAME_SETTING_OPTION` with `intValue` (index),
  `optionCount`, `optionLabel`; the kernel owns the valid list.
- resolution: kernel list {1280x960, 1600x900, 1920x1080}; hot shows the label,
  click cycles, `setting.set` applies/validates.
- graphicsPreset: kernel list {Low, Medium, High}; same path.
- validation: kernel clamps the index and returns the applied value; hot cannot
  inject unsupported modes.
- ownership: still the hot settings screen (`screen.settings`).

## SETTINGS COMPLETE?
Yes for user-facing values: fov, master/music/sfx volume, sensitivity, mute,
resolution, graphicsPreset. Internal tuning/developer state not migrated.

## PAUSE MENU
Audited as next (not migrated this pass): cold `PauseMenu::render`
(`engine-tick-ui`), Esc input; logical actions `pause.resume/settings/leave/quit`
planned via `ui.action` + `HotUiPendingAction`; hot would claim `screen.pause`.

## SERVER BROWSER
Not migrated this pass (next big owner after pause).

## OTHER UI RE-AUDIT
Cold: replay browser, avatar creator, help, login, notifications/consent/music
overlays. Largest: server browser/pause.

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet (pause, server browser, remaining menus).

## AUDIO POLICY
Not started.

## NEW ABI / PRIMITIVES
`GAME_UI_SELECT`, `GAME_SETTING_OPTION` + `GameSettingV1.optionCount/optionLabel`.
WHY GENERIC? One discrete-choice widget and one option-type setting express any
enumerated setting/choice; no resolution/preset-specific ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No (a concurrent `hot-reload-system.h` edit broke one cold build; waited/retried).

## COLD-RESTART METRIC
Now NO: main menu, **settings (all user-facing)**, actor overlays, TDM/FFA/CS HUD,
scoreboard, tool presentation. Still YES: pause menu, server browser, remaining
menus, audio policy, resource generations.

## LIVE-PROOF DEBT
Interactive settings/pause/server-browser appearance, audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Pause menu, then server browser, then audio policy.

## Files changed
`src/hot-reload/game-api.h`, `src/live-code/live-behavior.cpp`,
`src/live-code/live-ui.cpp`, `src/hot-reload/modules/ui/settings-screen.cpp`,
`src/hot-reload/modules/ui/ui-actions.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
