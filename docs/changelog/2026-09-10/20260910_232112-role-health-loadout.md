// 2026-09-10T23:21:12Z
/* purpose
* record role-specific health and weapon/loadout application at authoritative spawn
* preserve exact source, config, build, unit, and headless runtime evidence
* this file does NOT claim movement profiles, personality, or spectator camera
* this file does NOT replace the append-only regression record
*/

# Role health and loadout at spawn

## Session

- Branch: `8292026stash`
- HEAD commit: `fdac4e2`
- Timestamp (UTC): `2026-09-10T23:21:12Z`
- Display timezone: America/New_York
- Display time: `2026-09-10 19:21:12 EDT`
- Pre-existing changes: role/team/state identity and the death/respawn/
  elimination slices from prior passes were preserved. This session only added
  role health + loadout resolution and spawn application.
- Unrelated concurrent edits under `docs/regressions/`, `docs/gold/`, and one
  `docs/changelog/` file were left untouched.

## Changed files

- `src/gamemode/match-roles.h/.cpp`: `MatchRoleDefinition` gains `health` and
  `startingWeapon`; parser reads `health` and `starting_weapon`.
- `config/roles.json`: hunter `health 100` / `starting_weapon revolver`;
  juggernaut `health 500` / `starting_weapon rocket_launcher`.
- `src/network/community-server-config.h/.cpp`: `CommunityWeaponSet` gains
  `key` and `roleOnly`; added `weaponSetByKey` (matches key or name);
  `weaponSetItems()` hides role-only sets from the host menu.
- `config/weaponsets.json`: added role-only sets 6 `standard` and
  7 `juggernaut` (weapon-id references only).
- `src/network/server.h`: `ServerPlayer::maxHealth` and `ServerPlayer::weaponSetId`.
- `src/network/server-gamemode.h/.cpp`: `ActorSpawnProfile` +
  `serverResolveActorSpawnProfile(actorId)` resolves role health, weapon set id,
  resolved weapon list, and starting weapon from `matchActors` + role registry +
  weapon sets; NPC gamemode spawn applies role health and `npcApplyLoadout`.
- `src/network/server-players.cpp`: `getInitialInventory(out, setId)` uses the
  effective set; `resetPlayerForSpawn` applies role health, sets
  `maxHealth`/`weaponSetId`, and rebuilds the role inventory; spawn packet sends
  the per-player set.
- `src/network/server-npcs.cpp`: `respawnServerNpc` reapplies role health and
  loadout; `[ROLE SPAWN]` diagnostic added at the gamemode spawn owner.
- `src/network/server-packet-chat.cpp`: kill-confirm heal and `healthall` use the
  actor's role-resolved `maxHealth`.
- `src/npc/npc.h`: `Npc::loadoutOverride` and `startingWeaponOverride`.
- `src/npc/npc-internal.h`, `src/npc/npc-spawn.cpp`: `npcApplyLoadout` replaces
  the NPC runtime inventory with the role weapons and equips the role starting
  weapon.
- `src/npc/npc.cpp`: NPC background reload and weapon switching use
  `loadoutOverride` when present; global `forceWeapon` is ignored while a role
  loadout is active.

## Exact spawn data flow

1. `assignMatchParticipants` stores `roleId` and `weaponProfileId` per actor.
2. At spawn/respawn, `serverResolveActorSpawnProfile(actorId)`:
   - reads `matchActors[actorId].roleId`,
   - `MatchRoleRegistry::get(roleId)`,
   - `CommunityServerConfig::weaponSetByKey(def.weaponSet)` -> set id/weapons,
   - returns `{health, weaponSetId, startingWeapon, weapons}`.
3. Player: `resetPlayerForSpawn` sets `maxHealth`/`health` (host healthall
   override wins), sets `weaponSetId` (role set, else gamemode set), rebuilds
   `ownedWeaponIds`/`weaponRuntimes` from the effective set, and the spawn packet
   carries the per-player set so the client hotbar maps the role loadout.
