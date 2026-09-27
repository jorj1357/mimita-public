# Hot participant assignment + complete TDM round/match lifecycle

- EST timestamp: 2026-09-14 22:11:50 EDT (UTC 2026-09-15T02:11:50Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--match-policy-selftest` 14/14 + full
  suite 20/20)

## 1. Match ownership audit (server-side)
| Stage | Owner | State |
|---|---|---|
| mode selection | `serverCommunityStartMatch` + runtime mode registry | HOT (mode id -> descriptor -> domain) |
| match creation | `serverMatchResetEntity`, kernel match entity | KERNEL MECHANISM |
| participant assignment | hot `gamemode.tdm` (generic entity discovery) | **HOT (migrated in this pass)** |
| team/role state | `ActorTeamState`/`ActorRoleState` dynamic components | HOT (source of truth) |
| typed `matchTeams`/participants | `projectGenericActorTeams` | PROJECTION (bridge) |
| countdown/GO/ACTIVE/RESULTS/intermission | hot `gamemode.tdm` state machine | **HOT (full lifecycle, migrated)** |
| lifecycle durations + respawn | `match.lifecycle` hot handler | HOT |
| active-phase score/win | hot mode `actor.killed`/`match.evaluate` | HOT |
| respawn execution | damage/death path reads `serverMatchRespawnSeconds` | KERNEL MECHANISM |
| broadcasts | `broadcastDuelState` | KERNEL/TRANSPORT |

## 2. Participant assignment moved hot (no new slot)
- Hot `gamemode.tdm` enumerates generic actors (`ActorHealthState`), sorts by
  stable `EntityId`, and writes generic `ActorTeamState`/`ActorRoleState`
  (deterministic: team = index % 2, role = `role.soldier`). No mode enum, no
  `assignTdmTeams`.
- The hot module registers the `ActorTeamState`/`ActorRoleState` schemas so
  assignment works even when the cold assignment path never ran.
- Deliberately did **not** add a `match.assign-participants` event/slot: generic
  entity discovery + dynamic components + `match.setTeam`/`match.setPhase`
  already express assignment (live-runtime generic-bootstrap rule).

## 3. Generic team authority + typed projection
- `serverMatchActorTeam(actorId)` now reads the generic `ActorTeamState`
  component (source of truth) and falls back to the typed map.
- `serverMatchSetTeam(actorId, team)` mirrors the decision into the generic
  component and the typed map.
- `projectGenericActorTeams(d)` projects generic team state onto `matchTeams`
  and the participant roster each tick when a hot mode owns the phase, keeping
  the scoreboard/broadcast correct.

## 4. Complete hot TDM lifecycle
- Phases: countdown -> active -> results -> intermission -> next round via
  `match.setPhase`; counts start `round` at 1.
- Score limit (20) and time limit (600s) both finish the match; ties go to red;
  `match.finish` sets `victoryType` (0 score / 1 time).
- Results and intermission timers run hot; the next round increments `round`,
  resets both team scores, re-runs assignment, and returns to countdown.
- `serverMatchSetPhase` clears the previous match-over lock when a fresh round
  starts (`WAITING`/`COUNTDOWN`/`ACTIVE`/`GO`), enabling multi-round matches.

## 5. Runtime mode proof
`--match-policy-selftest` loads the DLL and confirms: `tdm` is a runtime mode
(no EXE enum); hot countdown -> active; `MatchPhaseOwnership` written;
`ActorTeamState`/`ActorRoleState` assigned and deterministic; score limit
finishes; results -> intermission -> next round (round 2, scores reset); time
limit finishes; respawn policy owned by the mode.

## 6. Cold owner classification
- `assignMatchParticipants` team/role math: **HOT for TDM** (cold remains only
  as fallback for non-migrated modes).
- `serverGamemodeTick` shared phase `switch`: **bypassable** via
  `MatchPhaseOwnership`; still fallback for non-migrated modes.
- `checkMatchWinConditions` FFA/TDM branches: bypassed when `match.evaluate`
  handled.
- Nothing proven dead was deleted.

## 7. Evidence
`--match-policy-selftest` PASS 14/14. Full suite 20/20 PASS
(`dynamic-lifecycle`, `gamemode-hot`, `hot-combat`, `dynamic-replication`,
`npc-entity`, `npc-actor-state`, `gameplay-boundary`, `match-policy`,
`capability`, `entity-slice`, `live-code`, `hot-authoritative`, `project`,
`phase456`, `telemetry`, `creation`, `movement`, `movement-parity`,
`ragdoll-slice`, `snapshot-chunk`).

## Status labels
- SELFTEST PROVEN: runtime mode registration, hot assignment (deterministic),
  full phase lifecycle, score/time-limit finish, multi-round reset,
  phase-ownership gate, generic team source-of-truth + typed projection.
- COMPILED INTEGRATION: `serverGamemodeTick` projects generic teams before the
  phase-ownership return; `serverMatchSetPhase` clears match-over on round start;
  TDM reachable from `config/onlinemodes.json` (`team_deathmatch`->`tdm`).
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game TDM multi-round phase/score/respawn parity
  and HUD.

## Honest limits
- The cold phase machine and `assignMatchParticipants` remain as fallback for
  non-migrated modes; only TDM is proven migrated.
- Objectives and round-based (objective) modes are not migrated.
- Team assignment uses generic actors via `ActorHealthState`; human/NPC
  controller distinction is not yet passed to the mode (not needed for TDM).
- `MatchPhaseOwnership`/`TdmMatchState` are generic dynamic components, not
  mode-specific flags.

## Files changed
`src/network/server-gamemode.cpp` (generic team source-of-truth + projection +
round-start match-over clear), `src/hot-reload/modules/gamemodes/tdm.cpp` (hot
assignment + full lifecycle), `src/network/match-policy-selftest.cpp` (extended
coverage), docs + this changelog.

## Next (auto-selected)
Generic objective architecture: objective = entity + components +
relationships; `objective.interact`/`objective.state-changed` events; then a
non-TDM runtime mode proof; then transform/velocity generic state.
