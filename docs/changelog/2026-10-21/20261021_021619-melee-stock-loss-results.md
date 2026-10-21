# Melee stock loss announcements and result display

- Time: 2026-10-20 22:16:19 EDT / 2026-10-21T02:16:19Z
- Branch: `20261011zt`
- HEAD before this task: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- State: `PASS_WITH_HUMAN_REVIEW`

## Existing work preserved

The checkout already contained edits in `config/accounts/default.json`, `config/analytics.json`, `config/behavior-profiles.json`, `config/weapons.json`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `src/network/server-melee-actions.cpp`, `src/npc/npc-behavior.cpp`, and `src/npc/npc-state-machine.cpp`, plus four untracked Melee changelogs dated 2026-10-21. These were present before this task's edits and were preserved. The Melee specification already had unrelated recent changes in sections 9, 34, and elsewhere; this task only edited section 52.

## Changes

### `src/network/server-gamemode.cpp`

`updateActorStates` now broadcasts one server chat message on each authoritative Melee stock loss, stating the fighter's name and exact remaining stock count. On the final stock it includes the surviving fighter's name and says they win the round. The existing stock event remains server-authoritative and is still consumed only on the Alive-to-dead transition. The Melee result packet also reuses the existing winner-name fields to send the actual winner name (including NPC winners) for the result HUD.

Before stock handling:

```cpp
// Melee: any fighter (human or NPC) that dies loses one stock.
if (d.matchMode == "melee" && before == ActorState::Alive && dead)
    serverMeleeConsumeStock(kv.first);
```

After:

```cpp
// Melee: any fighter (human or NPC) that dies loses one stock. Publish
// the stock count in chat so the event is visible to everyone, not
// only present in the server journal.
if (d.matchMode == "melee" && before == ActorState::Alive && dead) {
    serverMeleeConsumeStock(kv.first);
    const auto stockIt = d.meleeStocks.find(kv.first);
    const int stocksLeft = stockIt == d.meleeStocks.end()
        ? 0 : std::max(0, stockIt->second);
    const auto nameIt = d.participantNames.find(kv.first);
    const std::string fighterName = nameIt != d.participantNames.end()
        ? nameIt->second : "A fighter";
    std::string message = fighterName + " lost a stock. " +
        std::to_string(stocksLeft) +
        (stocksLeft == 1 ? " stock left." : " stocks left.");

    if (stocksLeft == 0) {
        uint32_t winnerId = 0;
        for (uint32_t participantId : d.participants) {
            if (participantId == kv.first) continue;
            const auto otherIt = d.matchActors.find(participantId);
            if (otherIt != d.matchActors.end() &&
                (otherIt->second.state == ActorState::Alive ||
                 otherIt->second.state == ActorState::Respawning)) {
                winnerId = participantId;
                break;
            }
        }
        const auto winnerNameIt = d.participantNames.find(winnerId);
        if (winnerId != 0 && winnerNameIt != d.participantNames.end())
            message += " " + winnerNameIt->second + " wins the round!";
    }
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
                               message.c_str());
}
```

The winner-name packet addition is at `broadcastDuelState` (lines 1332–1342): it fills the existing result label fields only while Melee is in RESULTS with a winner. This avoids a protocol-layout change.

### `src/network/community-match-client.cpp`

Before:

```cpp
if (mMode != "tdm") hud.updateFFA(leaders);
```

After:

```cpp
if (mMode == "melee") hud.updateFFA({});
else if (mMode != "tdm") hud.updateFFA(leaders);
```

This keeps the FFA leaderboard hidden in Melee while retaining the winner-name packet lookup that feeds the existing winner HUD.

### `docs/specs/gamemodes/supersmashbrosmelee.md`

Before:

```text
At active-match start, each fighter has four lives and 0 percent. The first
playable configuration has one human and one NPC, both using the
`captain_falcon` profile and move set. A void death consumes one life; if lives
remain, the server respawns the fighter immediately at 0 percent and applies
60 server ticks of spawn protection. A fighter with no lives is eliminated.
The match ends when one fighter remains. Protection does not disable movement;
its exact blocked effects follow the open question in section 55. No other
death cause is allowed.
```

After:

```text
At active-match start, each fighter has four stocks (lives) and 0 percent. The first
playable configuration has one human and one NPC, both using the
`captain_falcon` profile and move set. A void death consumes one life; if lives
remain, the server respawns the fighter immediately at 0 percent and applies
60 server ticks of spawn protection. Every stock loss is announced in global
chat with the fighter's name and exact stocks remaining. Losing the last stock
announces the elimination and the surviving fighter as the round winner. The
server ends the match when one fighter remains, shows the winner in chat and
on the results HUD for exactly five seconds, then returns to intermission
before the next match. The Melee mode's `results_seconds` is 5. A fighter with
no lives is eliminated. Protection does not disable movement; its exact blocked
effects follow the open question in section 55. No other death cause is
allowed.
```

The player percent reset was already present: `resetPlayerForSpawn` calls `serverMeleeResetForSpawn`, which sets `player.meleePercent = 0.0f`. NPC Melee action state is erased when its death is processed, so a newly created NPC Melee state starts at zero. The Melee mode already sets `results_seconds: 5`, and the generic `last_actor_alive` path already puts the match in RESULTS and then returns to INTERMISSION when the result timer expires.

## Documents and focused review

Read `docs/ROUTER.md`, Melee and shared gamemode specifications, networking and chat specifications, runtime-scenario validation, build and task-completion operations, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/chat-checker-v1.md`.

Review result: `PASS_WITH_HUMAN_REVIEW`. The server stock transition, reliable server chat path, winner serialization, client winner label lookup, and five-second result configuration agree after these edits. A real stock-loss scenario and on-screen acceptance remain unverified.

## Validation and limits

- `git diff --check`: passed (only existing Windows line-ending warnings).
- Canonical dev-loop build request: success, build 2721, source generation current. Build identity SHA-256: `3518cd8f0e34c1a39558e56eff4d49ca836ed26cff2b50c552264a5e2e980587`.
- Exact build executable: `C:\mimita-v9\.dev\builds\2721\mimita.exe`.
- `--versioninfo` identity run printed `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_221619/events.jsonl` (resolved path: `C:\mimita-v9\logs\10-20-2026\20261020_221619\events.jsonl`). This was an identity-only run, not a gameplay run.
- The existing dev-loop client was still running build 2718 with `auto_restart=false`; the server was no longer alive when checked. I left the client untouched and did not launch a competing match. Therefore no live journal proof that the stock chat, winner HUD, or five-second transition works is claimed.
- No new tests were added or run.

## Human review still needed

Run a real Melee match and verify: a non-final death resets the percent display to 0 and announces the remaining stocks; a final death announces the winner, displays that winner in the HUD for five seconds, and then enters intermission. Confirm that the player and NPC both follow this lifecycle.

