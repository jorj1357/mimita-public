# Team Deathmatch match phases and roster

Time: 2026-10-24T01:18:50Z (2026-10-23 21:18:50 EDT)
Branch: `20261011zt`
HEAD before work: `2103321b`

## Result

Updated TDM to use five actors per RED/BLUE team, 30 team points, a three-minute
stalemate timeout, a ten-second intermission, a frozen three-second countdown,
release at GO, five-second results, and continuous repetition. Team capacity,
scoring goal, timers, and freeze policy are JSON-owned. The generic team roster
policy now reconciles NPC fillers to declared team capacities; the TDM result
HUD names the winning team or shows a stalemate.

## Pre-existing work

The checkout already contained unrelated staged/unstaged and untracked work in
configuration, documentation, gamemode, networking, map, GUI, and terminal
files. Several edited source files touched here (`gamemode.cpp`, `gamemode.h`,
`server-gamemode.cpp`, `server-gamemode.h`, `server-npcs.cpp`, and
`community-match-client.cpp`) already had unrelated changes. Those changes were
preserved. During this session, an unrelated untracked
`docs/changelog/2026-10-24/20261024_011040-payload-team-spawns.md` appeared in
the shared checkout and was left untouched.

## Files changed and exact before/after

### `config/gamemodes/tdm.json`

Before: `"time_limit_seconds": 300`, `"intermission_seconds": 15`,
`"results_seconds": 8`, with no ordered team capacities or roster policy.

After: `"time_limit_seconds": 180`, `"time_limit_outcome": "stalemate"`,
`"intermission_seconds": 10`, `"results_seconds": 5`, two teams with
`"capacity": 5`, `"npc_spawn": { "policy": "team_roster" }`, and
`"features": { "freeze_during_countdown": true, "freeze_during_go": false }`.
The existing `goal_value: 30`, respawn, heal, spawn, and map values remain
JSON-owned. The obsolete `hunter`/`juggernaut` role counts were removed.

### `src/gamemode/gamemode.h:42-43` and `src/gamemode/gamemode.cpp:387-390`

Before:

```cpp
bool freezeDuringCountdown = false;
```

After:

```cpp
bool freezeDuringCountdown = false;
bool freezeDuringGo = true;
```

The feature parser now reads `features.freeze_during_go` and preserves the
default GO hold for modes that do not opt out.

### `src/network/server-gamemode.h:260`

Before:

```cpp
uint32_t teamRosterNpcId = 0; // single mode-owned opponent for solo team-roster matches
```

After:

```cpp
std::vector<uint32_t> teamRosterNpcIds; // mode-owned fillers for configured team capacities
```

### `src/network/server-gamemode.cpp`

- `serverCommunityStartMatch`, lines 948-980: the existing
  `gamemode.match.configured` event now records score goal, timeout outcome,
  phase timing, NPC roster policy, and ordered team capacities. These values
  identify which JSON configuration the server applied.
- `assignMatchParticipants`, lines 3325-3353: `team_roster` participants keep
  valid human/NPC team assignments and otherwise fill the least-populated team
  within configured capacity.
- `reconcileTeamRosterNpc`, lines 3467-3600: the former one-opponent path now
  removes stale owned NPC IDs, fills each ordered team to its configured
  capacity, trims excess fillers per team, and records expected/actual counts
  in `gamemode.team-roster.resolved`.
- `beginMatchCountdown`, line 3871: applies the JSON countdown freeze policy.
  The TDM GO transition at line 6271 releases the hold when
  `freeze_during_go` is false; the shared GO-to-ACTIVE transition always
  clears it at line 6307.
- The team-roster policy now enters the normal intermission from WAITING and
  returns an empty lobby to WAITING after intermission (lines 6218-6228).
- `checkMatchEnd`, line 4976: `time_limit_outcome: stalemate` now suppresses a
  winner at timeout for any mode that selects it. The score-limit check still
  runs first.
- The TDM kill scorer at lines 5921-5940 records one point for a confirmed
  opposing-team kill and emits `tdm.team-score.updated` with actor/team IDs,
  both team scores, and the JSON goal.

Representative scorer change:

```cpp
// Before
if (victimTeam >= 0 && teamIt->second != victimTeam) {
    if (teamIt->second == 0) ++d.redTeamKills;
    else ++d.blueTeamKills;
}

// After
if (victimTeam >= 0 && teamIt->second != victimTeam) {
    if (teamIt->second == 0) ++d.redTeamKills;
    else ++d.blueTeamKills;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Duel, StructuredLevel::Important,
        "tdm.team-score.updated", d.matchMode,
        "confirmed opposing-team kill scored", tick,
        nlohmann::json{{"killer", killerId}, {"victim", victimId},
                       {"killer_team", teamIt->second},
                       {"victim_team", victimTeam},
                       {"red_score", d.redTeamKills},
                       {"blue_score", d.blueTeamKills},
                       {"goal", d.goalValue}},
        __FILE__, __LINE__, __FUNCTION__);
}
```

