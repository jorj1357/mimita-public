// 2026-09-10T22:27:23Z
/* purpose
* record the authoritative match-actor identity foundation (team/role/state/controller/profile IDs)
* preserve the exact source, configuration, build, and runtime evidence
* distinguish this session's additions from the pre-existing on-disk foundation work
* this file does NOT claim GUI/client acceptance
* this file does NOT claim advanced NPC AI, spectator, or elimination behavior
* this file does NOT replace the append-only regression record
*/

# Match-actor identity foundation

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-10T22:27:23Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 18:27:23 EDT`
- Pre-existing changes: a prior pass had already added the foundation on disk.
  Those files and edits are preserved and are attributed to that earlier work,
  not to this session. This session only added the server config load/reload
  path, fixed the role apportionment, and added the per-actor diagnostic.
- Unrelated concurrent edits: `docs/regressions/regressions-v1.md`,
  `docs/changelog/2026-09-10/20260910_222048-ragdoll-regression-model-record.md`,
  and `docs/gold/2026-09-10-ragdoll-model-power-lesson.md` appeared during the
  session and are not part of this task. They were left untouched.

## Pre-existing foundation (not authored this session)

- `src/network/actor-match.h`: `ActorController {Human, Npc}`, `ActorState {Alive, Dead, Respawning, Spectating}`, `ActorMatchDescriptor {controller, state, teamId, roleId, movementProfileId, weaponProfileId, behaviorProfileId}`.
- `src/gamemode/match-roles.h` / `.cpp`: `MatchRoleDefinition` and `MatchRoleRegistry` loading `config/roles.json`, with stable 1-based wire indices.
- `config/roles.json`: `hunter` (team 0, retrograd_fast/standard/hunter) and `juggernaut` (team 1, heavy/juggernaut/aggressive).
- `src/terminal/actor-commands.h` / `.cpp`: `actorlist` diagnostic.
- `src/gamemode/gamemode.h` / `.cpp`: optional `roleCounts` parsed from a gamemode `roles` object.
- `src/network/server-gamemode.h`: `ServerGamemodeState::matchActors`.
- `src/network/server-gamemode.cpp`: initial `assignMatchParticipants` descriptor seeding plus `updateActorStates` and `broadcastDuelState` role/state fields.
- `src/network/packets.h`: `DuelStatePacket::participantRoles[32]`, `participantStates[32]`.
- `src/network/community-match-client.h` / `.cpp`: replicated actor identity storage.
- `src/engine/engine-tick-setup.cpp`, `src/main-systems.cpp`: client-side role registry load + hot reload + command registration.
- `config/gamemodes/retrograd.json`, `config/gamemodes/tdm.json`: `roles` count blocks.

## Changes this session

### 1. Authoritative server loads and hot-reloads roles

File: `src/network/server.cpp`

- Added `#include "gamemode/match-roles.h"`.
- In `runServer()` (server config load, next to `GamemodeRegistry::loadDirectory`):
  `MatchRoleRegistry::instance().load("config/roles.json");`
- In the dedicated server loop (after `MovementJsonConfig::instance().pollReload()`):
  `MatchRoleRegistry::instance().pollReload();`
- In `simulateOneServerTick()` (after `SpawnVelocityConfig::instance().pollReload()`):
  `MatchRoleRegistry::instance().pollReload();`

Reason: without this, a dedicated/listen server had no roles loaded and
`assignMatchParticipants` would have skipped every role and warned `unknown role`.

### 2. Role apportionment is proportional, not greedy-majority

File: `src/network/server-gamemode.cpp`, `assignMatchParticipants()`

- Old: repeatedly dealt the role with the most remaining slots. With few
  participants this clumped the majority role (e.g. 4 players all `hunter`).
- New: largest-remainder style apportionment. For participant position `i`,
  choose the role maximizing `cap * (i+1) / totalSlots - assigned`. This keeps
  two roles visible in a small match (e.g. 10 participants with 8/4 becomes
  7 hunter + 3 juggernaut) and remains deterministic.

### 3. One-shot per-actor identity diagnostic

File: `src/network/server-gamemode.cpp`, end of `assignMatchParticipants()`

Added a `Debug::Category::Duel` dump that prints one `[MATCH ACTOR]` line per
participant (`id/controller/role/team/state/movement/weapon/behavior`). It runs
once per assignment (not per tick), so it is not spam and it makes the
foundation testable on a headless server without the GUI terminal.

## Data flow

- Human: `players` map -> `assignMatchParticipants` seeds
  `controller=Human`, deals a role from `Gamemode::roleCounts` +
  `MatchRoleRegistry`, assigns `def->team` (or TDM round-robin), writes
  `Player::matchTeam`, stores one `ActorMatchDescriptor` in `matchActors`.
- NPC: identical path; `controller=Npc` because the id is found in the `npcs`
  map, and the role's `behaviorProfile` is also copied. `ServerNpc::matchTeam`
  is written the same way.
