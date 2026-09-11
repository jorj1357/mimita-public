// 2026-09-10T22:50:51Z
/* purpose
* record the authoritative death/respawn/elimination slice built on ActorState
* make gamemode respawn_seconds, kill_heals, and last_team_standing authoritative
* preserve exact source, config, build, unit, and headless runtime evidence
* this file does NOT claim spectator camera, role movement, or advanced NPC AI
* this file does NOT replace the append-only regression record
*/

# Authoritative death, respawn, kill-heal, and elimination

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-10T22:50:51Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 18:50:51 EDT`
- Pre-existing changes: the actor/role/team identity foundation from the prior
  pass was preserved (actor-match.h, match-roles.*, roles.json, actor-commands.*,
  packet fields, assignment path). This session only built the death/respawn/
  elimination behavior on top of it.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and one
  `docs/changelog/` file were present and left untouched.

## Changed files

- `src/gamemode/gamemode.h`: added `Gamemode::winCondition`.
- `src/gamemode/gamemode.cpp`: parses `win_condition`; load log includes it.
- `src/network/actor-match.h`: added pure `nextActorState(...)` and
  `matchKillHeals(...)` rules (single source of truth, unit-testable).
- `src/network/server-gamemode.h`: added `ServerGamemodeState::respawnSeconds`
  (`-1` unset, `0` one-life, `>0` delay), `killHeals`, `winCondition`; declared
  `serverMatchRespawnsEnabled()` and `serverMatchRespawnSeconds()`.
- `src/network/server-gamemode.cpp`:
  - rule helpers; copy rules in `serverStartMode` and `serverCommunityStartMatch`;
  - `[MATCH RULES]` diagnostic;
  - `updateActorStates` is now the single ActorState transition owner (uses
    `nextActorState`) and logs `[ACTOR STATE]` transitions;
  - `checkMatchWinConditions` implements `last_team_standing`;
  - state machine runs for any `last_team_standing` mode;
  - team fallback no longer uses defaulted `team_names`;
  - kill block preserves the assigned respawn timer and pins NPC respawn
    anchors instead of resetting all actors;
  - `serverGamemodeRecordKill` gates heal on `matchKillHeals` and logs
    `[KILL HEAL]`.
- `src/network/server-damage.cpp`: lethal damage sets respawn timer from the
  gamemode rule (or `-1` for one-life) instead of hardcoded `0.01`.
- `src/network/server-players.cpp`: `simulatePlayer` never respawns one-life
  actors and ignores instant-respawn requests in that mode.
- `src/network/server-npcs.cpp`: NPC lethal paths take the configured respawn
  delay; the NPC respawn loop is skipped when respawns are disabled.
- `src/network/server-packet-chat.cpp`, `src/network/server-packets.cpp`:
  void-death and explode paths use the configured respawn rule.
- `config/gamemodes/elimination.json` (new): one-life, 2 teams, no kill heal,
  `win_condition: last_team_standing`, roles, fast start.
- `config/onlinemodes.json`: added the `elimination` community mode entry.
- `tests/match-rules-test.cpp` (new): unit tests for the pure rules.

## Old vs new lifecycle behavior

Old (all modes, server):
- Lethal damage hardcoded `respawnSeconds = 0.01f`.
- The kill-processing block re-zeroed the timer and, for NPC victims, called
  `resetGamemodeActorsAtMapSpawn`, instantly reviving every actor.
- `ActorState` was derived from health only (`Alive`/`Dead`), never
  `Respawning`/`Spectating`.
- Player killers were healed unconditionally (`health = serverMaxHp()`).
- No way to end a match by elimination.

New:
- Lethal damage assigns the gamemode respawn timer; `0` means one-life.
- Respawn pumps never revive one-life actors; they stay `dead` and advance to
  `Spectating`.
- `updateActorStates` owns the explicit transition graph and broadcasts it.
- Kill healing is gated on `kill_heals`.
- `win_condition: "last_team_standing"` ends the match when one team (or one
  FFA actor) remains in play, using the existing RESULTS/intermission flow.

## ActorState transition rules (single owner: `updateActorStates`)

Inputs per participant: `dead` (players: `dead`; NPCs: `health <= 0`),
`respawnsEnabled` (`serverMatchRespawnsEnabled()`), and the previous state.

```
!dead                         -> Alive
dead + respawns enabled:
    Alive -> Dead -> Respawning -> Alive (after the pump revives)
    Respawning holds until revived
dead + no respawns (one-life):
    Alive -> Dead -> Spectating (terminal for the round)
    Spectating stays Spectating
