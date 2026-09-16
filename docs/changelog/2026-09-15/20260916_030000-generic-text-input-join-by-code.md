# Generic text input + join-by-code

Date: 2026-09-16 03:00 EST (UTC 2026-09-16T07:00:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## GENERIC TEXT INPUT
- widget: `GAME_UI_TEXT_INPUT` (text = value, maxValue = max length, flags bit0 =
  masked). Backend draws the field + value and tracks only the focused element id.
- action events: `GAME_UI_ACTION_TEXT_INPUT` (value = codepoint; 0 = backspace)
  and `GAME_UI_ACTION_TEXT_SUBMIT`, plus FOCUS on click.
- storage: hot-owned migratable `HOT_UI_TEXT_COMPONENT` / `HotUiTextStateV1`
  (elementId + text[32]) on the local actor entity; not in the backend.
- max-length safety: bounded by HOT_UI_TEXT_MAX; input ignores overflow.
- hot-reload persistence: text lives in a dynamic component, so it survives a
  generation swap (typed state is not in code).

## SERVER BROWSER
- join-by-code: hot field + JOIN CODE button; submit -> pending
  `serverbrowser.join-code` with bounded value.
- host: not migrated (recorded; cold host flow remains).
- listed-server join: JOIN per row -> pending `serverbrowser.connect` (code).
- refresh: `serverbrowser.refresh` -> cold refresh.
- ownership flip: NOT flipped; cold online menu remains the shipping owner until
  the real cold char/key wiring, connect glue, and host flow are complete.
- cold fallback: cold yields when hot owns `screen.server-browser`.

## GENERATION SAFETY
- typing across F->G: text is in a dynamic component (survives).
- list update while typing: rows and text state are independent.
- control removal: focus id is logical; no stale pointer (focus clears safely).

## PAUSE SETTINGS / HELP / CONFIRM LEAVE
Remain cold (allowed debt; not migrated this pass).

## HELP SCREEN
Not migrated (would be mechanical; opportunistic later).

## REPLAY BROWSER RE-AUDIT
Not migrated; could reuse the listing-facts pattern once server browser flips.

## LOGIN CLASSIFICATION
Secure mechanism (password/token/transport) stays cold; composition could be hot.

## AVATAR CLASSIFICATION
Defer until live resources (ContentArtifactV1) are ready; do not add temporary
resource APIs.

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet (browser flip, pause subviews/help).

## NEW PRIMITIVES
`GAME_UI_TEXT_INPUT` + TEXT_INPUT/TEXT_SUBMIT actions; `HotUiTextStateV1`.
WHY GENERIC? Any bounded text field and any keystroke/submit expressed by logical
id + codepoint; no field-specific ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard, pause
Main view, server-browser list composition + join-by-code UI, tool presentation.
Still YES: browser cold keyboard wiring/connect glue/host/flip, pause subviews,
help, login composition, avatar, audio, resource generations.

## LIVE-PROOF DEBT
Interactive text input/browser/host, audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. Remaining work is mechanical + cold-to-hot wiring, not new ABI.

## NEXT LARGEST COLD OWNER
Wire real text keystrokes + connect glue + host to flip the browser; then pause
subviews/help; then audio policy.

## Files changed
`src/hot-reload/game-api.h`, `src/hot-reload/hot-ui.h`,
`src/hot-reload/modules/ui/ui-actions.cpp`,
`src/hot-reload/modules/ui/server-browser.cpp`, `src/live-code/live-ui.h`,
`src/live-code/live-ui.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
