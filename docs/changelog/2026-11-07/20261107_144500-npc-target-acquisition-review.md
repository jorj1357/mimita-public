# NPC target acquisition review

- Timestamp: 2026-11-07 14:44:45 EST (2026-11-07T19:44:45Z)
- Branch: `20261011zt`
- HEAD: `64d28fe1ca2b4c374e1ed11e0c6ae748e285b463`
- Result: `PASS_WITH_HUMAN_REVIEW` — investigation only; no source/configuration changes.

## Finding

The visible game and live server evidence do not line up cleanly. The game window showed a connected Counter-Strike-style match, but dev-loop run `20261107_193229` recorded the server launch as `infinite_cramped / connected_world`; the managed mode-start request timed out, and the client was marked stale with its last snapshot about 12 seconds old. The current server journal's NPC `1000` (`nervousbad`, team 0) repeatedly reports `server_target_id=0`, `candidate_valid=false`, `belief_has_target=false`, and `target_visible=false`. At those sampled times, the immediate reason it cannot attack is that the server has not supplied it a valid live hostile target. The journal's run state shows only one roster member, consistent with the client/server session being out of sync; the displayed client's “Connected” label does not prove current server snapshots are arriving.

An older segment in the same client journal belongs to a different FCS session (`fun_cs_rush`, `funworld3`). There, NPCs had valid assigned target IDs, but sampled targets were commonly 160–390 m away and line-of-sight blocked. One sample had clear LOS but remained invisible, consistent with the separate range or horizontal-FOV gates. This explains why they can behave differently across situations, but it is not clean evidence for the currently displayed match.

## Code path and specification

- `src/network/server-npcs.cpp`, `simulateSharedNpcs`: selects hostile targets using the active mode's NPC targeting policy, candidate inclusion, team hostility, and alive state. No selected actor means no live target is mirrored into perception/combat.
- `src/npc/npc.cpp`, `senseWorld`: perception requires a valid living candidate plus configured horizontal FOV, sight range, and line of sight (`npc-perception.cpp`, `perceive`). Smoke and world geometry can block LOS.
- `src/npc/npc.cpp`, combat decision in `updateOneNpc`: only a visible legal target requests attack; reaction timer and attack cooldown must also be clear.
- `src/npc/npc-combat.cpp`, `tryFire`: rejects a dead target, cooldown, missing weapon/ammo, reload, or blocked LOS; it then uses ordinary aim and weapon rules.
- `docs/specs/npc-behavior/npc-behavior.md`, sections 8–9: a visible legal target should trigger an attack request, while persistent pursuit continues after LOS is lost. The spec does not promise firing through cover.

## Evidence and limits

- Read-only live inspection: `python devscripts/dev-loop.py --status`, `run.json`, the server journal, and the active game window.
- Active executable build identity: `2026-11-07T18:53:36.293589Z:68d89e871a70589d7588dcd2895cdd486171122d5b6bd15ea2bbcd43d9ed6e01`; server PID `26928`; client `stale` in the run record.
- Representative server event: `npc.target-sense`, actor `1000`, event sequence `21878`, tick `42560`, reason `no_visible_target`; fields show no target ID and invalid candidate.
- Earlier client samples include `npc.target-sense` events with `candidate_valid=true`, `cached_los_blocked=true`, and `target_visible=false`; these are from the other profile/session context above.
- No build, test, or gameplay intervention was performed. The current snapshot is enough to identify the first observed failure (no valid target reaching perception), but not enough to determine why the displayed client's server-side actor is absent/stale or whether target/team selection is wrong in the intended live mode. Human gameplay acceptance remains open.

## Repository state

Pre-existing dirty/untracked work was observed and left untouched, including edits in config, docs, `src/gamemode/map-config.h`, `devscripts/blender-ai`, and two earlier changelogs. This file is the only file created by this investigation.

## Documents reviewed

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/dev-loop/dev-loop.md`, `docs/specs/dev-loop/quality-gates.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/README.md`
- `docs/skills/spec-behavior-review-v1.md`

## Review checklist

`docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. Owner path, expected gates, live evidence, and context mismatch are recorded. No spec/code disagreement was established from the current sample. Further diagnosis needs a fresh synchronized server/client sample while the user reproduces one ignored and one attacking moment in the same mode.

## Follow-up — explicit leave/rejoin and host identity

The user clarified that this occurs after an intentional Leave and rejoin, and
that their original host privileges are also missing after return.

- Confirmed source path: `handleDisconnect()` in `src/network/server-packets.cpp`
  erases the player record for an explicit leave. `assignHostFlag()` in that
  file assigns the host only when `gServerHostPlayerId == 0`; that global is
  reset only at server-process startup (`src/network/server.cpp`, `runServer`).
  A fresh join gets a new player ID, so it cannot inherit the old host flag or
  admin state. This matches the missing-admin report. The current admin spec
  says the first accepted human session owns host access and that session
  records own grants; it does not define how the same person proves host
  continuity after an intentional leave.
- FCS identity path: `assignMissingRoundModePlayers()` in
  `src/network/server-gamemode.cpp` repairs active players lacking a valid team
  and role, and `broadcastDuelState()` calls it before serializing round
  identity. `simulateSharedNpcs()` in `src/network/server-npcs.cpp` rejects a
  player as hostile in `opposite_team` mode when either team is unknown. The
  server tick runs NPC simulation before `serverGamemodeTick()`, so a newly
  active player can be ignored for an initial tick before the round identity
  repair runs. A continuing failure after the next identity publication is
  not proven; current FCS journal rows are from a different/stale session.
- Explicit leave clears the client's reconnect token in
  `teardownPreviousSession()` (`src/network/multiplayer-packets.cpp`), while
  the server deletes the player record. Reusing the reconnect token would
  require a deliberate same-server resume flow and a server-side reservation;
  promoting whichever player joins next would transfer admin authority to an
  unverified person, so that is not a safe fix.
- The server-session performance sample is not representative of the visible
  game: its manifest has zero players and one NPC. Its recent 60-tick windows
  show `npc_avg_ms` about 0.23–1.18 ms and no >100 ms stalls, but this cannot
  rule out the reported client slowdown. Client frame slices show about
  776.5 MB working set in sampled windows and variable allocation volume; the
  samples are not a clean same-session time series. Reconnect probes show the
  avatar upload queue at 689,228 bytes before teardown and zero after teardown,
  which proves the queue is cleared at teardown, not that it grows over time.

No source/configuration behavior was changed. The dev-loop was observed in an
active build with a stale client and mismatched mode identity; it was left
untouched. A safe host-continuity change needs to bind a rejoin to the original
server-session owner (for example, an explicit capability-resume flow), and
runtime verification needs a fresh run of the same server mode with an
intentional leave/rejoin and an increasing-time performance sample.
