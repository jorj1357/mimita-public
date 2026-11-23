# Late-join team identity, hostile targeting, and team outlines

- Issue: `issue-5d82fc58a875`
- Agent: `latejoin-team-identity`
- Date: 2026-11-23 (UTC)
- Spec review: `docs/skills/spec-behavior-review-v1.md`
- Runtime workflow: `docs/workflows/runtime-scenario-validation.md`

## Problem

A human who joined a live team mode (TDM in COUNTDOWN/GO/ACTIVE) had no
authoritative team, role, participant roster entry, or team spawn. Enemy NPCs
using `npc_targeting.mode = "opposite_team"` did not treat an unknown team as
hostile, and the client had no local team identity so friendly/enemy outlines
were skipped.

## Change

- Added one live-join owner in `src/network/server-gamemode.cpp`:
  - `isLiveTeamPhase`, `modeUsesLiveTeamIdentity`, `chooseLiveJoinTeam`,
    `applyLiveJoinIdentity`.
  - Team choice is fewest humans first, then lower configured team index.
  - The joiner gets `matchTeams`, `matchActors` (team role/profiles), the
    participant roster entry, scoreboard slots, and a mirrored
    `ServerPlayer.matchTeam`.
  - The first live identity is applied before the mode spawn point is resolved
    when that join path is used.
  - If the first life was already active, one authoritative team-spawn respawn
    re-applies the team spawn group, role health/avatar, and mode loadout.
  - A team-roster mode marks the roster dirty; the next fixed tick reuses
    `reconcileTeamRosterNpc` with spawn repositioning disabled, replacing one
    roster NPC without teleporting or healing survivors.
  - `reconcileTeamRosterNpc` also counts an authoritative joiner while its
    spawn acknowledgement is in flight, so the capacity reconciliation is not
    skipped.
  - New bounded events: `match.actor.late-join-assigned`,
    `match.actor.late-join-spawn`, `gamemode.team-roster-npc.replaced`.
- Extended `assignMissingRoundModePlayers` with the live team-mode branch;
  existing Fun Counter Strike / Payload round behavior is unchanged.
- Regenerated `config/hot-reload/packet-ledger.json` for the shifted
  `server-gamemode.cpp` packet-reference line.
- Documented the contract in `docs/specs/gamemodes/teamdeathmatch.md` and
  `docs/specs/gamemodes/gamemodes.md`.

## Runtime evidence (direct observation)

Run `20261123_165703`, pinned EXE
`2026-11-23T13:26:39.822193Z`, game DLL generation 565. A second client joined
after `network.mode.start` `stage=active`.

Server journal:
`logs/2026-11-23/20261123_165703/server-01/events.jsonl`

- `match.actor.late-join-assigned` actor 2, team 1, role `elite_fighter`,
  reason `live_late_join`, participants 11.
- `gamemode.team-roster.resolved` actual/expected `[5,5]`.
- Three RED NPCs emitted `npc.target-changed` with `target_actor = 2`.
- `server.damage.applied` from RED NPC 200003 (team 0) to actor 2 (team 1),
  `killed = true`; all seven hits on the joiner came from team 0.

Client-02 journal:
`logs/2026-11-23/20261123_165703/client-02/events.jsonl`

- `client.gamemode.local-actor-identity` `found=true`, `team=1`,
  `role=elite_fighter`.
- `player_visuals.outline` `drawn` with `team_identity_known=true`,
  `teammate=true` for team-1 NPCs and `teammate=false` for team-0 NPCs.

Client-01 snapshot showed `remote_players=1`, `remote_npcs=8`.

Run `20261123_173613`, game DLL generation 567, proved
`match.actor.late-join-spawn` team 1 at `[78.526,-52.044,297.136]` for the
joiner. That generation still showed 11 world entities because the team-spawn
respawn moved the joiner to `AwaitingSpawnAck` before the roster reconcile
counted it.

Run `20261123_191110`, game DLL generation 569 (the follow-up fix), is the
final combined proof:

- Server `match.actor.late-join-assigned` actor 2, team 1,
  `role=elite_fighter`, `phase=2`.
- `match.actor.late-join-spawn` team 1 at
  `[73.377,-50.295,297.136]`.
- `gamemode.team-roster-npc.replaced` actor 200008, team 1, `human_count=1`,
  `reason=capacity_reconcile`.
- `gamemode.team-roster.resolved` actual/expected `[5,5]`, `generated_npcs=8`.
- Client-02 `network.client.snapshot.applied` `entity_count=10`,
  `remote_npcs=8`, `remote_players=1`.
- Client-02 `client.gamemode.local-actor-identity` `found=true`, `team=1`,
  `role=elite_fighter`; `player_visuals.outline` drawn with
  `team_identity_known=true`, `teammate=true` for team-1 NPCs and
  `teammate=false` for team-0 NPCs.
- Enemy NPC 200003 emitted `npc.target-changed target_actor=2` and applied
  authoritative `server.damage.applied` hits to `LateJoin` (`target_team=1`)
  from `attacker_team=0`.

The second-client join path still used the generic initial placement
`[-38.462,57.937,296.210]` before the identity existed; the single
authoritative team-spawn respawn moved the joiner to the team spawn. No manual
action is required.

## Limitations

- FFA/teamless live regression was attempted on Bloodtest launch mode 51 (run
  `20261123_172251`) but the test client never observed active mode. FFA is
  code-inspected only: the live branch returns before assignment for teamless
  modes.
- On the tested second-client path, the very first placement can still be the
  generic neutral position for roughly one second before the authoritative
  team-spawn respawn. The team identity, outlines, and NPC hostility are active
  after that respawn; this is not a teamless round.
- Human visual confirmation of outline color/placement and actual gameplay feel
  remains outstanding.
- The build was validated on a shared checkout that also contained concurrent
  `src/npc/npc.cpp` and `src/network/server-gamemode.cpp` objective-lock work by
  `npc-elite-fcs`; generation 567 includes both agents' changes. This record
  claims only the late-join hunks.

## Low-resource build collaboration

Appended direct evidence to `issue-2a5c5da53565` at
`2026-11-23T17:47:22Z`: during the 642-source game-DLL build
(`game-dll-20261123T174329519651Z-c751ad`, jobs=10, priority=high, seeded=0) the
12-logical-CPU / 15.65 GiB host had 10 `cc1plus` processes with 4.08 GiB
combined working set and 0.32 GiB free physical memory. Proposed a single
shared build-worker budget, memory-aware job selection, and recording the
resource snapshot in the attempt timing. No build-authority code was changed by
this session.
</content>
