# Counter-Strike Checkpoint 2 — 5v5 roster and round/match lifecycle

Date: 2026-10-02
EST timestamp: 2026-10-02 22:00:00 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and rule-math runtime evidence are proven. Live gameplay and
visual acceptance (server session, HUD appearance) are NOT performed and remain
required.

## Scope

Checkpoint 2 of `docs/specs/20261002plan.md`: 5v5 roster creation and the
round/match lifecycle (Stage 2 body, Stage 3). Bomb objectives are deferred to
Checkpoints 6-7; rounds currently resolve by elimination or timeout.

## Pre-existing edits (not mine)

Same as Checkpoint 1: `config/accounts/default.json`, `config/analytics.json`,
`config/camconfig.json`, `shaders/post.frag`, `src/config/camera-config.*`,
`src/engine/engine-tick-setup.cpp`, `src/render/post-fx.*`, plus untracked
camera-lens changelogs and `docs/specs/20261002plan.md`. Untouched.

## Files changed

### `src/network/server-gamemode.h`

`ServerGamemodeState` gained round-lifecycle fields: `objectiveRounds`,
`victoryCondition`, `roundNumber`, `roundVersion`, `roundWins[2]`,
`roundsToWin`, `maxRounds`, `roundSeconds`, `roundEndTick`, `roundWinnerTeam`,
`roundEndReason`, `freezeSeconds`, `rosterLocked`, `roundNextNpcId`.

Added declaration `bool serverCounterStrikeRoundSelfTest(std::string& report);`.

### `src/network/server-gamemode.cpp`

- Added round helpers (in the anonymous namespace before
  `assignMatchParticipants`): `roundTeamName`, `roundTeamRoleId`,
  `roundTeamCapacity`, `roundRosterNpcCounts`, `chooseFallbackTeam`,
  `applyHumanRosterTeam`, `buildObjectiveRoster`, `beginObjectiveRound`,
  `endObjectiveRound`, `checkObjectiveRoundEnd`. Forward declarations for
  `resetMatchScores` / `resetGamemodeActorsAtMapSpawn`.
- `serverCommunityStartMatch`: loads `victoryCondition`, `rounds.*` into the
  state, enables `objectiveRounds` when victory is `rounds`, and overrides
  countdown/intermission/results from the rounds block when set.
- `serverStartMode` / community start reset: resets round fields and roster id.
- `broadcastDuelState`: writes `roundVersion`, `roundNumber`, `roundWins`,
  `winnerTeam`, `roundEndReason`, `roundSeconds`, `roundTimerLeft`.
- `serverGamemodeTick`: new `if (d.objectiveRounds)` branch implementing
  INTERMISSION/WAITING -> COUNTDOWN -> GO -> ACTIVE -> RESULTS -> next round or
  intermission, with a human-connected guard and stale-round version bumping.
- Added `serverCounterStrikeRoundSelfTest`.

### `src/network/packets.h`

`DuelStatePacket` gained `roundVersion`, `roundNumber`, `roundWins[2]`,
`winnerTeam`, `roundEndReason`, `roundSeconds`, `roundTimerLeft`.

### `src/network/community-match-client.h` / `.cpp`

Added round accessors (`roundVersion`, `roundNumber`, `roundWins`, `winnerTeam`,
`roundEndReason`, `roundSeconds`, `roundTimerLeft`, `teamName`) and members;
`onState` mirrors the fields; `reset()` clears them; `teamName` resolves from the
gamemode JSON ordered teams.

### `src/engine/engine-tick-ui-overlays.cpp`

Score line uses JSON team names and round wins for named-team modes; added an
active round timer and "X win the round" / "X win the match" result text from
the mode's ordered teams.

### `config/gui/gamemode-meta-gui.json`

Added a `counterstrike` layout section with `scoreText`, `matchTime`,
`roundText`, `roundOverText`, and `countdownText`.

### `src/game/game-cli.cpp`

Added `--cs-round-selftest` (loads `config/gamemodes` and runs
`serverCounterStrikeRoundSelfTest`).

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 2 to the attempt log.

## Reasoning

Round modes are a distinct lifecycle, so they route before the generic
FFA/TDM/elimination branch rather than being folded into it. Roster NPCs reuse
the existing `ServerNpc` mirror + `adoptNewServerNpcs` path, so no CS-only actor
system is created. Roster sizing is a single shared helper used by both the
runtime and the test to prevent drift. `roundVersion` is bumped on every round
start/end so stale packets cannot revive a previous round.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stage 2 body, Stage 3; Checkpoint 2).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.
- Skill: `docs/skills/terminal-command-checker-v1.md` — no new commands.

## Validation

Build (`python build.py build-only`):

```text
 BUILD SUCCESS
Compiled: 1
Skipped : 507
```

Runtime rule-math (`mimita.exe --cs-round-selftest`):

```text
team0=Counter-Terrorists team1=Terrorists
humanOnCT npcs=4+5
after8 CT=8 T=0 winner=0
PASS
```

Regression checks (still PASS): `--gamemode-selftest`, `--actor-preset-selftest`.

## Human review still needed

- Run `start counterstrike`, `team_list`, `team_pick 1`, then play a full round
  cycle: confirm INTERMISSION, 3-2-1-GO, ACTIVE, round result with JSON team
  names, score increment, and match victory at 8 rounds returning to
  intermission.
- Confirm one human + 4 allied NPCs vs 5 enemy NPCs spawn and fight.
- Confirm HUD round timer and results text render correctly.

## Explicitly not done yet

Bomb objectives, plant/defuse/explosion, grenades, NPC perception/utility/
TeamBrain, and first-life roster NPC loadout application (weapon overrides).
