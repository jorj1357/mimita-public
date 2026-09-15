# Hot main menu is the default shipping owner

Date: 2026-09-15 23:45 EST (UTC 2026-09-16T03:45:00Z) [worktree, uncommitted]
Branch: `8292026stash`
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Main-menu shell composition + navigation + account/auth entry actions. The hot
main menu is now ON by default; cold `drawMainMenu` is the fallback.

## MAIN MENU SHELL
- background/logo: hot emits `GAME_UI_IMAGE` with logical texture resources
  registered via `resource.register` (`ui.menu.background`, `ui.menu.logo`).
- avatar preview: approximate hot 3D preview via `render.mesh` + `uiClip` +
  view space (framing is cosmetic debt).
- account/profile stats: `HotMenuShellStateV1` extended with MMR/W/L/K/D/tier,
  cold-projected from `AuthSystem`.
- VIP style: tier -> colour policy lives in hot code.
- auth entry points: SIGN IN / SIGN UP / SWITCH ACCOUNT / LOG OUT / PLAY buttons
  with logical ids; routed through the generic pending-action bridge.
- modal coverage: cold auth/password/notification overlays are drawn globally
  (outside `drawMainMenu`) and remain cold; they do not block ownership.
- claim enabled by default? YES.
- cold owner status: `drawMainMenu` yields; compatibility fallback only.

## NAVIGATION GENERATION SAFETY
Nav state is `HotUiNavigationStateV1` (migratable component); claim is cleared
each frame and recomputed by the active generation.

## SETTINGS / LOADOUT / SPECTATE / SCOREBOARD
Not migrated. `menu.settings`/`menu.play` route to cold screens via the pending
bridge. Cold owners unchanged.

## CS OBJECTIVE STATE / CS HUD OWNERSHIP / COLD CS OWNER
None / not migrated / cold.

## NEW PRIMITIVES
- `HotUiPendingActionV1` (`HotUiPendingAction`) — generic cold-bridge action id.
- `HotMenuShellStateV1` profile stats/tier fields.
WHY GENERIC? The pending action is a logical id any hot UI can route to any cold
secure/screen mechanism; not menu-specific.

## BAD-GENERATION LAST-GOOD UI
Per-frame claim clear means a rejected candidate keeps last-good (which re-claims)
and a loaded generation that does not claim falls back to cold (no blank UI).

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI-interaction
mechanism, hot nav state, **main-menu composition + navigation + account/auth
entry actions**. Still YES: settings, loadout, spectate, scoreboard, CS HUD,
audio policy, resource generations.

## LIVE-PROOF DEBT
Interactive main-menu appearance/behaviour, settings/loadout/spectate/scoreboard/
CS, audio, resource swap, avatar framing. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Settings (needs generic slider/toggle widgets + a generic setting read/write
seam), then loadout, spectate, scoreboard, CS objective state; then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/ui-actions.cpp`,
`src/gui/hud/menu-shell-bridge.cpp`, `src/gui/gui-main.cpp`,
`src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/architecture/live-development/hot-cold-audit.md`.