### `src/network/server-npcs.cpp:1248-1252` and `src/network/server-gamemode.cpp:5475-5477`

Before:

```cpp
const bool pauseNpcSimulation =
    matchState.phase == DUEL_PHASE_COUNTDOWN ||
    matchState.phase == DUEL_PHASE_GO;
```

After, NPC simulation and team AI pause during COUNTDOWN and pause during GO
only when the active mode's `freeze_during_go` is true. TDM sets it to false,
so actors resume when the server enters GO.

### `src/network/community-match-client.cpp:584-594`

Before: team results supplied no `winnerName`; the client only named an
individual FFA winner or a configured stalemate.

```cpp
if (winnerName.empty() && packet.matchOver &&
    packet.phase == DUEL_PHASE_RESULTS && packet.winnerPlayerId == 0 &&
    packet.victoryType == 1 && modeConfig.timeLimitOutcome == "stalemate")
    winnerName = "STALEMATE";
hud.setWinner(winnerName,
    packet.matchOver != 0 && packet.phase == DUEL_PHASE_RESULTS,
    packet.winnerPlayerId != 0);
```

After: a TDM result resolves `winnerTeam` through the JSON team names and
passes `<TEAM> WINS` to the existing winner HUD. A timeout stalemate continues
to use `STALEMATE`.

```cpp
bool hasWinner = packet.winnerPlayerId != 0;
if (mMode == "tdm" && packet.matchOver &&
    packet.phase == DUEL_PHASE_RESULTS && packet.winnerTeam >= 0 &&
    packet.winnerTeam < (int)modeConfig.teamNames.size()) {
    winnerName = modeConfig.teamNames[(size_t)packet.winnerTeam] + " WINS";
    hasWinner = true;
}
hud.setWinner(winnerName,
    packet.matchOver != 0 && packet.phase == DUEL_PHASE_RESULTS,
    hasWinner);
```

### Specifications and routing

- `docs/specs/gamemodes/teamdeathmatch.md:1-36` replaces the informal
  placeholder with the requested first-version contract and evidence gates.
  Before, the complete file was:

  ```text
  2026 10 06 1701 jorj todo explain

  its just like team daethmatch
  ur team need to kill the other team more times than they kill u
  then u win
  infinite, ez fun , not a big deal, go go go ,

  NO BEING DEAD = DEAD FRO WHOLE ROUND
  ITS FUN AND FAST PACED ARCADE ok dude
  ```

  After: the new `## Match contract` and `## Runtime ownership` sections at
  lines 5-36 state the explicit 5v5, score, timer, freeze, phase, JSON, and
  evidence requirements.
- `docs/specs/gamemodes/gamemodes.md:49-56` and `:267-279` describe the
  configurable GO freeze policy and update the TDM example from five minutes,
  15-second intermission, and eight-second results to the requested values.
- `docs/specs/networking/networking.md:22-26` keeps the default NPC pause
  through GO while documenting the per-mode JSON opt-out used by TDM.
- `docs/ROUTER.md:80` routes TDM behavior changes to the dedicated spec, shared
  gamemode spec, and networking spec.

## Review and remaining documentation notes

- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The user
  request specified the new TDM values. The older informal TDM note did not
  specify roster fill or GO movement; this change uses the existing generic
  `team_roster` mechanism and exposes GO pause policy in JSON.
- `docs/skills/logging-checker-v1.md`: PASS for source observability. Match
  configuration, roster expected/actual counts, and confirmed team scoring
  have owner-level StructuredLogger events. Runtime execution is still needed
  to observe those events in a real TDM match.
- `docs/skills/documentation-checker-v1.md`: PASS for changed links and
  routing. The router still names `docs/doc-review-09-03-2026.md`, which is
  absent in this checkout.
- Existing TODO at `docs/specs/gamemodes/gamemodes.md:3` remains untouched:
  `todo this not good but move tothis version more  more general things  for gamemodes`.
  Suggested replacement: state that the document defines shared, data-driven
  gamemode ownership and routes mode-specific behavior to focused specs.

## Validation evidence

- `config/gamemodes/tdm.json` parsed successfully as JSON.
- Targeted `git diff --check` passed with no whitespace errors in changed
  lines; Git printed only line-ending conversion notices.
- Canonical build: `python devscripts/dev-loop.py --profile npc-navigation
  --request build --wait-for-build` returned success, build 2960. Dev Loop
  status reported `published_current` and `latest_build_stale: false`.
- Identity probe: `C:\mimita-v9\.dev\builds\2960\mimita.exe --versioninfo`
  identified build 2960 and wrote
  `logs/10-23-2026/20261023_211514/events.jsonl`. The journal contains
  `run.started`, `versioninfo.executed`, and `logger.stopped` only.
- No gameplay test or TDM scenario was run. The current Dev Loop session is
  launch mode 15 (`payload`); it was left running. A fresh TDM match is still
  required to verify ten actors, score updates, GO release, the score goal,
  timeout stalemate, results duration, and repetition. Human visual review of
  the scoreboard and result text also remains open.
- No deployment or production files were changed.
