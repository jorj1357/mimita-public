# Generic server listings + hot server browser composition

Date: 2026-09-16 02:30 EST (UTC 2026-09-16T06:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Server-browser listing substrate + hot composition. Discovery/sockets/ping/
connect remain cold. Ownership flip pending (cold online menu also ships host and
join-by-code features).

## SERVER BROWSER AUDIT
Owner: `gui/menus/online-menu.cpp` using `MimitaNet::ServerBrowserEntry`
(code, serverName, hostPlayerName, map, gamemode, players, maxPlayers,
passwordProtected, uptimeSeconds, publicIp, port, ping) via
`serverBrowserEntries`, `serverBrowserRefreshing`, `serverBrowserRequestRefresh`.
Cold sort modes: Name Az/Za, Ping low/high, Players most/least, Uptime long/short.
Also ships host/room creation and join-by-code (text input).

## GENERIC LISTING REPRESENTATION
- entity/component projection (chosen over a list ABI): one presentation entity
  per listing with `HotServerListingV1` (`ServerListingState`), enumerated by hot.
- why: reuses Entity/Component/enumeration already used for actors/tools/effects;
  no fake world semantics beyond a presentation entity.
- identity: opaque `listingId = gameHash(code)`; `code` carried for the cold
  connect key. No socket/address/pointer in the component.
- lifetime: re-projected each frame from the thread-safe snapshot; disappeared
  listings have the component cleared.

## DISCOVERY STATUS
Cold and unchanged (`serverBrowserTick` per frame; refresh via
`serverBrowserRequestRefresh`). Hot never touches discovery.

## HOT SERVER BROWSER
- rows: name, players/max, mode, ping.
- sorting: hot policy (ping ascending, unreachable last). Shipping's other sort
  modes are recorded as future hot policy.
- filtering: none shipping in the list path migrated; not invented.
- selection: JOIN per row via the listing id (no separate selected-row state yet).

## REFRESH ACTION
`serverbrowser.refresh` -> pending action -> cold `serverBrowserRequestRefresh()`.

## CONNECT ACTION
JOIN button elementId = listingId -> hot handler resolves the listing component to
its cold room code -> pending `serverbrowser.connect` with `value=code`. The cold
connect glue (mapping code -> connection) is recorded as the remaining bridge.

## RUNTIME-UNKNOWN LISTING
A listing entity with only generic facts appears in the hot browser and the claim
is owned (selftest PASS).

## LISTING REMOVAL SAFETY
Stale components are cleared each frame; hot resolves by id, no cached pointers.

## GENERATION SWAP SAFETY
Listing facts live in dynamic components; the claim is per-frame. A generation
that stops claiming falls back to the cold browser.

## SERVER BROWSER OWNERSHIP
- claim default?: only when hot nav == screen.server-browser (dev/selftest).
- cold fallback?: the cold online menu remains the shipping owner until host/
  join-by-code coverage; it yields when hot owns.

## OTHER UI RE-AUDIT
Cold: replay browser, avatar creator, help, login, pause Settings/Help/
ConfirmLeave, notification/consent/music overlays. Pause subviews left as
compatibility debt.

## PAUSE SUBVIEWS
Settings/Help/ConfirmLeave remain cold (allowed debt).

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet (server browser flip pending text-input/host; pause subviews; replay/help).

## AUDIO POLICY
Not started.

## NEW GENERIC PRIMITIVES
`HotServerListingV1` (`ServerListingState`) + `HotUiPendingActionV1.value[32]`.
WHY GENERIC? Listing facts + a bounded action payload are reusable for any
server/replay/item list and any value-carrying action; no server-browser-specific
ABI.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings, actor overlays, TDM/FFA/CS HUD, scoreboard, pause
Main view, tool presentation, **server-browser list composition**. Still YES:
browser host/join-code + ownership flip, pause subviews, replay/help, audio,
resource generations.

## LIVE-PROOF DEBT
Interactive browser appearance, host/join flow, audio, resource swap.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. Missing generic primitive identified: **text input** (needed to
flip the browser owner for join-by-code/host).

## NEXT LARGEST COLD OWNER
Generic text-input primitive -> finish browser ownership flip; then pause
subviews/help; then audio policy.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/server-browser.cpp` (new),
`src/hot-reload/modules/ui/ui-actions.cpp`, `src/render/presentation-entities.h`,
`src/render/presentation-entities.cpp`, `src/gui/gui-main.cpp`,
`src/gui/menus/online-menu.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
