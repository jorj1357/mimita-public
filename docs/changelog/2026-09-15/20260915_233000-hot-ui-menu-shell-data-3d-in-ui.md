# Hot UI in the menu + shell data + generic 3D-in-UI primitive

Date: 2026-09-15 23:30 EST (UTC 2026-09-16T03:30:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live proof deferred by policy)

## SUBSYSTEMS MIGRATED
Hot UI now runs while the menu shell is active; generic menu-shell data bridge;
generic 3D-in-UI clip primitive. Fixed an invisible-button bug.

## MAIN MENU SHELL
- cold elements audited: background image/video, cover/logo image, 3D avatar
  preview, account panel (username/MMR/W/L/K/D), VIP styled name, nav buttons,
  Continue-as/Play, sign-in/sign-up/switch/logout entry points, auth/password
  popups, music/notification/consent overlays.
- hot coverage: background panel, title, username, version, PLAY/SETTINGS/QUIT.
- claim enabled by default? NO. Missing avatar/logo/account-stats/auth entry
  points; enabling would drop required UI (safety invariant).
- cold owner status: `drawMainMenu` remains the single live owner; it yields only
  when hot explicitly enables the screen (`uiscreen`).

## HOT NAVIGATION STATE
- persistence across generation swap: still a hot module static (`g_screen`);
  reset on generation replacement. Migrating it to a migratable component is
  recorded as the next step.
- no raw pointers/callbacks: confirmed (ids + rects only).

## SETTINGS
- hot composition: none this pass. generic value binding: `GameUiActionType`
  has VALUE_CHANGED; no slider/checkbox widget kind yet. cold mechanism: engine
  settings untouched. (recorded)

## LOADOUT
- generic tool enumeration / runtime-unknown tool support / hot equip action:
  none this pass (recorded; the generic tool substrate already exists).

## SPECTATE
- entity-based targeting / hot actions: none this pass (recorded).

## SCOREBOARD
- generic rows / hot sorting / cold status: none; cold `MatchLeaderboard`.

## CS OBJECTIVE STATE
None. Missing generic objective-presentation state (design recorded).
## CS HUD OWNERSHIP / COLD CS HUD STATUS
Not migrated / cold.

## NEW ABI/PRIMITIVES ADDED? WHY GENERIC?
- `HotMenuShellStateV1` (hot shared state; profile/version/connection chrome).
- `GameRenderMeshCommandV1.uiClip[4]`: binds a mesh draw to a UI rect (viewport +
  scissor) -> generic 3D-in-UI. Reusable for avatar/inventory previews, editor
  viewports, spectator thumbnails. No per-screen ABI.
- Fixed `GAME_UI_BUTTON` draw.

## GENERATION SAFETY
UI backend stores logical element ids + rects only; no callbacks. A generation
swap re-resolves handlers. Menu nav state currently resets on swap (recorded).

## BAD-GENERATION LAST-GOOD UI
Not exercised this pass. Existing hot-DLL last-good mechanism unchanged.

## BUILD/FILE-LOCK STATUS
No build lock issue this pass. Generation-unique DLL filenames already used.
## DID ANY PASS REQUIRE KILLING THE RUNNING EXE?
No this pass. (Previous pass did; recorded and not repeated.)

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, UI-interaction
mechanism, hot menu chrome (mechanism). Still YES: main-menu shipping ownership,
settings, loadout, spectate, scoreboard, CS HUD, audio policy, resource
generations.

## LIVE-PROOF DEBT
Interactive hot menu (claim off), settings/loadout/spectate, scoreboard, CS HUD,
audio, resource swap. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. Hot menu claim off to avoid dropping required UI (no duplicate,
no regression). Invisible-button bug fixed (was a functional defect).

## NEXT LARGEST COLD OWNER
Complete hot menu shell (avatar preview via the new uiClip primitive, logo/bg
images, account stats, auth entry points) -> flip the claim; then settings
(+ slider/checkbox widgets), loadout, spectate, scoreboard, CS objective state.

## Files changed
`src/live-code/live-ui.cpp`, `src/gui/gui-main.cpp`,
`src/gui/hud/menu-shell-bridge.h` (new), `src/gui/hud/menu-shell-bridge.cpp` (new),
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/ui-actions.cpp`,
`src/hot-reload/game-api.h`, `src/render/presentation-render.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
