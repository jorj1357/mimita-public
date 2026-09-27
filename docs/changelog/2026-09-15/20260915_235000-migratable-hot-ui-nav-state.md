# Migratable hot UI navigation state + generation-safe claim

Date: 2026-09-15 23:50 EST (UTC 2026-09-16T03:50:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED
Hot UI navigation state is now migratable session state, and the hot screen
ownership claim is recomputed per frame (generation-safe). This fixes the
explicit primary blocker to real hot menu ownership.

## NAVIGATION STATE
- where stored: dynamic component `HotUiNavigationStateV1` (`HOT_UI_NAV_COMPONENT`)
  on the local actor entity — not a module static.
- migration behavior: state survives a hot generation swap; a generation that
  removes/renames a screen yields no claim and the cold owner recovers.
- survives generation swap? Yes (component persists; the static reset cause is
  removed).

## MAIN MENU SHELL
- background / logo / avatar preview / account stats / VIP style / auth entry
  points / modals: still cold. Hot covers only chrome (bg panel, title,
  username, version) + PLAY/SETTINGS/QUIT.
- claim enabled by default? No.
- cold owner status: `drawMainMenu` remains the single live owner except when
  hot explicitly enables the screen.

## SETTINGS
- widgets added / generic read/write seam / hot ownership: none this pass.
- cold mechanism: engine settings untouched.

## LOADOUT / SPECTATE / SCOREBOARD
- not migrated. cold owners unchanged.

## CS OBJECTIVE STATE / CS HUD / COLD CS OWNER
- none / not migrated / cold.

## GENERATION-SAFE UI STATE
`LiveUi::beginFrame()` clears `HotUiClaim`; the active generation re-asserts it
during the UI domain. No generation leaves a permanent cold-yield with nothing
hot rendering.

## BAD-GENERATION LAST-GOOD
Not exercised this pass; existing hot-DLL last-good mechanism unchanged. The
per-frame claim clear means a failed candidate that is rejected keeps the
last-good generation (which re-claims), and a loaded generation that does not
claim falls back to cold deterministically.

## NEW ABI/PRIMITIVES
`HotUiNavigationStateV1` (hot shared migratable state). Generic: holds only hash
screen/modal/focus ids, no cold enum, reusable by any hot screen.
WHY GENERIC? Any multi-screen hot UI needs durable navigation state; this is not
menu-specific.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI-interaction
mechanism, migratable hot nav state. Still YES: main-menu shipping ownership,
settings, loadout, spectate, scoreboard, CS HUD, audio policy, resource
generations.

## LIVE-PROOF DEBT
Interactive hot menu (claim off), settings/loadout/spectate/scoreboard/CS,
audio, resource swap. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Complete the hot menu shell (avatar preview via `uiClip`, logo/background
images, account stats in `MenuShellState`, VIP style, auth entry points, modal
structure) then flip the claim; then settings (＋ slider/checkbox widgets and a
generic setting read/write seam), loadout, spectate, scoreboard, CS objective
state.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/ui-actions.cpp`,
`src/live-code/live-ui.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/architecture/live-development/hot-cold-audit.md`.