4. NPC: `resetGamemodeActorsAtMapSpawn` (round start) and `respawnServerNpc`
   (every life) set `body.maxHp`/`currentHp` and call `npcApplyLoadout`, which
   replaces the runtime inventory and equips the starting weapon.
5. On respawn in a respawning mode, `serverResolveActorSpawnProfile` is called
   again, so role identity/stats/loadout are reapplied. In one-life modes the
   dead actor becomes `Spectating` and no spawn path runs.

## Human vs NPC role application

- Both resolve the same `MatchRoleDefinition` via the same
  `serverResolveActorSpawnProfile` helper (no per-role C++ branches).
- Players consume the resolved `maxHealth` and effective weapon set in the
  existing inventory/spawn path; NPCs consume the same health and weapon list in
  the existing `NpcSystem` spawn/respawn path.
- Starting weapon: NPCs equip `startingWeapon` explicitly; players use the role
  weapon set's first logical slot (the existing hotbar slot 1).

## Fallback behavior

- No role, empty `roleId`, or unknown role: `hasRole=false`, no health/loadout
  override; players keep `100` max health and the gamemode community set; NPCs
  keep the global `NpcDifficultyConfig` loadout and the legacy allowed-weapon
  filter.
- Unknown `weapon_set` key: warns and falls back (no inventory override).
- A host `healthall` override (`serverGameOverrides().maxHpOverride > 0`) wins
  over role health, preserving the existing debug command.

## Tests and results

Build: `python build_agent.py` => `Status: SUCCESS` (return code 0).

Unit: `tests/match-rules-test.cpp` => `PASS: actor match rules (12 cases)`
(lifecycle transitions + kill-heal policy unchanged).

Test A - role health (elimination, 8 NPCs): server log shows
`[ROLE SPAWN] ... role=hunter hp=100 weaponSet=6` and
`... role=juggernaut hp=500 weaponSet=7`. The human path is the same resolver
and inventory code; no headless human was available.

Test B - role loadout: `[NPC ROLE LOADOUT] npc=... weapons=3 equipped=revolver`
for hunter and `weapons=2 equipped=rocket_launcher` for juggernaut. The two
sets (ids 6 and 7) contain different weapon-id lists.

Test C - respawn (TDM, respawning): 23 `[SERVER NPC RESPAWN]` events, each
followed by an `[NPC ROLE LOADOUT]` reapplication; role weapons/hp are restored
per life.

Test D - elimination (one-life): role loadout applies at round start, deaths
advance `dead -> spectating`, `spectating -> alive` count is `0`, and the match
still ends via `[PERSISTENCE] Match result emitted: mode=elimination winner=blue`.

Regression:
- FFA: `[ROLE SPAWN] ... role=none hp=100 weaponSet=0 weapons=0`; match active
  and `FFA_SCORED` present.
- TDM: starts, respawns, scores.
- Elimination: role hp/loadout plus one-life win condition.
- Role/team/state replication unchanged (packets untouched this session).

## Regression found

None. The prior session's FFA team-fallback regression remains fixed; FFA actors
show `role=none` and keep legacy behavior.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`, `docs/specs/weapons/weapons.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`: PASS; role values are data-driven,
  no hardcoded role branches.
- `docs/skills/logging-checker-v1.md`: PASS; `[ROLE SPAWN]` is once per
  gamemode spawn, `[NPC ROLE LOADOUT]` once per NPC (re)spawn, Duel/NpcCombat
  categories.

## Human review still needed

- A GUI client should confirm role max/current health and the role hotbar/weapon
  selection on a human actor (headless runs only exercised NPCs).
- Client health-bar max is still inferred from current health (no maxHealth
  field on the wire); role max display after taking damage needs checking.

## Smallest logical next phase

Apply the role movement profile (`movementPreset`) per actor at spawn using the
existing `physicsMainUpdate(..., const MovementConfig* overrideConfig)` path, so
roles differ in movement without any new movement kernel.
