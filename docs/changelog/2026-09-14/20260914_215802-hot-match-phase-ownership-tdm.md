# Hot match phase ownership + TDM mode owns lifecycle/score/win/respawn

- EST timestamp: 2026-09-14 21:58:02 EDT (UTC 2026-09-15T01:58:02Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--match-policy-selftest` 8/8 + full
  suite 18/18)

## 1. Match ownership audit (server-side)
| Stage | Owner | State |
|---|---|---|
| mode selection | `serverCommunityStartMatch` + runtime mode registry | HOT (mode id -> descriptor -> domain) |
| match creation | `serverMatchResetEntity`, kernel match entity | KERNEL MECHANISM |
| participant assignment | `assignMatchParticipants` (server-gamemode) | COLD (team/role math) |
| team/role state | `ActorTeamState`/`ActorRoleState` | HOT (written from assignment; authoritative) |
| countdown/GO/ACTIVE/RESULTS/intermission | `serverGamemodeTick` shared state machine | **COLD (now bypassable)** |
| lifecycle durations + respawn | `dispatchMatchLifecyclePolicy` + hot mode handler | HOT policy |
| active-phase score/win | hot mode `actor.killed`/`match.evaluate` | HOT |
| respawn execution | damage/death path reads `serverMatchRespawnSeconds` | KERNEL MECHANISM |
| broadcasts | `broadcastDuelState` | KERNEL/TRANSPORT |

## 2–3. Generic phase ownership + one cold owner removed
- New generic gate: a hot mode writes `MatchPhaseOwnership` on the match entity;
  `serverGamemodeTick` then **skips the cold phase state-machine transitions**
  (after pending broadcast) and returns. No mode name is consulted.
- Hot `gamemode.tdm.cpp`: mode descriptor `tdm`, schema `TdmMatchState`, system
  on `mode.tdm`:
  - owns the countdown -> active transition via `match.setPhase`;
  - writes `MatchPhaseOwnership`;
  - handles `actor.killed` to update team score from `match.actorTeamRead`;
  - finishes the match at the score limit via `match.finish`;
  - handles `match.evaluate` (win) and `match.lifecycle` (respawn policy).

## 4. Team/role
`ActorTeamState`/`ActorRoleState` remain authoritative; assignment math is still
cold (`assignMatchParticipants`) and is the next team/role owner to move.

## 5. Respawn policy
Hot mode (`match.lifecycle`) owns the respawn decision; the kernel applies the
out-fields in the shared machine and the damage/death path reads the resulting
authoritative value.

## 6. Real mode end-to-end
TDM: hot mode owns phase transition, team scoring, win/end, and respawn policy;
the cold TDM score/win branches are bypassed via `match.evaluate` and the cold
phase transitions via `MatchPhaseOwnership`.

## 7. Objective
Not done (documented next): objectives as entities/components/relationships.

## 9. Replication
Match state is generic dynamic components (`TdmMatchState`,
`MatchPhaseOwnership`) on the match entity; replication uses the generic
component path. No mode-specific packet; HUD unaffected (other agent).

## 10. Runtime mode proof
`--match-policy-selftest` loads the DLL and confirms `tdm` is a runtime mode
(no EXE enum), owns phase transitions, scores from `actor.killed`, finishes via
`match.finish`, and owns respawn policy.

## 11. Cold owner classification
- `serverGamemodeTick` shared phase `switch`: **bypassable (B)** via
  `MatchPhaseOwnership`; still fallback for non-migrated modes.
- `assignMatchParticipants` team/role math: **cold (next)**.
- `checkMatchWinConditions` FFA/TDM branches: **bypassed (B)** when
  `match.evaluate` handled.
- `dispatchMatchLifecyclePolicy`: kernel mechanism + hot policy.
- Nothing proven dead was deleted.

## 12. Evidence
`--match-policy-selftest` PASS 8/8: hot package active; runtime TDM mode
registered (no EXE enum); match entity; hot mode claimed generic phase ownership;
countdown -> active via `match.setPhase`; score limit finished via `match.finish`;
winning team selected; hot mode owns respawn policy. Full suite PASS 18/18.

## Status labels
- SELFTEST PROVEN: runtime mode registration, hot phase transition, kill->team
  score, score-limit finish, respawn-policy ownership, phase-ownership gate.
- COMPILED INTEGRATION: `serverGamemodeTick` skips cold transitions when
  `MatchPhaseOwnership` is set; `match.lifecycle` out-fields applied by the
  kernel; TDM reachable from `config/onlinemodes.json` (`team_deathmatch`->`tdm`).
- LIVE MULTIPLAYER PROVEN: no.
- HUMAN VERIFICATION NEEDED: in-game TDM phase/score/respawn parity and HUD.

## Honest limits
- Results/intermission/time-limit transitions are not yet hot-owned for TDM
  (only countdown->active); the cold machine remains fallback.
- Team/role assignment math is still cold; `objective`/round modes not migrated.
- `MatchPhaseOwnership` is a generic component, not a mode-specific flag.

## Files changed
`src/network/server-gamemode.cpp` (phase-ownership gate),
`src/hot-reload/modules/gamemodes/tdm.cpp` (new),
`src/network/match-policy-selftest.{h,cpp}` (new), `src/game/game-cli.cpp`;
docs + this changelog.

## Next (auto-selected)
Move team/role assignment math hot (generic assignment capabilities) and finish
TDM results/intermission; then objective/round state via entities/components;
then transform/velocity generic state.
