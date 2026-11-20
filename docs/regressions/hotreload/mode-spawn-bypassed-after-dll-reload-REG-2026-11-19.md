// 2026-11-19
/* purpose
* record why newly joining actors bypassed active gamemode spawn rules
* provide a repeatable prevention and live-acceptance procedure
* this file DOES NOT claim the source correction has passed runtime review
*/

# REG: New joins bypassed active mode spawns after DLL reload

- Recorded: 2026-11-19 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — build, runtime, and human validation pending
- Audience: gameplay, networking, and hot-reload contributors
- Related specification: `docs/specs/hotreload/hotreload.md`
- Source-fix changelog: `docs/changelog/2026-11-19/20261119_101520-mode-spawns-after-dll-reload.md`
- Regression-record changelog: `docs/changelog/2026-11-19/20261119_102415-mode-spawn-regression-record.md`

## Reported behavior

The user reported that after a DLL reload, actors in the active mode appeared
to spawn at the same spot and the mode's spawn rules were not being honored.
The exact map, mode, actor type, and report time were not captured. The
reported visual result has not yet been reproduced in a live run for this
record.

## Expected behavior

New joins, respawns, and mode-created actors use the active mode's spawn policy
after a successful DLL generation swap. That policy includes applicable team,
role, authored spawn-group, and explicit spawn-override rules. A generic world
spawn is only a startup fallback while active-mode spawn state is not ready;
that fallback must be logged.

## Why it was broken

Source tracing found that both first-time network join paths,
`dispatchHello` and `dispatchJoin` in
`src/game/server-command-dispatch-module.cpp`, placed a new player by calling
`pickHostSpawn`. That enters the resident `pickServerSpawn` policy, which
selects from the generic map spawn list using the player ID. It does not
consult the active `ServerGamemodeState`, its team/role assignments, or the
mode's spawn-group rules.

The mode-aware resolver already existed in
`src/network/server-gamemode.cpp` as `gamemodeSpawnPointForActor`, and the
active mode state survives generation swaps in resident state. The join path
simply bypassed that owner. Therefore a newly joining actor after a reload
could receive a generic map position even while the match continued in a mode
with its own spawn policy. The reload exposed the gap during live iteration;
the source path itself was not conditional on reload.

The actor team lookup also omitted the existing `communityTeams` mapping. Even
when using the mode resolver, that omission could leave a community actor's
team unknown and select the neutral mode fallback instead of its team spawn.

This source-level cause matches the report, but the exact reason the observed
actors overlapped (for example, map spawn count or team state at join time) is
unconfirmed without the reported mode's journal and map data.

## Incorrect path

```cpp
if (pickHostSpawn(playerId, &spawn) == mimita::host::HostResult::Ok &&
    spawn.found != 0) {
    // place new player at generic resident world spawn
}
```

Both `dispatchHello` and `dispatchJoin` had this behavior for new players.

## Correction saved

- Added `serverGamemodeSpawnPointForNewActor` in
  `src/network/server-gamemode.cpp`. It selects with the active mode resolver
  after `spawnsAssigned` and applies the server's explicit spawn override.
- Updated both join paths to use that resolver first. They use `pickHostSpawn`
  only before mode spawn setup is ready.
- Added `communityTeams` to the authoritative team lookup used by actor spawn
  resolution.
- Added `gamemode.spawn-new-actor` with actor, mode/map, team, reason, and
  resulting position. If an active mode is not ready, the owner emits
  `gamemode.spawn-mode-not-ready` with the explicit generic fallback reason.
- Added the cross-generation spawn contract to
  `docs/specs/hotreload/hotreload.md`.

This correction is source-reviewed only. No build or post-reload runtime proof
is recorded yet, so the regression remains open pending acceptance.

## How to prevent recurrence

1. Keep mode spawn selection owned by `serverGamemodeSpawnPointForActor` and
   route every new actor entry path through it once mode spawn state is ready.
   Do not add another direct generic picker call for a gameplay join path.
2. Keep team resolution connected to the active mode's authoritative
   assignments, including both `matchTeams` and `communityTeams`.
3. When a generic picker is needed during startup, emit the bounded
   `gamemode.spawn-mode-not-ready` event. Treat that event after mode setup as
   a failure to investigate, not as successful mode spawn selection.
4. For each affected mode, perform a real same-PID DLL activation, then exercise
   a new player join and a respawn. Inspect that server instance's
   `events.jsonl`: require activation followed by mode-correct spawn evidence
   and the expected resulting positions; require no not-ready fallback after
   mode setup. Verify distinct/team-correct placement where the mode's authored
   rules require it.
5. Keep build, runtime journal, and human visual acceptance as separate
   evidence. A successful DLL build or activation alone does not close this
   regression.

## Related records

- `docs/regressions/networking/payload-npc-replication-REG-2026-10-25.md`
  records the similar visible symptom of teams sharing a spawn because Payload
  defaulted to `spawn_strategy=shared`. That was a different configuration
  cause; this record covers new join handlers bypassing the active mode owner.

