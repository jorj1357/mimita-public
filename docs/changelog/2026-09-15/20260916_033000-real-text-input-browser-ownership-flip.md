# Real text-input wiring + server-browser ownership flip

Date: 2026-09-16 03:30 EST (UTC 2026-09-16T07:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## REAL TEXT INPUT WIRING
- keyboard source: cold GLFW `glfwSetCharCallback` / `glfwSetKeyCallback` in
  `main-init.cpp`, which now check `LiveUi::textInputFocused()` first and route
  codepoints to `handleTextChar`, BBackspace to `handleTextBackspace`, Enter/KP
  Enter to `handleTextSubmit`, returning before any cold text widget.
- focus: set by clicking a `GAME_UI_TEXT_INPUT` (backend stores only the focused
  logical element id).
- codepoints/backspace/submit: generic `ui.action` TEXT_INPUT (codepoint; 0 =
  backspace) / TEXT_SUBMIT.
- F->G persistence: text in `HotUiTextStateV1` (dynamic component).

## LISTED-SERVER CONNECT
- real cold mechanism: pending `serverbrowser.connect` (value = room code) ->
  `consumeHotUiPendingAction` sets `gPendingConnect` and enters GAME_PLAYING,
  reusing the existing room-code connection path.
- stale listing validation: the cold connect path validates the code; hot state
  is advisory. No stale pointer (code string only).

## JOIN-BY-CODE
- real text path: yes (generic text input + TEXT_SUBMIT / JOIN CODE button).
- real connect path: same `gPendingConnect` room-code path.

## HOST AUDIT
No host/creation form exists in the shipping online menu (only a host-name
column; hosting is a separate/duel flow). No host form migrated; not invented.

## HOST HOT FLOW
Not applicable this pass.

## REFRESH
`serverbrowser.refresh` -> `consumeHotUiPendingAction` -> `serverBrowserRequestRefresh()`.

## SERVER BROWSER SHIPPING OWNERSHIP
- claim default?: YES — `menu.play` routes to `screen.server-browser` and the hot
  browser claims whenever that screen is active.
- cold online menu fallback?: yes, it yields when hot owns and renders otherwise.

## BROWSER END-TO-END SHIPPING TEST
Covered headlessly: PLAY routes to the hot browser; runtime listing appears; text
input updates hot state; connect/refresh dispatch into the real cold mechanisms
(gPendingConnect/serverBrowserRequestRefresh). No Internet needed.

## PAUSE SETTINGS / HELP / CONFIRM LEAVE
Remain cold (allowed debt).

## UI COLD-OWNER RE-AUDIT
Cold: pause Settings/Help/ConfirmLeave, global Help, replay browser, avatar
creator, login/auth composition, notification/consent/music overlays.

## UI ARCHITECTURE COMPLETE ENOUGH?
Close; remaining are narrow debt (pause subviews/help) or secure/resource work.

## AUDIO POLICY STATUS
Not started.

## NEW PRIMITIVES
None this pass (reused `GAME_UI_TEXT_INPUT`, `HotUiTextStateV1`, pending actions).

## DID ANY WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard, pause
Main view, **server browser (list/join/join-by-code/refresh/ownership)**, tool
presentation. Still YES: pause subviews/help, replay/avatar/login, audio,
resource generations.

## LIVE-PROOF DEBT
Interactive browser/list/join/host (n/a), audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Pause Settings/Help/ConfirmLeave (mechanical), then audio policy.

## Files changed
`src/main-init.cpp`, `src/gui/gui-main.cpp`,
`src/hot-reload/modules/ui/ui-actions.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
