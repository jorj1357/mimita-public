# Scoreboard show/hide bridge -> hot scoreboard full shipping owner

Date: 2026-09-16 01:00 EST (UTC 2026-09-16T05:00:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Scoreboard visibility policy. Combined with Round 54's real per-actor stats, the
hot scoreboard is now the shipping owner while the Tab scoreboard is held.

## SCOREBOARD SHOW/HIDE
- physical input source: `mpContext.showPlayerList = glfwGetKey(TAB)==PRESS`
  (`engine-tick-net.cpp:821`).
- logical action/state: `HOT_SCOREBOARD_VISIBLE_COMPONENT` /
  `HotScoreboardVisibleV1`, written by cold
  `PresentationEntities::projectScoreboardVisible()`.
- hot visibility policy: `hot.scoreboard` composes only while visible.
- cold MatchLeaderboard/tab-list status: yields via
  `hotOwnsScreen("screen.scoreboard")`; fallback when generic stats are absent.
- hold semantics preserved (not converted to a toggle).

## SCOREBOARD COMPLETE?
Mostly: real per-actor stats ship, hot rows/sorting/grouping, hot show/hide,
local highlight, FFA/TDM/CS share the substrate, cold is fallback. Remaining
minor: alive/spectator flags not generic (deferred; derivable from health/state).

## DISCRETE SELECT
Not added (resolution/graphicsPreset deferred).

## SETTINGS COMPLETE?
No (discrete options pending).

## PAUSE MENU / SERVER BROWSER
Not migrated this pass (recorded as next real owners).

## OTHER UI OWNER AUDIT
Cold: replay browser, avatar creator, help, login, server browser, pause menus,
notification/music/consent overlays. Largest: pause + server browser.

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet.

## AUDIO POLICY
No audio migrated this pass.

## NEW ABI / PRIMITIVES
`HotScoreboardVisibleV1` (generic logical visibility fact). No kernel ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings subset, actor overlays, TDM/FFA/CS HUD, scoreboard
(composition + real stats + show/hide), tool presentation. Still YES: discrete
settings, pause menu, server browser, remaining menus, audio policy, resource
generations.

## LIVE-PROOF DEBT
Live scoreboard rows/show-hide, remaining menus, audio, resource swap. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Discrete settings SELECT; pause menu; server browser; then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/scoreboard.cpp`,
`src/render/presentation-entities.h`, `src/render/presentation-entities.cpp`,
`src/engine/engine-tick-ui-hud.cpp`, `src/engine/engine-tick-ui-overlays.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
