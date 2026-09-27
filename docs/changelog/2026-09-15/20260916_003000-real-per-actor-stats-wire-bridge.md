# Real per-actor match stats wire bridge -> hot scoreboard default owner

Date: 2026-09-16 00:30 EST (UTC 2026-09-16T04:30:00Z) [worktree, uncommitted]
Result: `PASS_WITH_HUMAN_REVIEW` (headless; live network appearance is proof debt)

## SUBSYSTEMS MIGRATED
Client/network data bridge: authoritative per-actor match stats now reach the
client as generic actor state, making the hot scoreboard the default owner.

## AUTHORITATIVE ACTOR STATS AUDIT
Server authority: `ServerGamemodeState::ffaKills/ffaDeaths` (per actor) +
`matchTeams` + `matchActors`; `ServerPlayer.kills/deaths`. Wire previously: only
`ffaLeaderIds/Scores/Names[3]` + `redTeamKills/blueTeamKills`. Client destination:
`CommunityMatchClient` match packet parse. Missing wire field: per-participant
kills/deaths/score/name (now added).

## GENERIC STATS SHIPPING WRITER
- source: server mode counters.
- EntityId mapping: participant actor id -> `PresentationEntities::actorEntityFor`
  (same entity as overlays/team/identity).
- replication/projection: match packet -> client `ReplicatedActorIdentity` ->
  cold projection onto generic components (compatibility bridge; long-term
  authority remains the hot/server mode).
- fields: score, kills, deaths (component carries score/rank/flags; identity/team
  joined by EntityId).

## FFA SCOREBOARD SHIPPING PATH
Real participants now carry generic stats; hot scoreboard sorts by score desc,
highlights local, cold MatchLeaderboard yields.

## TDM SCOREBOARD SHIPPING PATH
Same per-actor stats + `ActorTeamState` grouping; team totals remain in
MatchHudState.

## CS SCOREBOARD PATH IF APPLICABLE
Reuses the same `ActorMatchStatsState` substrate; CS HUD objective already hot.

## SCOREBOARD VISIBILITY
Still gated by presence of generic stats (no generic "scoreboard.show" action
yet); cold Tab input not surfaced. Recorded.

## SCOREBOARD CLAIM
- default? Yes whenever real stats produce rows (shipping path).
- cold fallback? Yes when stats are absent (old server/temporary).
- category complete? Not fully (show/hide action + alive/spectator flags).

## DISCRETE SELECT
Not added (resolution/graphicsPreset deferred).

## UI COLD-OWNER RE-AUDIT
Remaining real cold owners: scoreboard show/hide action, discrete settings,
replay browser, avatar creator, help, login/server-browser/pause menus,
notification/music/consent overlays. Largest: server browser / pause (menus).

## UI ARCHITECTURE COMPLETE ENOUGH?
Not yet.

## AUDIO AUDIT
Hot: explosion/weapon-fire/footstep/air-jump. Cold: UI/button click, NPC
vocal/action, ambient, music selection/state, interactions. First batch planned:
UI-button + music/ambient selection via `audio.play`.

## NEW ABI / PRIMITIVES
Match packet `participantKills/Deaths/Scores/Names[32]` (extension of the
existing participant array — generic per-actor facts, not scoreboard-specific).
No kernel capability ABI added.

## DID ANY HOT WORK REQUIRE KILLING mimita.exe?
No.

## COLD-RESTART METRIC
Now NO: main menu, settings subset, actor overlays, TDM/FFA/CS HUD, scoreboard
composition with real stats, tool presentation. Still YES: scoreboard show/hide
+ alive/spectator, discrete settings, remaining menus, audio policy, resource
generations.

## LIVE-PROOF DEBT
Live scoreboard rows, remaining menus, audio, resource swap. Recorded.

## BLOCKING ARCHITECTURAL ISSUES
None A/B/C/D/E. Note: this changed the match packet wire layout (client+server in
one build); worth a protocol review if other clients are in the wild.

## NEXT LARGEST COLD OWNER
Discrete settings SELECT; scoreboard show/hide action; then the larger real menus
(server browser / pause); then audio policy.

## Files changed
`src/network/packets.h`, `src/network/server-gamemode.cpp`,
`src/network/community-match-client.h`, `src/network/community-match-client.cpp`,
`src/render/presentation-entities.h`, `src/render/presentation-entities.cpp`,
`src/engine/engine-tick-ui-hud.cpp`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.