- Replication: `broadcastDuelState` maps `roleId -> MatchRoleRegistry::indexOf`
  and `state` into `participantRoles` / `participantStates`; the client's
  `CommunityMatchClient::onState` stores them in `mActors`.
- Live state: `updateActorStates` runs every `serverGamemodeTick` and derives
  `Alive/Dead/Respawning` from `ServerPlayer`; NPCs derive from `health`.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md` (general, data-driven modes; generic names)
- `docs/architecture/player-npc-systems/player-npc-systems.md` (shared actor paths)
- `docs/architecture/ecs-entity-etc/ecs.md` (transitional actor containers)
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS. One pre-existing
  spec-code disagreement recorded below; not introduced or fixed this pass.
- `docs/skills/logging-checker-v1.md`: PASS. New diagnostic is at the assignment
  owner, once per match, categorized `Duel` (off by default), and includes the
  decision inputs.
- `docs/skills/terminal-command-checker-v1.md`: PASS. `actorlist` is registered
  once in `registerActorCommands`, has a name/description/usage, and reads
  server state or replicated client state. GUI/terminal invocation still needs
  human confirmation.

## Findings

## Finding

- Severity: medium
- Type: spec-code disagreement (pre-existing, not introduced here)
- Specification: `docs/specs/gamemodes/gamemodes.md` requires generic,
  mode-agnostic runtime paths and rejects per-mode `else if`.
- Exact quoted requirement: "the code should not do hardcoded things ... it
  should work based on the id in the json ... also, somehow to change from
  bombtagmanager, this is too specific, make it like gamemodemanager".
- Code path: `server-gamemode.cpp` scoring/lifecycle branches still use
  `d.matchMode == "ffa"` / `"tdm"` / `"duel"` / `"bombtag"`, and modes other
  than those (e.g. `retrograd`) do not enter a match state machine, so their
  `roles` block is parsed but not assigned.
- Actual behavior: identity/role assignment is data-driven, but the surrounding
  lifecycle is still mode-string branched.
- Expected behavior: one generic lifecycle driven by JSON rules.
- Evidence: this session's headless TDM/FFA runs reached assignment; a
  `retrograd` run would not reach `assignMatchParticipants`.
- Recommended wording or implementation action: later phase should route
  non-ffa/tdm modes through the shared lifecycle before Retrograd roles are used.
- Human decision required: no; recorded for the later gamemode-generalization phase.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS` (116 compiled), return
  code 0. A second incremental build after the diagnostic edit -> `BUILD SUCCESS`.
- Runtime (authoritative dedicated server, roles loaded):
  `mimita.exe --server --bind 127.0.0.1:0 --mode team_deathmatch --npcs 10 --timeout 40 --no-discord-notification --no-map-rotation`
  Server log (`logs/09-10-2026/Server_log_182529.txt`) with `duel` logging
  temporarily verbose showed:
  - `[ROLES] Loaded 2 role(s) from roles.json`
  - `[MATCH ACTORS] assigned=10 mode=tdm roleCounts=2`
  - 7 actors `role=hunter team=0 ... movement=retrograd_fast weapon=standard behavior=hunter`
  - 3 actors `role=juggernaut team=1 ... movement=heavy weapon=juggernaut behavior=aggressive`
  - `[ServerMatch] countdown started mode=tdm ... participants=10`
- Runtime (FFA regression): `--mode free_for_all --npcs 6` reached
  `[FFA/TDM] Match ACTIVE mode=ffa` with `[MATCH ACTORS] assigned=6 mode=ffa
  roleCounts=0` and all actors `role=none team=-1`; no `unknown role` warnings.
- `config/debuglogger.json` was temporarily set `duel: verbose` only to capture
  the Duel-category evidence and was restored with `git checkout` (verified back
  to `"off"`).
- `git diff --check`: clean (exit 0).

## Human review still needed

- GUI path: open MiMITA -> Play -> create/select a server with TDM, 1 human +
  NPCs, run `actorlist`, and confirm the printed role/team/state matches.
- Client replication: confirm a joining client sees team/role/state (the code
  path is built but was not exercised by a second client here).
- A real human participant was not available in the headless runs; the
  `controller=human` branch is the same assignment code but is unobserved.
- `DuelStatePacket` gained 64 bytes; mixed old/new client builds were not tested
  and are not supported.

## Remaining assumptions

- Role `movement_preset` / `weapon_set` / `behavior_profile` values are stored
  as IDs only. The referenced preset files (`retrograd_fast`, `heavy`,
  `standard`, `juggernaut`, `hunter`, `aggressive`) do not exist yet and are not
  resolved or applied in this pass, by design.
- `ActorState::Spectating` is defined and replicated but nothing produces it
  yet (no spectator/elimination behavior in this pass).
- `config/roles.json` indices are the wire contract; reordering roles without a
  client rebuild would remap indices.

## Smallest logical Phase 2

Make the server honor one already-parsed rule for a real observable effect:
gate death/respawn on `Gamemode::respawnSeconds` and `killHeals`, and add an
`ActorState::Spectating` transition for a zero-respawn match. This uses the
identity/state foundation directly and needs no new packet fields.
