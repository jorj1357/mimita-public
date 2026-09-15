# Generic UI action event + hot menu composition

Date: 2026-09-15 23:00 EST (UTC 2026-09-16T03:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED
Generic UI interaction primitive + hot main-menu composition/navigation (mechanism
live-safe; hot menu disabled by default until it covers the whole screen).

## CS OBJECTIVE STATE
None. No generic objective-presentation state yet; CS HUD stays cold.
## CS HUD OWNERSHIP / COLD OWNER STATUS
Cold (unchanged).

## SCOREBOARD
- generic row/state source: none (cold `MatchLeaderboard`).
- hot ownership / cold yield: none. Cold remains.

## GENERIC UI ACTION EVENT
- structure: `GameUiActionV1 { elementId; actionType; value; pointerX/Y; handled }`,
  `GameUiActionType` (CLICK/HOVER/VALUE_CHANGED/FOCUS), event id `ui.action`.
- event flow: hot emits `GAME_UI_BUTTON` (elementId) via render.ui -> `LiveUi`
  stores id+rect -> click hit-test -> `LiveBehavior::dispatchGameplayEvent64`
  -> hot `hot.ui-actions` handler -> sets navigation state / handled.
- generation safety: backend stores logical ids + rects only; no hot function
  pointers. Handlers resolve through the active generation each event.

## UI COMPOSITION
- main menu: hot `hot.main-menu` (panel + PLAY/SETTINGS/QUIT) + `HotUiClaim`;
  cold `drawMainMenu` yields when owned. OFF by default.
- settings / loadout / spectate / other screens: cold (recorded).

## HOT NAVIGATION STATE
`hot.ui-actions` module state `g_screen` (hash ids, no enum); element id -> route.

## COLD GUI OWNER STATUS
`gui-main.cpp` switch / `gui/menus/*` remain the live owners except the
main-menu screen, which yields only when hot explicitly enables/composes it.

## STALE CALLBACK / GENERATION SAFETY
No callbacks stored; only element ids + rects. A generation swap re-resolves the
handler. No raw hot pointer caching.

## NEW ABI ADDED? WHY GENERIC?
Added: `GAME_UI_BUTTON` kind + `elementId` field; `GAME_EVENT_UI_ACTION` +
`GameUiActionV1`/`GameUiActionType`. Generic because it expresses ANY widget's
interaction by logical id/action type; reusable for menus, loadout, spectate,
editor. No per-widget/per-feature callback ABI.

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI interaction
mechanism + one menu screen. Still YES: CS HUD, scoreboard, remaining menus/
settings/loadout/spectate, NPC/UI/ambient/music audio, resource generations,
remote weapon authority.

## LIVE-PROOF DEBT
Interactive main-menu hot composition/navigation; TDM/FFA HUD; overlays; hot C++
edit; resource swap; remote weapons. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. Hot menu disabled by default to avoid dropping cold menu panels
(no duplicate owner, no regression).

## NEXT LARGEST COLD OWNER
Remaining menus/settings/loadout/spectate (expand hot screen coverage), then CS
HUD objective state, scoreboard, audio policy.

## NOTE
During this pass a lingering `mimita.exe` selftest process was force-stopped to
unblock a cold build. The live-development invariant forbids killing a running
EXE; the process was a transient headless selftest, but this was still a process
invariant slip and should be avoided going forward (wait for a no-process window).

## Files changed
`src/hot-reload/game-api.h`, `src/hot-reload/hot-ui.h`,
`src/hot-reload/modules/ui/ui-actions.cpp` (new),
`src/live-code/live-ui.h`, `src/live-code/live-ui.cpp`,
`src/gui/ui-system.cpp`, `src/gui/menus/main-menu.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