```

Mapping of legacy fields (documented in `server-gamemode.h`):
- `Player::dead`, `ServerNpc::health` feed the `dead` input.
- `ServerPlayer::respawnSeconds` and `Npc::body.respawnTimer` drive the pump;
  `ActorTeam`/`matchActors` holds the canonical `ActorState`.
- `ActorState::Spectating` implies `dead == true`, which is what enforces the
  existing no-fire (`server-attack.cpp`, `server-packet-handlers.cpp`),
  no-move (`simulatePlayer`), and no-damage (`applyPlayerDamageLegacy`)
  restrictions without new branches.

## Respawn config consumption

- `Gamemode::respawnSeconds` is copied to `ServerGamemodeState::respawnSeconds`
  in `serverCommunityStartMatch`; duel keeps the default `-1` (legacy 0.01s).
- `serverMatchRespawnsEnabled()`: `enabled=false` => true (legacy); `0` => false
  (one-life); any other value => true.
- `serverMatchRespawnSeconds()`: `-1`/disabled => `0.01f`, else the config value.
- Every lethal path (hitscan/projectile, void death, `explode`, bomb explosion,
  NPC-on-NPC) now writes the configured value. The kill-processing block no
  longer overwrites it.

## kill_heals consumption

`serverGamemodeRecordKill` calls `matchKillHeals(rules.enabled, rules.killHeals)`;
when false the killer's health is left unchanged. It logs
`[KILL HEAL] killer=... kill_heals=N healed=N`. `kill_heals` is copied from the
gamemode JSON in `serverCommunityStartMatch`.

## Elimination implementation

`checkMatchWinConditions` (owned by the shared runtime) handles
`win_condition == "last_team_standing"`:
- a team is in play while it has a participant whose state is `Alive` or
  `Respawning`;
- if at least two teams exist and at most one is in play, that team wins:
  phase -> RESULTS, `winnerTeam` set, `phaseTimer = resultsSeconds`,
  `emitGamemodeMatchPersistence`, `[ELIMINATION]` log;
- with no teams (FFA), the last `Alive`/`Respawning` actor wins.
The mode state machine condition was widened from `ffa || tdm` to also include
`winCondition == "last_team_standing"`, so new modes reuse the same lifecycle.

## Tests and results

Build:
- `python build_agent.py` => `Status: SUCCESS` (return code 0), repeatedly.

Unit test `tests/match-rules-test.cpp` (compiled with the repo MinGW g++,
`-std=c++17 -I src`): `PASS: actor match rules (12 cases)`. Covers all six
`nextActorState` transitions in both respawn and one-life modes and the three
`matchKillHeals` cases.

Test A - normal respawn (TDM, temporarily `respawn_seconds: 2.0` to observe the
delay): server log shows
`[MATCH RULES] mode=tdm respawn=2.00s respawns=1 kill_heals=1`,
then repeated `alive -> dead`, `dead -> respawning`, `respawning -> alive`
for role/team-assigned actors. TDM probe was reverted to `0.01` afterward.

Test B - no respawn (elimination, `respawn_seconds: 0`): server log shows
`alive -> dead` then `dead -> spectating`, and zero
`spectating -> alive` transitions. Spectating actors keep `dead == true`, so
fire/move/damage gates already in place reject them.

Test C - match completion (elimination, 8 NPCs, 2 teams, one life): server log
shows `[MATCH ACTORS] assigned=8 mode=elimination roleCounts=2`,
`[FFA/TDM] Match ACTIVE mode=elimination`, then
`[PERSISTENCE] Match result emitted`, and
`[ELIMINATION] last_team_standing winnerTeam=0 winnerActor=1000 aliveActors=1`.

Test D - kill heal: the pure policy is unit-tested for `true` and `false`; the
server logs `kill_heals=1` for TDM and `kill_heals=0` for elimination, and the
heal is gated at the single call site. A live player-killer heal was not
observable headless (all headless kills are NPC kills) and needs a client.

Regressions verified:
- FFA still starts (`Match ACTIVE mode=ffa`) and scores (`FFA_SCORED`), teamless.
- TDM still starts and respawns.
- NPC combat still records kills and respawns NPCs in respawning modes.
- Role/team assignment and role/state packet replication unchanged.

## Regression found and fixed during this session

Introduced then corrected within the session: the team-assignment fallback was
generalized to `gm.teamNames.size() >= 2`, but `Gamemode` defaults empty
`team_names` to `{"RED","BLUE"}`, so FFA received teams (which would have
enabled friendly-fire filtering and broken FFA). Corrected to
`d.matchMode == "tdm" || d.winCondition == "last_team_standing"`. Post-fix FFA
log shows `team=-1` for all actors and normal scoring. No shipped regression.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md` (generic lifecycle, JSON-driven modes)
- `docs/architecture/player-npc-systems/player-npc-systems.md`, `ecs.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; one pre-existing mode specific
  disagreement remains (non-ffa/tdm modes outside elimination still lack a
  generic branch), recorded for later.
- `docs/skills/logging-checker-v1.md`: PASS; new diagnostics (`[MATCH RULES]`,
  `[ACTOR STATE]` on change only, `[KILL HEAL]`, `[ELIMINATION]`) are at their
  owners, transition/event based, and Duel-category (off by default).

## Human review still needed

- A GUI client should confirm spectator actors cannot fire/move and that the
  HUD/result screen reflects `Spectating` and the elimination winner.
- Live player-killer kill_heals true/false behavior needs a human client.

## Smallest logical next phase

Drive role-specific health/loadout from the existing `ActorMatchDescriptor`
profile IDs at spawn (still no movement-profile work), so the elimination slice
can be played with distinct roles rather than identical bodies.
