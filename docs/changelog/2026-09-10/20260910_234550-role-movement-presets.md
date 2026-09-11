// 2026-09-10T23:45:50Z
/* purpose
* record per-actor role movement presets applied through the shared movement kernel
* preserve exact source, config, build, and headless runtime evidence
* this file does NOT claim new pathfinding, personality, or spectator camera
* this file does NOT replace the append-only regression record
*/

# Role movement presets through the shared kernel

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-10T23:45:50Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 19:45:50 EDT`
- Pre-existing changes: role/team/state identity, death/respawn/elimination,
  and role health/loadout slices were preserved. This session only added
  per-actor role movement resolution and kernel use.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and one
  `docs/changelog/` file were left untouched.

## Changed files

- `src/gamemode/match-roles.h/.cpp`: new `RoleMovementCache` (see below).
- `config/movement/movement-heavy.json` (new): `name: heavy`.
- `config/movement/movement-retrograd-fast.json` (new): `name: retrograd_fast`.
- `src/network/server-gamemode.h/.cpp`: `ActorSpawnProfile::movementPreset`;
  `serverResolveActorSpawnProfile` resolves/validates the role preset and logs
  `[ROLE MOVEMENT]`; NPC gamemode spawn sets `npc.movementProfileId`.
- `src/network/server.h`: `ServerPlayer::movementProfileId`.
- `src/npc/npc.h`: `Npc::movementProfileId`.
- `src/network/server-players.cpp`: `resetPlayerForSpawn` sets the effective
  role movement id; the authoritative player movement replay uses the role
  `MovementConfig` via `applyRuntimeMovementTuning`.
- `src/npc/npc.cpp`: the NPC kernel call prefers the role `MovementConfig`.
- `src/network/server-npcs.cpp`: `respawnServerNpc` reapplies the role movement
  id; added `spd=` to the existing per-second NPC fire-state summary.
- `src/engine/engine-tick-setup.cpp`, `src/network/server.cpp`: poll
  `RoleMovementCache::pollReload()` next to the movement config hot reload.

## Exact movement-profile data flow

1. `assignMatchParticipants` stores `roleId`; role health/loadout already flow
   through `serverResolveActorSpawnProfile`.
2. `serverResolveActorSpawnProfile(actorId)` now also reads
   `MatchRoleDefinition::movementPreset`, validates it through
   `RoleMovementCache::get`, sets `out.movementPreset`, and logs
   `[ROLE MOVEMENT] actor=... role=... preset=... groundSpeed=... dash=...`.
3. At spawn/respawn the resolved preset id is stored on the actor:
   `ServerPlayer::movementProfileId` (human) and `Npc::movementProfileId`.
4. Simulation resolves the cached config at movement time:
   - Human: `server-players.cpp` `simulatePlayer` does
     `roleMove = RoleMovementCache::get(p.movementProfileId)` then
     `cfg = roleMove ? applyRuntimeMovementTuning(*roleMove)
                     : makeCurrentRuntimeMovementConfig()`.
   - NPC: `npc.cpp` `updateOneNpc` does
     `roleMove = RoleMovementCache::get(npc.movementProfileId)` then
     `npcMove = roleMove ? roleMove : NpcDifficultyConfig::npcMovementConfig()`
     and passes it to `physicsMainUpdate(npc.body, ..., npcMove)`.
5. Both paths reach the same `physicsMainUpdate` / `movement-step.cpp` kernel;
   only the chosen `MovementConfig` and the input source differ.

## Cache / resolution design

- `RoleMovementCache` (in `match-roles`, the role owner) maps preset name ->
  `{valid, MovementConfig, path, writeTime}`.
- First use calls the existing `MovementJsonConfig::loadPresetInto(name, cfg,
  &path)` and caches the parsed config and file path. No JSON is parsed per
  simulation tick.
- Lifetime: process lifetime. Entries are updated in place and never erased, so
  the returned `const MovementConfig*` stays valid.
- Misses are cached too (`valid=false`) so an unknown preset does not retry or
  warn every tick. `serverResolveActorSpawnProfile` catches the miss once at
  spawn, warns with role context, and leaves `movementPreset` empty.
- No second movement configuration system was added; the cache only wraps the
  existing loader and `MovementConfig`.

## Human vs NPC use

- Humans and NPCs resolve the same `MatchRoleDefinition::movementPreset`
  through the same `serverResolveActorSpawnProfile` + `RoleMovementCache` path.
- The physics kernel does not branch on actor type; only input generation
  differs (client input commands vs NPC `buildInputState`).
- The human path uses `applyRuntimeMovementTuning` because the server kernel
  call takes a `MovementConfig` value; the NPC path passes the cache pointer
  directly to `physicsMainUpdate`, which applies the same runtime tuning
  internally.

## Fallback behavior

- Empty `roleId` / empty `movementPreset`: `movementProfileId` stays empty,
  `RoleMovementCache::get("")` returns nullptr, and movement uses the existing
  global config (human) or `NpcDifficultyConfig::npcMovementConfig()` (NPC).
- Unknown preset: warned once at spawn; role movement falls back to the same
  legacy/default path. FFA/TDM without roles are unchanged.

## Hot reload behavior

- `RoleMovementCache::pollReload()` compares each cached preset file's mtime and
  reloads it in place when changed; it keeps the last valid config if a parse
  fails. It is wired next to `MovementJsonConfig::pollReload()` in the client
  hot-reload list (`engine-tick-setup.cpp`) and both server loops
  (`server.cpp`).
- Live verification: with a running dedicated server, editing
  `config/movement/movement-heavy.json` `ground_speed` logged
  `[ROLE MOVEMENT] reloaded preset 'heavy' from config/movement\movement-heavy.json`.
  The test value was restored to `13.0` afterward.

## Movement capabilities

- No new capability system was needed. `MovementConfig` already expresses the
  required distinctions and `applyPresetOverrides` already parses
  `dash_enabled`, `down_dash_enabled`, and `freeze_enabled` in addition to
  speed, acceleration, jump, gravity, air control, and bhop fields.
- `retrograd_fast` enables dash/down-dash/freeze; `heavy` disables them and
  lowers speed/acceleration/jump. No role-name branching exists.

## Tests and results

Build: `python build_agent.py` => `Status: SUCCESS` (return code 0).

Test A - distinct movement (elimination, 8 NPCs), Duel category:
- `[ROLE MOVEMENT] actor=1000 role=hunter preset=retrograd_fast
  groundSpeed=24.0 airSpeed=24.0 jump=19.0 gravity=-58.0 dash=1 downDash=1
  freeze=1`
- `[ROLE MOVEMENT] actor=1001 role=juggernaut preset=heavy groundSpeed=13.0
  airSpeed=13.0 jump=13.0 gravity=-70.0 dash=0 downDash=0 freeze=0`

Test B - measurable difference: the resolved configs differ in ground/air
speed (24 vs 13), jump (19 vs 13), gravity (-58 vs -70), and dash/down-dash/
freeze (enabled vs disabled).

Test C - NPC usage (TDM, 8 NPCs, NpcCombat `spd=` samples): average of each
actor's maximum planar speed was `retrograd_fast: 66.5` vs `heavy: 43.3`
(hunters dash, juggernauts cannot), confirming role configs drive NPC movement.

Test D - human path: `resetPlayerForSpawn` stores the role movement id and
`simulatePlayer` resolves the role config through the same cache before the
shared kernel phases. No headless human client was available, so live-human
runtime validation is pending; the human path is the same resolver/kernel as
the verified NPC path.

Test E - respawn (TDM): 47 NPC respawns; `serverResolveActorSpawnProfile` logs
(including `[ROLE MOVEMENT]`) recur after respawn, so role movement identity
survives `Alive -> Dead -> Respawning -> Alive`.

Regression:
- FFA: `role_movement_logs=0`, `role=none hp=100 weaponSet=0`, match active and
  scoring.
- TDM: starts, respawns, scores.
- Elimination: role movement + role health/loadout both apply; `dead ->
  spectating`, `spectating -> alive = 0`, match ends
  (`[PERSISTENCE] Match result emitted: mode=elimination winner=blue`).
- Role health/loadout and actor-state replication unchanged.

## Regressions found

None.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/movement/movement.md` (shared kernel + config contract)
- `docs/specs/gamemodes/gamemodes.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; role->preset->config->physics
  with no role-name branches and no global movement mutation.
- `docs/skills/efficiency-checker-v1.md`: PASS; no per-tick JSON parsing
  (cached), no new allocations in the movement hot path.
- `docs/skills/logging-checker-v1.md`: PASS; `[ROLE MOVEMENT]` is once per
  spawn/respawn and on preset reload, Duel category.

## Human review still needed

- A GUI client should confirm the human actor's role movement preset changes
  movement feel on the authoritative path (live-human runtime pending).
- Confirm remote clients observe the resulting authoritative positions/speeds
  for role-moved actors.

## Smallest logical next phase

Wire the existing replicated `ActorMatchDescriptor` role into a minimal
role/team HUD readout (role name, team, living counts) using the already
transmitted `DuelStatePacket` role/state fields, so the role systems built so
far are visible in-game without adding new gameplay.
