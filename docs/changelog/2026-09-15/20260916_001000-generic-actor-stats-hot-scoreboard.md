# Generic actor match-stats + hot scoreboard

Date: 2026-09-16 00:10 EST (UTC 2026-09-16T04:10:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live appearance is proof debt)

## SUBSYSTEMS MIGRATED
Scoreboard composition policy hot via a generic per-actor match-stats component.
(The scoreboard's live data projection is recorded as a follow-up.)

## SCOREBOARD AUDIT
Fields the cold `MatchLeaderboard` shows: FFA top-3 (name, score, rank, local
flag, VIP style) and TDM team kills. Identity/team already generic
(`ActorIdentityState`/`ActorTeamState`). Per-actor kills/deaths/score for ALL
players do NOT exist client-side: the client only receives
`ffaLeaderIds/Names/Scores` and `redTeamKills/blueTeamKills`. A full scoreboard
needs a generic stats projection/replication (networking).

## GENERIC ACTOR MATCH-STATS STATE
- fields: score, rank, flags (bit0 local).
- source: none shipping yet (client typed data is top-3/TDM only).
- projection/replication path: none this pass; recorded as the remaining bridge.

## RUNTIME-UNKNOWN ACTOR PROOF
An entity with only `ActorIdentityState` + `ActorTeamState` +
`ActorMatchStatsState` appears in the hot scoreboard (selftest PASS). No Player*
registration, no cold branch.

## HOT SCOREBOARD
- sorting: score descending (hot policy).
- grouping: team colouring via `ActorTeamState`.
- local highlight: stats flags bit0.
- alive/dead/spectator: not yet generic; deferred.
- visibility/input: no generic "scoreboard shown" action yet; the hot scoreboard
  composes whenever generic stats exist (claim only then).

## SCOREBOARD OWNERSHIP
- claim default? Only when the hot scoreboard composed rows; otherwise cold owns.
- cold MatchLeaderboard status: yields via `hotOwnsScreen("screen.scoreboard")`
  when hot owns; live owner until stats are projected (no regression).

## SCOREBOARD GENERATION SAFETY
Rows are resolved by EntityId each frame (no cached pointers); a destroyed actor
is skipped safely; nav/claim are migratable/per-frame.

## DISCRETE SETTINGS SELECT
Deferred (resolution/graphicsPreset). No widget added this pass.

## UI COLD-OWNER RE-AUDIT
Real remaining cold UI owners: scoreboard live stats projection, discrete settings
(resolution/preset), replay browser, avatar creator, help, login/server-browser/
pause menus, notifications/music/consent overlays. Main menu, settings (subset),
overlays, TDM/FFA/CS HUD are hot.

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet (scoreboard live data + remaining menus). Recorded.

## AUDIO AUDIT
Hot: explosion, weapon-fire, footstep, air-jump (via `audio.play`). Cold owners:
NPC vocal/action audio, UI/button sounds (`uiButton` click), ambient, music
selection/state, interaction sounds. Next batch: UI-button + NPC + music/ambient
selection through `audio.play` (hot logical ids; cold device/mixer/decoder).

## NEW ABI / PRIMITIVES
`HotActorMatchStatsV1` (`ActorMatchStatsState`) — actor match facts, joinable by
EntityId; not a UI row type. No kernel ABI added.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: weapon presentation, actor overlays, TDM/FFA HUD, main-menu, settings
(subset), CS HUD objective, **scoreboard composition policy** (given generic
stats). Still YES: scoreboard stats projection, discrete settings, audio policy,
resource generations, remaining menus.

## LIVE-PROOF DEBT
Interactive scoreboard/CS/settings/menu appearance, audio, resource swap.
Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E.

## NEXT LARGEST COLD OWNER
Discrete settings SELECT, then audio policy (UI-button + NPC + music/ambient
selection via `audio.play`). Scoreboard live stats projection remains a recorded
networking follow-up.

## Files changed
`src/hot-reload/hot-ui.h`, `src/hot-reload/modules/ui/scoreboard.cpp` (new),
`src/engine/engine-tick-ui-hud.cpp`, `src/network/hot-combat-selftest.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
