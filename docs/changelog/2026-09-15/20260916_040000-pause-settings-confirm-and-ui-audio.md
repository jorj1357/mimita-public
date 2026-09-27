# Pause Settings/ConfirmLeave + first UI-audio batch

Date: 2026-09-16 04:00 EST (UTC 2026-09-16T08:00:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## PAUSE SETTINGS
- routed to hot settings? Yes: `pause.settings` -> `HotUiNavigationStateV1`
  (screen = screen.settings, previousScreenId = screen.pause). No duplicate.
- return navigation: `menu.back` returns to screen.pause when previous is pause,
  else main. Generic previousScreenId reused (no new state).

## PAUSE HELP
Not migrated (cold help view retained; classified as debt/mechanical later).

## CONFIRM LEAVE
Hot composition: pause view `pause.confirm-leave` -> "Leave this game?" +
CONFIRM/CANCEL; actions `pause.leave.confirm`/`pause.leave.cancel` -> cold
mechanism (leaveRoom / view Main). Cold confirm view preserved as fallback.

## GLOBAL HELP
Not migrated; would share the same static content composition (recorded).

## GENERATION SAFETY
Navigation and pause state are dynamic components; a generation that stops
claiming a view leaves no claim and cold renders (no trapped modal).

## UI COLD-OWNER RE-AUDIT
Remaining cold: global Help, replay browser, avatar creator, login/auth
composition, notification/consent/music overlays, pause Help view.

## REPLAY BROWSER CLASSIFICATION
A. mechanical — can reuse the listing-entity/action pattern; cold keeps file
scan/metadata/load. Migrate if budget; else defer.

## AVATAR CLASSIFICATION
C. resource/editor dependent — defer until live resources (ContentArtifactV1).

## LOGIN/AUTH CLASSIFICATION
B. secure mechanism — password/token/transport stay cold; composition migratable
later but not required for UI completion.

## NOTIFICATION/CONSENT CLASSIFICATION
B/D — platform/legal/system mechanism; hot presentation optional; does not block
UI completion.

## UI ARCHITECTURE COMPLETE ENOUGH?
Close: main menu, settings, pause core, server browser, HUDs, scoreboard,
overlays, tool presentation are hot. Remaining is narrow/secure/resource debt.

## AUDIO AUDIT
Cold owners that CHOOSE sounds: UI button/click (now hot-mapped), NPC
vocal/action, ambient, music selection/state, interaction sounds. Cold mechanism:
decode/mix/device.

## UI AUDIO POLICY
- first hot mappings: `menu.back`/cancel -> `audio.ui.back`;
  `menu.play`/connect/join-code/resume -> `audio.ui.confirm`; default
  `audio.ui.click`.
- cold mechanism status: unchanged (`audio.play` resolves/plays).

## NPC AUDIO STATUS
Not migrated (next audio batch).

## MUSIC/AMBIENT STATUS
Not migrated.

## NEW PRIMITIVES
None (reused navigation, pending actions, audio.play).

## DID ANY WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard, pause
Main + Settings-route + ConfirmLeave, server browser, UI sound selection, tool
presentation. Still YES: pause Help/global Help, replay/avatar/login, NPC/music/
ambient audio, resource generations.

## LIVE-PROOF DEBT
Interactive pause/settings/confirm, audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
NPC/music/ambient audio policy; then pause/global Help + replay if budget.

## Files changed
`src/hot-reload/modules/ui/ui-actions.cpp`,
`src/hot-reload/modules/ui/pause-menu.cpp`, `src/gui/menus/pause-menu.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
